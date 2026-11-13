"""Rebuild the declarations of a Linux GoldSrc library's header files from DWARF.

  python tools/dwarf_headers.py <lib.so> <out dir> [--only <path part> ...]

Every DWARF declaration records the file and line it comes from
(DW_AT_decl_file, DW_AT_decl_line). Collected over all compile units, this
gives for each original header the structs, unions, enums and typedefs it
defined, the functions it declared and its extern variables, in line order.
Writes one file per header under <out dir> (same relative path, for example
engine/cvar.h), each declaration preceded by its original line number.
The output is reference material: the Linux build is about six years newer
than the 2013 Windows build, so every layout must be checked in the binary
before a header in src/ uses it. C++ classes are written with their data
members and the names of their methods only.
"""
import collections
import os
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
        for link in ("DW_AT_abstract_origin", "DW_AT_specification"):
            if link in die.attributes:
                return attr(die.get_DIE_from_attribute(link), name, default)
        return default
    v = a.value
    return v.decode("latin-1") if isinstance(v, bytes) else v


def type_die(die):
    if "DW_AT_type" not in die.attributes:
        for link in ("DW_AT_abstract_origin", "DW_AT_specification"):
            if link in die.attributes:
                return type_die(die.get_DIE_from_attribute(link))
        return None
    return die.get_DIE_from_attribute("DW_AT_type")


TAGWORD = {"DW_TAG_structure_type": "struct", "DW_TAG_union_type": "union",
           "DW_TAG_enumeration_type": "enum", "DW_TAG_class_type": "class"}


def declarator(t, inner, depth=0):
    """C declaration of `inner` (a name or an inner declarator) with type t."""
    if depth > 16:
        return "/*?*/ " + inner
    if t is None:
        return ("void " + inner).rstrip()
    tag = t.tag
    if tag in ("DW_TAG_base_type", "DW_TAG_typedef", "DW_TAG_unspecified_type"):
        return ("%s %s" % (attr(t, "DW_AT_name", "void"), inner)).rstrip()
    if tag in TAGWORD:
        n = attr(t, "DW_AT_name")
        if n is None:
            return ("%s /*anonymous*/ %s" % (TAGWORD[tag], inner)).rstrip()
        return ("%s %s %s" % (TAGWORD[tag], n, inner)).rstrip()
    if tag == "DW_TAG_pointer_type":
        sub = type_die(t)
        if sub is not None and sub.tag in ("DW_TAG_subroutine_type", "DW_TAG_array_type"):
            return declarator(sub, "(*%s)" % inner, depth + 1)
        return declarator(sub, "*" + inner, depth + 1)
    if tag == "DW_TAG_reference_type":
        return declarator(type_die(t), "&" + inner, depth + 1)
    if tag == "DW_TAG_const_type":
        sub = type_die(t)
        if sub is not None and sub.tag == "DW_TAG_pointer_type":
            return declarator(sub, "const " + inner, depth + 1)
        return "const " + declarator(sub, inner, depth + 1)
    if tag == "DW_TAG_volatile_type":
        return "volatile " + declarator(type_die(t), inner, depth + 1)
    if tag == "DW_TAG_array_type":
        dims = ""
        for c in t.iter_children():
            if c.tag == "DW_TAG_subrange_type":
                ub = attr(c, "DW_AT_upper_bound")
                cnt = attr(c, "DW_AT_count")
                dims += "[%s]" % (ub + 1 if isinstance(ub, int) else (cnt if isinstance(cnt, int) else ""))
        return declarator(type_die(t), inner + dims, depth + 1)
    if tag == "DW_TAG_subroutine_type":
        ps = [declarator(type_die(c), "", depth + 1) for c in t.iter_children() if c.tag == "DW_TAG_formal_parameter"]
        if any(c.tag == "DW_TAG_unspecified_parameters" for c in t.iter_children()):
            ps.append("...")
        return declarator(type_die(t), "%s(%s)" % (inner, ", ".join(ps) or "void"), depth + 1)
    return "/*%s*/ %s" % (tag, inner)


