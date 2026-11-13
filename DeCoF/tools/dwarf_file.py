"""Declarations of one source file of a Linux GoldSrc library, from DWARF.

  python tools/dwarf_file.py <lib.so> <file suffix> [--types]

For the compile unit whose name ends with <file suffix> (for example
engine/cvar.c), prints in declaration order its global variables and its
functions with return type, parameters and locals (names and types as the
original source declared them), each with its declaration line. --types
also prints the members (offset, name, type) of every struct, union and
enum the unit uses. Names come from the Linux build of about 2019: check
every layout against the 2013 Windows binary before using it.
"""
import sys

from elftools.elf.elffile import ELFFile


def member_offset(loc):
    """DW_AT_data_member_location: a constant, or an expression
    DW_OP_plus_uconst <ULEB128> (0x23) as GCC writes it."""
    if isinstance(loc, int):
        return loc
    if isinstance(loc, list) and loc and loc[0] == 0x23:
        v, shift = 0, 0
        for b in loc[1:]:
            v |= (b & 0x7F) << shift
            shift += 7
            if not b & 0x80:
                break
        return v
    return None


def attr(die, name, default=None):
    a = die.attributes.get(name)
    if a is None:
        # out-of-line copies of inlined functions keep their names in the
        # abstract instance (DW_AT_abstract_origin) or the declaration
        for link in ("DW_AT_abstract_origin", "DW_AT_specification"):
            if link in die.attributes and name not in ("DW_AT_low_pc", "DW_AT_location"):
                return attr(die.get_DIE_from_attribute(link), name, default)
        return default
    v = a.value
    return v.decode("latin-1") if isinstance(v, bytes) else v


class Types:
    def __init__(self, cu):
        self.cu = cu
        self.used = {}

    def die(self, die, name="DW_AT_type"):
        if name not in die.attributes:
            for link in ("DW_AT_abstract_origin", "DW_AT_specification"):
                if link in die.attributes:
                    return self.die(die.get_DIE_from_attribute(link), name)
            return None
        return die.get_DIE_from_attribute(name)

    def name(self, die):
        t = self.die(die)
        return self.spell(t)

    def spell(self, t, depth=0):
        if t is None:
            return "void"
        if depth > 12:
            return "?"
        tag = t.tag
        n = attr(t, "DW_AT_name")
        if tag == "DW_TAG_base_type":
            return n
        if tag == "DW_TAG_typedef":
            return n
        if tag in ("DW_TAG_structure_type", "DW_TAG_union_type", "DW_TAG_enumeration_type", "DW_TAG_class_type"):
            key = n or "<anon %x>" % t.offset
            self.used.setdefault(key, t)
            kw = {"DW_TAG_structure_type": "struct", "DW_TAG_union_type": "union",
                  "DW_TAG_enumeration_type": "enum", "DW_TAG_class_type": "class"}[tag]
            return "%s %s" % (kw, key)
        if tag == "DW_TAG_pointer_type":
            return self.spell(self.die(t), depth + 1) + " *"
        if tag == "DW_TAG_reference_type":
            return self.spell(self.die(t), depth + 1) + " &"
        if tag == "DW_TAG_const_type":
            return "const " + self.spell(self.die(t), depth + 1)
        if tag == "DW_TAG_volatile_type":
            return "volatile " + self.spell(self.die(t), depth + 1)
        if tag == "DW_TAG_array_type":
            dims = ""
            for c in t.iter_children():
                if c.tag == "DW_TAG_subrange_type":
                    ub = attr(c, "DW_AT_upper_bound")
                    dims += "[%s]" % (ub + 1 if isinstance(ub, int) else "")
            return self.spell(self.die(t), depth + 1) + dims
        if tag == "DW_TAG_subroutine_type":
            ps = [self.spell(self.die(c), depth + 1) for c in t.iter_children() if c.tag == "DW_TAG_formal_parameter"]
            return "%s (*)(%s)" % (self.spell(self.die(t), depth + 1), ", ".join(ps))
        return n or tag


def locals_of(die, types, out, depth=1):
    for c in die.iter_children():
        if c.tag == "DW_TAG_variable":
            out.append("%s%s %s;  // line %s" % ("  " * depth, types.name(c), attr(c, "DW_AT_name"),
                                                 attr(c, "DW_AT_decl_line", "")))
        elif c.tag == "DW_TAG_lexical_block":
            out.append("%s{" % ("  " * depth))
            locals_of(c, types, out, depth + 1)
            out.append("%s}" % ("  " * depth))


def main():
    lib, suffix = sys.argv[1], sys.argv[2].replace("\\", "/").lower()
    show_types = "--types" in sys.argv
    dwarf = ELFFile(open(lib, "rb")).get_dwarf_info()
    for cu in dwarf.iter_CUs():
        top = cu.get_top_DIE()
        if not (attr(top, "DW_AT_name") or "").replace("\\", "/").lower().endswith(suffix):
            continue
        types = Types(cu)
        print("// compile unit %s" % attr(top, "DW_AT_name"))
        for die in top.iter_children():
            if die.tag == "DW_TAG_variable" and "DW_AT_location" in die.attributes:
                ext = "" if attr(die, "DW_AT_external") else "static "
                print("%s%s %s;  // line %s" % (ext, types.name(die), attr(die, "DW_AT_name"), attr(die, "DW_AT_decl_line", "")))
            elif die.tag == "DW_TAG_subprogram" and "DW_AT_low_pc" in die.attributes:
                params = ["%s %s" % (types.name(c), attr(c, "DW_AT_name", "")) for c in die.iter_children()
                          if c.tag == "DW_TAG_formal_parameter"]
                ext = "" if attr(die, "DW_AT_external") else "static "
                print("\n%s%s %s( %s )  // line %s" % (ext, types.name(die), attr(die, "DW_AT_name"),
                                                     ", ".join(params) or "void", attr(die, "DW_AT_decl_line", "")))
                out = []
                locals_of(die, types, out)
                print("\n".join(out))
        if show_types:
            print("\n// types")
            for key, t in sorted(types.used.items()):
                print("%s %s  // size %s" % (t.tag.replace("DW_TAG_", ""), key, attr(t, "DW_AT_byte_size")))
                for c in t.iter_children():
                    if c.tag == "DW_TAG_member":
                        loc = member_offset(attr(c, "DW_AT_data_member_location"))
                        print("  +%s %s %s" % (loc, types.name(c), attr(c, "DW_AT_name")))
                    elif c.tag == "DW_TAG_enumerator":
                        print("  %s = %s" % (attr(c, "DW_AT_name"), attr(c, "DW_AT_const_value")))
        return
    sys.exit("no compile unit ends with %s" % suffix)


if __name__ == "__main__":
    main()
