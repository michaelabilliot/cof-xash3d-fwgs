// Set the body of the function at <entry> to the contiguous range
// <entry>..<last> (hex addresses in the args), disassembling bytes there that
// are not yet instructions. Used for VC6 __except filter/handler blocks that
// no flow reaches.
//@category CoF

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Listing;

public class SetBodyRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] a = getScriptArgs();
		if (a.length == 1) {
			a = a[0].trim().split("\s+");
		}
		Address entry = toAddr(Long.parseLong(a[0], 16));
		Address last = toAddr(Long.parseLong(a[1], 16));
		Listing L = currentProgram.getListing();
		Function f = getFunctionAt(entry);
		if (f == null) {
			println("no function at " + entry);
			return;
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
		f.setBody(new AddressSet(entry, last));
		println("body of " + f.getName() + " set to " + entry + ".." + last);
	}
}