def body(t, indent="\t"):
    lines = []
    for c in t.iter_children():
        if c.tag == "DW_TAG_member":
            name = attr(c, "DW_AT_name", "")
            sub = type_die(c)
            if sub is not None and sub.tag in TAGWORD and attr(sub, "DW_AT_name") is None:
                lines.append("%s%s" % (indent, TAGWORD[sub.tag]))
                lines.append("%s{" % indent)
                lines.extend(body(sub, indent + "\t"))
                lines.append("%s} %s;" % (indent, name))
                continue
            d = declarator(sub, name)
            bits = attr(c, "DW_AT_bit_size")
            loc = member_offset(attr(c, "DW_AT_data_member_location"))
            lines.append("%s%s%s;%s" % (indent, d, " : %d" % bits if bits else "",
                                        "\t// +%d" % loc if isinstance(loc, int) else ""))
        elif c.tag == "DW_TAG_enumerator":
            lines.append("%s%s = %s," % (indent, attr(c, "DW_AT_name"), attr(c, "DW_AT_const_value")))
        elif c.tag == "DW_TAG_subprogram" and attr(c, "DW_AT_name"):
            # the method with its parameters (the first, artificial one is this)
            ps = []
            for p in c.iter_children():
                if p.tag == "DW_TAG_formal_parameter" and not attr(p, "DW_AT_artificial"):
                    ps.append(declarator(type_die(p), attr(p, "DW_AT_name", "")))
                elif p.tag == "DW_TAG_unspecified_parameters":
                    ps.append("...")
            virt = "DW_AT_virtuality" in c.attributes
            slot = attr(c, "DW_AT_vtable_elem_location")
            if isinstance(slot, list) and slot and slot[0] == 0x10:  # DW_OP_constu
                slot = member_offset([0x23] + slot[1:])
            name = attr(c, "DW_AT_name")
            ret = "" if name == attr(t, "DW_AT_name") or name.startswith("~") else None
            d = declarator(type_die(c), "%s( %s )" % (name, ", ".join(ps) or "void")) if ret is None \
                else "%s( %s )" % (name, ", ".join(ps) or "void")
            lines.append("%s%s%s;%s" % (indent, "virtual " if virt else "", d,
                                        "\t// vtable slot %s (Linux, destructors take two)" % slot if virt and isinstance(slot, int) else ""))
        elif c.tag == "DW_TAG_inheritance":
            lines.append("%s// base %s" % (indent, declarator(type_die(c), "")))
    return lines


def main():
    lib, outdir = sys.argv[1], sys.argv[2]
    only = sys.argv[sys.argv.index("--only") + 1:] if "--only" in sys.argv else []
    dwarf = ELFFile(open(lib, "rb")).get_dwarf_info()
    decls = collections.defaultdict(dict)  # header -> (line, name, kind) -> text
    for cu in dwarf.iter_CUs():
        lp = dwarf.line_program_for_CU(cu)
        if lp is None:
            continue
        files = lp["file_entry"]
        dirs = lp["include_directory"]
        ver = lp.header["version"]

        def fname(idx):
            i = idx if ver >= 5 else idx - 1
            if not (0 <= i < len(files)):
                return None
            fe = files[i]
            n = fe.name.decode("latin-1")
            di = fe.dir_index if ver >= 5 else fe.dir_index - 1
            if fe.dir_index and 0 <= di < len(dirs):
                n = dirs[di].decode("latin-1") + "/" + n
            return os.path.normpath(n).replace("\\", "/").lstrip("./")

        top = cu.get_top_DIE()
        def walk(parent):
            # declarations inside namespaces (namespace vgui { class Button ... })
            for d in parent.iter_children():
                if d.tag == "DW_TAG_namespace":
                    for x in walk(d):
                        yield x
                else:
                    yield d

        for die in walk(top):
            f = attr(die, "DW_AT_decl_file")
            if not f or "DW_AT_abstract_origin" in die.attributes:
                continue
            hdr = fname(f)
            if not hdr or (only and not any(o in hdr for o in only)):
                continue
            line = attr(die, "DW_AT_decl_line", 0)
            name = attr(die, "DW_AT_name")
            tag = die.tag
            text = None
            if tag in TAGWORD and name and "DW_AT_declaration" not in die.attributes:
                size = attr(die, "DW_AT_byte_size")
                text = "\n".join(["%s %s\t// size %s" % (TAGWORD[tag], name, size), "{"] + body(die) + ["};"])
            elif tag == "DW_TAG_typedef" and name:
                sub = type_die(die)
                if sub is not None and sub.tag in TAGWORD and attr(sub, "DW_AT_name") is None:
                    # typedef struct { ... } name;
                    text = "\n".join(["typedef %s\t// size %s" % (TAGWORD[sub.tag], attr(sub, "DW_AT_byte_size")), "{"]
                                     + body(sub) + ["} %s;" % name])
                else:
                    text = "typedef %s;" % declarator(sub, name)
            elif tag == "DW_TAG_subprogram" and name and attr(die, "DW_AT_external"):
                ps = []
                for c in die.iter_children():
                    if c.tag == "DW_TAG_formal_parameter":
                        ps.append(declarator(type_die(c), attr(c, "DW_AT_name", "")))
                    elif c.tag == "DW_TAG_unspecified_parameters":
                        ps.append("...")
                text = "%s;" % declarator(type_die(die), "%s( %s )" % (name, ", ".join(ps) or "void"))
            elif tag == "DW_TAG_variable" and name and attr(die, "DW_AT_external"):
                text = "extern %s;" % declarator(type_die(die), name)
            if text:
                decls[hdr].setdefault((line, name, tag), text)
    n = 0
    for hdr, items in sorted(decls.items()):
        if hdr.endswith((".c", ".cpp", ".C", ".CPP")):
            continue
        path = os.path.join(outdir, hdr.replace("../", "").replace("..", "_"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", newline="\n") as fh:
            fh.write("// declarations of %s from the Linux DWARF (reference only)\n" % hdr)
            for (line, name, tag), text in sorted(items.items(), key=lambda kv: (kv[0][0], kv[0][1] or "")):
                fh.write("\n// line %s\n%s\n" % (line, text))
        n += 1
    print("%d headers written to %s" % (n, outdir))


if __name__ == "__main__":
    main()
