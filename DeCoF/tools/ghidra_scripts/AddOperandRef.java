// Add a memory reference from operand <op> of the instruction at <from> to <to> (hex args: from op to).
//@category CoF

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

public class AddOperandRef extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] a = getScriptArgs();
		if (a.length == 1) {
			a = a[0].trim().split("\s+");
		}
		Address from = toAddr(Long.parseLong(a[0], 16));
		int op = Integer.parseInt(a[1]);
		Address to = toAddr(Long.parseLong(a[2], 16));
		Instruction ins = getInstructionAt(from);
		println("instruction " + ins);
		currentProgram.getReferenceManager().addMemoryReference(from, to, RefType.DATA, SourceType.USER_DEFINED, op);
		for (ghidra.program.model.symbol.Reference r : ins.getReferencesFrom()) {
			println("  ref " + r);
		}
	}
}
