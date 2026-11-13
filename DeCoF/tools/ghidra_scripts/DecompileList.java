// Decompile a list of functions into one text file per function.
//
// One argument: a job file with
//   out <directory>
//   func <name>          (decorated or primary name)
// The output is reference material for writing source by hand: Ghidra's
// pseudo C, never committed.
//@category CoF

import java.io.File;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;

public class DecompileList extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		File out = null;
		DecompInterface decomp = new DecompInterface();
		decomp.openProgram(currentProgram);
		int n = 0;
		for (String raw : Files.readAllLines(Paths.get(args[0]))) {
			String line = raw.strip();
			if (line.isEmpty()) {
				continue;
			}
			int sp = line.indexOf(' ');
			String cmd = line.substring(0, sp);
			String arg = line.substring(sp + 1).strip();
			if (cmd.equals("out")) {
				out = new File(arg);
				out.mkdirs();
				continue;
			}
			Function fn = null;
			for (Symbol s : currentProgram.getSymbolTable().getSymbols(arg)) {
				fn = getFunctionAt(s.getAddress());
				if (fn != null) {
					break;
				}
			}
			if (fn == null) {
				printerr("DecompileList: no function " + arg);
				continue;
			}
			DecompileResults res = decomp.decompileFunction(fn, 120, monitor);
			String safe = arg.replaceAll("[^A-Za-z0-9_@$]", "_");
			if (safe.length() > 120) {
				safe = safe.substring(0, 120);
			}
			try (PrintWriter w = new PrintWriter(new File(out, safe + ".c"))) {
				w.println("// " + arg + " at " + fn.getEntryPoint());
				w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "// failed: " + res.getErrorMessage());
			}
			n++;
		}
		decomp.dispose();
		println("DecompileList: " + n + " functions");
	}
}
