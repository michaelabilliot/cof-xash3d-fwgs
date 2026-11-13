"""Progress report: docs/progress.json, docs/progress.svg, docs/PROGRESS.md.

  python tools/progress.py [--csv PATH]

Reads docs/progress.csv (or PATH, used by the pre-commit hook to read the
staged copy) and writes the three outputs. Standard library only. Every
number comes from the CSV; never edit the outputs by hand.

Counting rules (also written into PROGRESS.md):
- Rows of class LIB (static CRT, STL and compiler helpers from the
  toolchain) are not decompiled and are left out of every figure.
- MATCHED: objdiff reports 100 percent for the function.
- EQUIVALENT: compiles, logic verified against the assembly, not byte
  identical.
- TODO: everything else.
- Byte figures weight each function by its size in the retail binary
  (column "size").
"""
import argparse
import csv
import datetime
import json
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = os.path.join(ROOT, "docs")

# CSV dll key -> binary name, in the priority order of the scope (AGENTS.md)
BINARIES = [
    ("hl", "hl.dll"),
    ("client", "client.dll"),
    ("hw", "hw.dll"),
    ("opengl32", "opengl32.dll"),
    ("gameui", "GameUI.dll"),
    ("launcher", "cof.exe"),
    ("coflaunchapp", "CoFLaunchApp.exe"),
]
# binaries given a category table in PROGRESS.md and counted in "shown_total"
SHOWN = [key for key, _name in BINARIES]
# what each binary is and what built it, drawn after its name in the SVG
ABOUT = {
    "hl": "server game DLL, MSVC 14.27",
    "client": "client DLL with the Paranoia renderer, MSVC 14.27",
    "hw": "GoldSrc engine, Visual C++ 6.0",
    "opengl32": "Paranoia OpenGL wrapper, Visual C++ 6.0",
    "gameui": "menus (VGUI2), Visual C++ 6.0",
    "launcher": "engine launcher, Visual C++ 6.0",
    "coflaunchapp": "settings launcher, C# 4",
}
# why a binary has not started, drawn next to its bar in the SVG
NOTES = {}
# in scope but not in the CSV yet: (name, note)
NOT_INVENTORIED = []
CATEGORIES = ["HLSDK", "PARANOIA", "SPIRIT", "COF", "ENGINE", "UNKNOWN"]
STATES = ["MATCHED", "EQUIVALENT", "TODO"]

# SVG colors, light and dark: the bar fills are two validated categorical
# hues (blue, orange; CVD and contrast checked against both surfaces), TODO
# is the neutral track; text uses ink colors, never a fill color
SVG_STYLE = """
text { font-family: system-ui, -apple-system, "Segoe UI", Helvetica, Arial, sans-serif; }
.num { font-variant-numeric: tabular-nums; }
.bg { fill: #fcfcfb; stroke: rgba(11, 11, 11, 0.10); }
.tile { fill: #f3f2ee; }
.rule { stroke: #e1e0d9; }
.t1 { fill: #0b0b0b; }
.t2 { fill: #52514e; }
.t3 { fill: #6f6e69; }
.MATCHED { fill: #2a78d6; }
.EQUIVALENT { fill: #eb6834; }
.TODO { fill: #dcdbd4; }
@media (prefers-color-scheme: dark) {
  .bg { fill: #1a1a19; stroke: rgba(255, 255, 255, 0.10); }
  .tile { fill: #242422; }
  .rule { stroke: #2c2c2a; }
  .t1 { fill: #f0efec; }
  .t2 { fill: #c3c2b7; }
  .t3 { fill: #9a9890; }
  .MATCHED { fill: #3987e5; }
  .EQUIVALENT { fill: #d95926; }
  .TODO { fill: #383835; }
}
"""


def empty():
    return {"functions": {"total": 0, "MATCHED": 0, "EQUIVALENT": 0, "TODO": 0},
            "bytes": {"total": 0, "MATCHED": 0, "EQUIVALENT": 0, "TODO": 0}}


def add(acc, state, size):
    for key, n in (("functions", 1), ("bytes", size)):
        acc[key]["total"] += n
        acc[key][state] += n


def merge(dst, src):
    for key in ("functions", "bytes"):
        for k, v in src[key].items():
            dst[key][k] += v


def pct(part, whole):
    return 100.0 * part / whole if whole else 0.0


