// Decompile every function in an address range into one file, followed by the
// string literals the range references (tools/range.py).
// Arguments: <start hex> <end hex> <output file>
//@category CoF

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import java.io.*;
import java.util.*;

public class RangeDecomp extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] a = getScriptArgs();
		Address start = toAddr(Long.parseLong(a[0], 16));
		Address end = toAddr(Long.parseLong(a[1], 16));
		PrintWriter out = new PrintWriter(new FileWriter(a[2]));
		DecompInterface di = new DecompInterface();
		di.openProgram(currentProgram);
		FunctionIterator it = currentProgram.getFunctionManager().getFunctions(start, true);
		int n = 0;
		while (it.hasNext()) {
			Function f = it.next();
			if (f.getEntryPoint().compareTo(end) >= 0) break;
			DecompileResults r = di.decompileFunction(f, 120, monitor);
			out.println("// " + f.getEntryPoint() + " " + f.getName() + " size " + f.getBody().getNumAddresses());
			out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompilation failed");
			n++;
		}
		// string literals referenced from the range, exact bytes
		TreeMap<Long, String> strs = new TreeMap<>();
		InstructionIterator ii = currentProgram.getListing().getInstructions(new AddressSet(start, end.subtract(1)), true);
		while (ii.hasNext()) {
			Instruction ins = ii.next();
			for (Reference ref : ins.getReferencesFrom()) {
				Address t = ref.getToAddress();
				if (!t.isMemoryAddress() || currentProgram.getMemory().getBlock(t) == null) continue;
				if (currentProgram.getMemory().getBlock(t).isExecute()) continue;
				StringBuilder sb = new StringBuilder();
				boolean ok = true;
				int len = 0;
				for (int i = 0; i < 400; i++) {
					int b;
					try { b = getByte(t.add(i)) & 0xff; } catch (Exception e) { ok = false; break; }
					if (b == 0) break;
					len++;
					if (b == 10) sb.append("\\n"); else if (b == 9) sb.append("\\t"); else if (b == 13) sb.append("\\r");
					else if (b == 34) sb.append("\\\""); else if (b == 92) sb.append("\\\\");
					else if (b < 32 || b > 126) { ok = false; break; } else sb.append((char) b);
				}
				if (ok && len > 0) strs.put(t.getOffset(), sb.toString());
			}
		}
		out.println("\n// string literals referenced by the range");
		for (Map.Entry<Long, String> e : strs.entrySet())
			out.println("// " + Long.toHexString(e.getKey()) + " \"" + e.getValue() + "\"");
		out.close();
		println("RangeDecomp: " + n + " functions, " + strs.size() + " strings");
	}
}
