// Extend functions that Ghidra ends at a call to a no-return function
// (longjmp, _exit) over the epilogue VC6 still emits after the call
// (8b e5 5d c3, 5d c3 or c3), so the delinked retail function has its full
// length (Host_EndGame, Host_Error, Sys_Error). Saves nothing by itself.
//@category CoF

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;

public class ExtendAfterNoReturn extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Listing L = currentProgram.getListing();
		FunctionManager fm = currentProgram.getFunctionManager();
		int n = 0;
		for (Function f : fm.getFunctions(true)) {
			Address end = f.getBody().getMaxAddress();
			Instruction last = L.getInstructionContaining(end);
			if (last == null || !last.getFlowType().isCall()) {
				continue;
			}
			Address[] flows = last.getFlows();
			if (flows.length != 1) {
				continue;
			}
			Function callee = fm.getFunctionAt(flows[0]);
			if (callee == null || !callee.hasNoReturn()) {
				continue;
			}
			Address next = last.getMaxAddress().add(1);
			if (fm.getFunctionContaining(next) != null) {
				continue;
			}
			byte[] b = new byte[4];
			currentProgram.getMemory().getBytes(next, b);
			int len = 0;
			if ((b[0] & 0xff) == 0x8b && (b[1] & 0xff) == 0xe5 && (b[2] & 0xff) == 0x5d && (b[3] & 0xff) == 0xc3) {
				len = 4;
			}
			else if ((b[0] & 0xff) == 0x5d && (b[1] & 0xff) == 0xc3) {
				len = 2;
			}
			else if ((b[0] & 0xff) == 0xc3) {
				len = 1;
			}
			if (len == 0) {
				continue;
			}
			Address tail = next.add(len - 1);
			if (L.getInstructionAt(next) == null) {
				clearListing(next, tail);
				new DisassembleCommand(next, new AddressSet(next, tail), false).applyTo(currentProgram, monitor);
			}
			AddressSet body = new AddressSet(f.getBody());
			body.add(next, tail);
			f.setBody(body);
			n++;
			println("extended " + f.getName() + " by " + len + " after " + callee.getName());
		}
		println("extended " + n);
	}
}