def git(*args):
    try:
        return subprocess.run(["git"] + list(args), cwd=ROOT, capture_output=True,
                              text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def target_version():
    """Version of the target build, from docs/versions.csv (the row whose
    note marks it as the target; its version is the version string found
    in client.dll, see docs/sessions.md)."""
    path = os.path.join(DOCS, "versions.csv")
    if os.path.exists(path):
        for r in csv.DictReader(open(path, encoding="utf-8")):
            if (r.get("note") or "").startswith("target"):
                return r["version"]
    return "unknown"


def collect(csv_path):
    per = {key: empty() for key, _ in BINARIES}
    cats = {key: {} for key, _ in BINARIES}
    for r in csv.DictReader(open(csv_path, encoding="utf-8")):
        dll, cls = r["dll"], r["class"]
        if dll not in per or cls == "LIB":
            continue
        state = r["status"] if r["status"] in ("MATCHED", "EQUIVALENT") else "TODO"
        size = int(r["size"] or 0)
        add(per[dll], state, size)
        add(cats[dll].setdefault(cls or "UNKNOWN", empty()), state, size)
    return per, cats


def write_json(per, cats, meta):
    out = {"target_version": meta["version"], "commit": meta["commit"], "date": meta["date"],
           "counting": "LIB rows excluded; bytes weighted by function size",
           "dlls": {}, "total": empty(), "shown_total": empty()}
    for key, name in BINARIES:
        d = dict(per[key])
        d["categories"] = {c: cats[key][c] for c in sorted(cats[key])}
        out["dlls"][name] = d
        merge(out["total"], per[key])
        if key in SHOWN:
            merge(out["shown_total"], per[key])
    with open(os.path.join(DOCS, "progress.json"), "w", newline="\n") as f:
        json.dump(out, f, indent=2, sort_keys=True)
        f.write("\n")
    return out


def esc(s):
    return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


DOT = " &#183; "


def pct_label(v):
    """One decimal, but 99.96 prints as 99.9, so only a complete figure reads 100."""
    return "100" if v >= 100.0 else "%.1f" % min(round(v, 1), 99.9)


def bar_segments(b, x0, width, gap=2.0, least=2.0):
    """(state, x, w) of the MATCHED, EQUIVALENT and TODO parts of a bar,
    byte weighted, with a surface gap between parts; a non-empty part is
    at least `least` wide so a sliver stays visible."""
    parts = [(s, b[s]) for s in STATES if b[s] > 0]
    if not parts or not b["total"]:
        return [("TODO", x0, width)]
    room = width - gap * (len(parts) - 1)
    ws = [max(least, room * n / b["total"]) for _s, n in parts]
    scale = room / sum(ws)
    out, x = [], float(x0)
    for (s, _n), w in zip(parts, ws):
        out.append((s, x, w * scale))
        x += w * scale + gap
    return out


def write_svg(per, meta):
    W, P = 880, 32
    total = empty()
    for key, _name in BINARIES:
        merge(total, per[key])
    fn, b = total["functions"], total["bytes"]
    done_fn = fn["MATCHED"] + fn["EQUIVALENT"]

    row_h, rows_top = 66, 286
    n_rows = len(BINARIES) + len(NOT_INVENTORIED)
    foot_y = rows_top + n_rows * row_h + 8
    H = foot_y + 38
    o = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d" role="img" '
         'aria-labelledby="title desc">' % (W, H, W, H),
         '<title id="title">DeCoF progress</title>',
         '<desc id="desc">Matching decompilation of Cry of Fear %s: %s of %s in-scope functions rebuilt, '
         '%s byte-identical (MATCHED), %s logic-verified (EQUIVALENT), %s%% of code bytes identical.</desc>'
         % (meta["version"], "{:,}".format(done_fn), "{:,}".format(fn["total"]), "{:,}".format(fn["MATCHED"]),
            "{:,}".format(fn["EQUIVALENT"]), pct_label(pct(b["MATCHED"], b["total"]))),
         "<style>%s</style>" % SVG_STYLE,
         # own card, so the chart reads the same on any page background
         '<rect class="bg" x="0.5" y="0.5" width="%d" height="%d" rx="12"/>' % (W - 1, H - 1)]

    # header
    o.append('<text class="t1" x="%d" y="54" font-size="30" font-weight="700">DeCoF</text>' % P)
    o.append('<text class="t2" x="%d" y="78" font-size="14">Cry of Fear %s rebuilt from source with the original '
             'compilers, function by function</text>' % (P, esc(meta["version"])))

    # one hero figure, then three stat tiles
    o.append('<text class="t1" x="%d" y="160" font-size="56" font-weight="700">%s%%</text>'
             % (P, pct_label(pct(done_fn, fn["total"]))))
    o.append('<text class="t2" x="%d" y="186" font-size="13">of %s functions in scope rebuilt</text>'
             % (P, "{:,}".format(fn["total"])))
    tiles = [("{:,}".format(fn["MATCHED"]), "byte-identical (MATCHED)"),
             ("{:,}".format(fn["EQUIVALENT"]), "logic-verified (EQUIVALENT)"),
             ("%s%%" % pct_label(pct(b["MATCHED"], b["total"])), "of code bytes identical")]
    tx, tgap = 300, 12
    tw = (W - P - tx - tgap * (len(tiles) - 1)) / float(len(tiles))
    for i, (value, label) in enumerate(tiles):
        x = tx + i * (tw + tgap)
        o.append('<rect class="tile" x="%.1f" y="106" width="%.1f" height="86" rx="10"/>' % (x, tw))
        o.append('<text class="t1" x="%.1f" y="150" font-size="28" font-weight="600">%s</text>' % (x + 16, value))
        o.append('<text class="t2" x="%.1f" y="174" font-size="12">%s</text>' % (x + 16, esc(label)))

    # section header with the legend
    o.append('<line class="rule" x1="%d" y1="222.5" x2="%d" y2="222.5" stroke-width="1"/>' % (P, W - P))
    o.append('<text class="t1" x="%d" y="252" font-size="15" font-weight="600">Per binary</text>' % P)
    o.append('<text class="t3" x="%d" y="270" font-size="12">bar length: share of code bytes; '
             'toolchain code (static CRT, STL) is not counted</text>' % P)
    # one right-aligned run (square glyph in the state color, then the label in ink), so no text widths are guessed
    o.append('<text x="%d" y="252" font-size="12" text-anchor="end">%s</text>' % (W - P, "".join(
        '<tspan class="%s" font-size="14"%s>&#9632;</tspan><tspan class="t2" dx="5">%s</tspan>'
        % (state, ' dx="16"' if i else "", state) for i, state in enumerate(STATES))))

    y = rows_top
    for key, name in BINARIES:
        d = per[key]
        rb, rf = d["bytes"], d["functions"]
        ident = pct_label(pct(rb["MATCHED"], rb["total"]))
        o.append("<g><title>%s: %s%% of code bytes identical; %s MATCHED, %s EQUIVALENT, %s TODO</title>" % (
            esc(name), ident, "{:,}".format(rf["MATCHED"]), "{:,}".format(rf["EQUIVALENT"]),
            "{:,}".format(rf["TODO"])))
        o.append('<text x="%d" y="%d"><tspan class="t1" font-size="15" font-weight="600">%s</tspan>'
                 '<tspan class="t3" font-size="12" dx="10">%s</tspan></text>'
                 % (P, y + 16, esc(name), esc(ABOUT.get(key, ""))))
        o.append('<text class="t1 num" x="%d" y="%d" font-size="14" font-weight="600" text-anchor="end">'
                 '%s%% identical</text>' % (W - P, y + 16, ident))
        by, bh = y + 26, 12
        o.append('<clipPath id="bar-%s"><rect x="%d" y="%d" width="%d" height="%d" rx="4"/></clipPath>'
                 % (key, P, by, W - 2 * P, bh))
        o.append('<g clip-path="url(#bar-%s)">' % key)
        for state, x, w in bar_segments(rb, P, W - 2 * P):
            o.append('<rect class="%s" x="%.2f" y="%d" width="%.2f" height="%d"/>' % (state, x, by, w, bh))
        o.append("</g>")
        detail = DOT.join(["%s MATCHED" % "{:,}".format(rf["MATCHED"]),
                           "%s EQUIVALENT" % "{:,}".format(rf["EQUIVALENT"]),
                           "%s TODO" % "{:,}".format(rf["TODO"]),
                           "%s code bytes" % "{:,}".format(rb["total"])])
        if NOTES.get(key):
            detail += DOT + esc(NOTES[key])
        o.append('<text class="t2 num" x="%d" y="%d" font-size="12">%s</text>' % (P, by + 30, detail))
        o.append("</g>")
        y += row_h
    for name, note in NOT_INVENTORIED:
        o.append('<text class="t1" x="%d" y="%d" font-size="15" font-weight="600">%s</text>' % (P, y + 16, esc(name)))
        o.append('<rect class="TODO" x="%d" y="%d" width="%d" height="12" rx="4"/>' % (P, y + 26, W - 2 * P))
        o.append('<text class="t2" x="%d" y="%d" font-size="12">not started%s</text>'
                 % (P, y + 56, DOT + esc(note) if note else ""))
        y += row_h

    o.append('<line class="rule" x1="%d" y1="%.1f" x2="%d" y2="%.1f" stroke-width="1"/>'
             % (P, foot_y + 0.5, W - P, foot_y + 0.5))
    foot = DOT.join(["target build " + esc(meta["version"]), "commit " + esc(meta["commit"] or "unknown"),
                     esc(meta["date"] or "unknown"), "generated by tools/progress.py from docs/progress.csv"])
    o.append('<text class="t3" x="%d" y="%d" font-size="11">%s</text>' % (P, foot_y + 24, foot))
    o.append("</svg>")
    with open(os.path.join(DOCS, "progress.svg"), "w", newline="\n") as f:
        f.write("\n".join(o) + "\n")


