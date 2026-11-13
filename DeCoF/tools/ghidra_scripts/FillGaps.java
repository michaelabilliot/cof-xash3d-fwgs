// Create functions over the code holes that tools/gaps.py reports.
// Arguments: <start>:<end> pairs (hex). For each hole: keep a function that
// already starts there, otherwise disassemble and create one; continue after
// its end (past int3/nop fill) until the hole is covered. Prints one line per
// function found or created: "FG <va> <size> <name> <new|old>".
//@category CoF

import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;

public class FillGaps extends GhidraScript {
	@Override
	protected void run() throws Exception {
		for (String arg : getScriptArgs()) {
			String[] p = arg.split(":");
			Address a = toAddr(Long.parseLong(p[0], 16));
			Address end = toAddr(Long.parseLong(p[1], 16));
			int guard = 0;
			while (a.compareTo(end) < 0 && guard++ < 64) {
				int b = getByte(a) & 0xff;
				if (b == 0xcc || b == 0x90) {
					a = a.add(1);
					continue;
				}
				Function f = getFunctionAt(a);
				String state = "old";
				if (f == null) {
					Function c = getFunctionContaining(a);
					if (c != null) {
						println("FG " + a + " inside " + c.getName() + " " + c.getEntryPoint());
						break;
					}
					if (getInstructionAt(a) == null) {
						new DisassembleCommand(a, new AddressSet(a, end.subtract(1)), true).applyTo(currentProgram, monitor);
					}
					CreateFunctionCmd cmd = new CreateFunctionCmd(a);
					cmd.applyTo(currentProgram, monitor);
					f = getFunctionAt(a);
					state = "new";
					if (f == null) {
						println("FG " + a + " failed " + cmd.getStatusMsg());
						break;
					}
				}
				Address last = f.getBody().getMaxAddress();
				long size = last.subtract(f.getEntryPoint()) + 1;
				println("FG " + f.getEntryPoint() + " " + size + " " + f.getName() + " " + state);
				a = last.add(1);
			}
		}
	}
}
