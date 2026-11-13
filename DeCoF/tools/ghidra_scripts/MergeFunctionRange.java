// Merge a function that Ghidra split (VC6 try/catch: the catch funclet and the
// continuation after it became functions of their own) back into one body.
//   args: <entry hex> <last byte hex> [dry]
// Lists the functions inside the range, removes every one except the entry,
// disassembles what is not yet code, clears the flow overrides inside the
// range (a jmp into the removed continuation was turned into a call) and sets
// the body of the entry function to <entry>..<last>. With "dry" it only lists.
//@category CoF

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.FlowOverride;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;

public class MergeFunctionRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] a = getScriptArgs();
		if (a.length == 1) {
			a = a[0].trim().split("\\s+");
		}
		Address entry = toAddr(Long.parseLong(a[0], 16));
		Address last = toAddr(Long.parseLong(a[1], 16));
		boolean dry = a.length > 2 && a[2].equals("dry");
		Listing L = currentProgram.getListing();
		AddressSet range = new AddressSet(entry, last);
		for (Function f : L.getFunctions(range, true)) {
			println("function " + f.getEntryPoint() + " " + f.getName() + " " + f.getBody());
		}
		for (Instruction i : L.getInstructions(range, true)) {
			if (i.getFlowOverride() != FlowOverride.NONE) {
				println("flow override " + i.getAddress() + " " + i + " " + i.getFlowOverride());
			}
		}
		if (dry) {
			return;
		}
		Function main = getFunctionAt(entry);
		if (main == null) {
			println("no function at " + entry);
			return;
		}
		for (Function f : L.getFunctions(range, true)) {
			if (!f.getEntryPoint().equals(entry)) {
				println("removing " + f.getEntryPoint() + " " + f.getName());
				removeFunction(f);
			}
		}
		Address p = entry;
		while (p.compareTo(last) <= 0) {
			if (L.getInstructionContaining(p) == null) {
				new DisassembleCommand(p, new AddressSet(p, last), false).applyTo(currentProgram, monitor);
				if (L.getInstructionContaining(p) == null) {
					println("cannot disassemble at " + p);
					return;
				}
			}
			p = L.getInstructionContaining(p).getMaxAddress().add(1);
		}
		for (Instruction i : L.getInstructions(range, true)) {
			if (i.getFlowOverride() != FlowOverride.NONE) {
				i.setFlowOverride(FlowOverride.NONE);
				println("cleared flow override at " + i.getAddress());
			}
		}
		main.setBody(range);
		println("body of " + main.getName() + " set to " + entry + ".." + last);
	}
}