def write_md(per, cats, meta):
    o = ["# Progress", "",
         "Generated by `tools/progress.py` from `docs/progress.csv`. Do not edit by hand.", "",
         "![progress](progress.svg)", "",
         "Target build %s, commit %s, %s." % (meta["version"], meta["commit"] or "unknown",
                                              meta["date"] or "unknown"), ""]
    for key, name in BINARIES:
        if key not in SHOWN:
            continue
        o.append("## %s" % name)
        o.append("")
        o.append("| Category | Matched | Equivalent | Total | Percent |")
        o.append("|---|---:|---:|---:|---:|")
        extra = sorted(c for c in cats[key] if c not in CATEGORIES)
        for c in CATEGORIES + extra:
            d = cats[key].get(c, empty())
            fn, b = d["functions"], d["bytes"]
            if not fn["total"]:
                continue
            o.append("| %s | %d | %d | %d | %.1f%% |" % (
                c, fn["MATCHED"], fn["EQUIVALENT"], fn["total"], pct(b["MATCHED"] + b["EQUIVALENT"], b["total"])))
        fn, b = per[key]["functions"], per[key]["bytes"]
        o.append("| **All** | **%d** | **%d** | **%d** | **%.1f%%** |" % (
            fn["MATCHED"], fn["EQUIVALENT"], fn["total"], pct(b["MATCHED"] + b["EQUIVALENT"], b["total"])))
        o.append("")
    idle = [(key, name) for key, name in BINARIES if key not in SHOWN]
    if idle or NOT_INVENTORIED:
        o += ["## Not started", ""]
        for key, name in idle:
            o.append("- %s: %d functions in the CSV, all TODO%s." % (
                name, per[key]["functions"]["total"], " (%s)" % NOTES[key] if key in NOTES else ""))
        for name, note in NOT_INVENTORIED:
            o.append("- %s: %s." % (name, note))
        o.append("")
    o += [
          "## Counting rules", "",
          "- **MATCHED**: the objdiff report gives 100 percent for the function: the instructions are "
          "identical to the retail binary. The report does not compare the names of relocation targets "
          "(many retail symbols are still unnamed); `tools/relocaudit.py` lists matched functions whose "
          "relocations point at differently named symbols.",
          "- **EQUIVALENT**: the function compiles and its logic was verified against the assembly, "
          "but the code is not byte identical. The remaining difference is recorded in `docs/progress.csv`.",
          "- **TODO**: every other function, including stubs and functions that do not compile.",
          "- Rows of class LIB (static CRT, STL instantiations and compiler generated helpers from the "
          "toolchain) are not decompiled and are left out of every figure.",
          "- Matched, Equivalent and Total in the tables are function counts. Percent is "
          "(MATCHED + EQUIVALENT) code bytes over all code bytes of the row; each function weighs its "
          "size in the retail binary.",
          "- The SVG bars use the same byte weighting; the function counts are shown next to them.", ""]
    with open(os.path.join(DOCS, "PROGRESS.md"), "w", newline="\n") as f:
        f.write("\n".join(o))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default=os.path.join(DOCS, "progress.csv"))
    args = ap.parse_args()
    meta = {"version": target_version(), "commit": git("rev-parse", "--short", "HEAD"),
            "date": git("log", "-1", "--format=%cs") or datetime.date.today().isoformat()}
    per, cats = collect(args.csv)
    out = write_json(per, cats, meta)
    write_svg(per, meta)
    write_md(per, cats, meta)
    for key, name in BINARIES:
        fn, b = per[key]["functions"], per[key]["bytes"]
        print("%-13s %5d/%5d functions matched, %5d equivalent, %5.1f%% of %8d bytes matched" % (
            name, fn["MATCHED"], fn["total"], fn["EQUIVALENT"], pct(b["MATCHED"], b["total"]), b["total"]))
    t = out["total"]
    print("%-13s %5d/%5d functions matched, %5d equivalent, %5.1f%% of %8d bytes matched" % (
        "total", t["functions"]["MATCHED"], t["functions"]["total"], t["functions"]["EQUIVALENT"],
        pct(t["bytes"]["MATCHED"], t["bytes"]["total"]), t["bytes"]["total"]))


if __name__ == "__main__":
    main()
