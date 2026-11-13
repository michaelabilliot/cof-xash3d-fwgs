# Prompt template for a decompilation agent

The main agent fills in the bracketed parts and sends the rest unchanged. Keep it short: the guide and one model file are the only reading an agent starts with.

---

You work on DeCoF, a matching decompilation of Cry of Fear 1.6a in I:\COFPP\DeCoF. Goal: make every function of your units compile to the same bytes as retail (objdiff 100%).

Units: [list, for example client/cl_dll/health and client/cl_dll/death]. Source files: [src/cl_dll/health.cpp, ...]. Model file already matched: [path].

Read first: docs/howto_cof_source.md (the sections "Inputs for one unit", "Keep the output small", "Loop", and [the section for this DLL]). Read nothing else up front.

Rules:
- Edit only your source files [and the header that belongs to the unit, for example health.h]. Do not edit any other header, tools/, docs/, the symbol maps or progress.csv. If another header needs a change, stop and report exactly what is needed.
- Never commit, never touch versions/ or refs/.
- Write code the original developers could have written: HLSDK and Paranoia naming and layout, no decompiler artifacts (FUN_, DAT_, iVar1, local_10, raw offsets where a member exists), only C++ that MSVC 2019 accepts with the project flags.
- Keep original behavior and original bugs. Report bugs, do not fix them.
- Names: before you give a parameter, local, member or function a `_guess` name (suffix, for example `CanUseWeapon_guess`; the old `guess_` prefix is being renamed), look for the real one: the 1.55 PDB (`tools/pdbtypes.py`, docs/symbols_1.55.csv) and the older builds (1.55, 1.0, prototype: strings, assert texts, debug build statements) for hl.dll; the reference source (refs/Paranoia first, then refs/halflife, refs/Spirit) and strings for client.dll, plus the hl.dll 1.55 PDB for code shared with the server (pm_shared, common headers, weapon and entity structures). Use a `_guess` name only when none of these has the name; retail orders the functions of an object alphabetically by decorated name, so pick the stem so the name sorts between the function's named neighbours. Say in your report which names are guesses.
- Keep tool output small: `build.py check` and `objdiff_tool.py show` defaults, `disasm.py --outline`, byte counts on listings. Do not print whole files or whole dumps.
- After 8 failed attempts on one function, stop working on it and record the exact remaining difference in your report.
- Edit files with the Edit tool or Python, never `sed -i` (it turns CRLF into LF).

Report at the end, in this order: per unit the `check` result (functions at 100% of total); every function below 100% with its exact remaining difference; header changes needed; names that are `guess_`; original bugs; patterns that are not in the guide yet. No other narrative.
