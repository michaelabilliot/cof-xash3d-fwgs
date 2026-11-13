"""Source file of every function of a Linux GoldSrc library (hw.so, gameui.so).

  python tools/elf_files.py <lib.so> <out.csv> [--inlined <out2.csv>]

The Linux steam_legacy builds carry DWARF debug information: every
compile unit names its source file and lists its functions (subprograms
with their address). Writes va,size,name,file,line for every function of
the library: file is the compile unit's source path, line the line of the
function's declaration. Functions the DWARF does not cover (CRT, glue)
fall back to the symbol table with an empty file. --inlined also writes
file,name,line of the functions GCC inlined everywhere (no address): an
unoptimized Windows build still has them as separate functions.
"""
import csv
import sys

from elftools.elf.elffile import ELFFile

INLINED = None  # [file, name, line] of functions without an address (--inlined)


def main():
    global INLINED
    lib, out = sys.argv[1], sys.argv[2]
    if "--inlined" in sys.argv:
        INLINED = []
    elf = ELFFile(open(lib, "rb"))
    funcs = {}
    if elf.has_dwarf_info():
        dwarf = elf.get_dwarf_info()
        for cu in dwarf.iter_CUs():
            top = cu.get_top_DIE()
            cuname = top.attributes.get("DW_AT_name")
            cuname = cuname.value.decode("latin-1") if cuname else ""
            # line of the first instruction of each address in the unit's own
            # file: the definition line of C++ methods, whose DIE carries only
            # the declaration's line inside the class
            firstline = {}
            primary = 1
            lp = dwarf.line_program_for_CU(cu)
            if lp is not None:
                # the unit's own file is not always entry 1 of its file table
                base = cuname.replace("\\", "/").split("/")[-1].lower()
                for i, fe in enumerate(lp["file_entry"]):
                    if fe.name.decode("latin-1").replace("\\", "/").split("/")[-1].lower() == base:
                        primary = i + 1
                        break
                for e in lp.get_entries():
                    st = e.state
                    if st is not None and not st.end_sequence and st.file == primary:
                        firstline.setdefault(st.address, st.line)
            for die in cu.iter_DIEs():
                if die.tag != "DW_TAG_subprogram":
                    continue
                if "DW_AT_low_pc" not in die.attributes:
                    # a function GCC only inlined still exists in an /Od
                    # build: keep it, without an address
                    df = die.attributes.get("DW_AT_decl_file")
                    if INLINED is not None and "DW_AT_inline" in die.attributes and "DW_AT_name" in die.attributes \
                            and df is not None and df.value == primary:
                        line = die.attributes.get("DW_AT_decl_line")
                        INLINED.append([cuname, die.attributes["DW_AT_name"].value.decode("latin-1"),
                                        line.value if line else ""])
                    continue
                lo = die.attributes["DW_AT_low_pc"].value
                hi = die.attributes.get("DW_AT_high_pc")
                size = 0
                if hi is not None:
                    size = hi.value if hi.form != "DW_FORM_addr" else hi.value - lo
                name = die.attributes.get("DW_AT_MIPS_linkage_name") or die.attributes.get("DW_AT_linkage_name") \
                    or die.attributes.get("DW_AT_name")
                name = name.value.decode("latin-1") if name else ""
                line = die.attributes.get("DW_AT_decl_line")
                line = line.value if line else firstline.get(lo, "")
                funcs.setdefault(lo, [lo, size, name, cuname, line])
    symtab = elf.get_section_by_name(".symtab")
    for s in symtab.iter_symbols():
        if s["st_info"]["type"] == "STT_FUNC" and s["st_value"]:
            f = funcs.get(s["st_value"])
            if f is None:
                funcs[s["st_value"]] = [s["st_value"], s["st_size"], s.name, "", ""]
            else:
                f[2] = s.name  # the mangled symbol name
                f[1] = f[1] or s["st_size"]
    rows = sorted(funcs.values())
    with open(out, "w", newline="") as fh:
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(["va", "size", "name", "file", "line"])
        for va, size, name, fl, line in rows:
            w.writerow(["%08X" % va, size, name, fl, line])
    print("%d functions, %d with a file, %d files" % (len(rows), sum(1 for f in rows if f[3]),
                                                      len(set(f[3] for f in rows if f[3]))))
    if INLINED is not None:
        seen = set()
        with open(sys.argv[sys.argv.index("--inlined") + 1], "w", newline="") as fh:
            w = csv.writer(fh, lineterminator="\n")
            w.writerow(["file", "name", "line"])
            for f, n, l in INLINED:
                if (f, n) not in seen:
                    seen.add((f, n))
                    w.writerow([f, n, l])
        print("%d inlined-only functions" % len(seen))


if __name__ == "__main__":
    main()
