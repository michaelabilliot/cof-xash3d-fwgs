// Remove the non-function labels at the given addresses (hex args).
//@category CoF

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SymbolType;

public class DropLabelsAt extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] a = getScriptArgs();
		if (a.length == 1) {
			a = a[0].trim().split("\s+");
		}
		SymbolTable st = currentProgram.getSymbolTable();
		for (String s : a) {
			Address addr = toAddr(Long.parseLong(s, 16));
			for (Symbol sym : st.getSymbols(addr)) {
				println(addr + " " + sym.getName() + " " + sym.getSymbolType() + " " + sym.getSource());
				if (sym.getSymbolType() == SymbolType.LABEL) {
					sym.delete();
					println("  deleted");
				}
			}
		}
	}
}
