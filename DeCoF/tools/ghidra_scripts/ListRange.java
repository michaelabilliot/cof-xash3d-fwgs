// List the labels and defined data of an address range (debugging the symbol map).
//   args: <start hex> <end hex>
//@category CoF

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Data;
import ghidra.program.model.symbol.Symbol;

public class ListRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] a = getScriptArgs();
		Address lo = toAddr(Long.parseUnsignedLong(a[0], 16));
		Address hi = toAddr(Long.parseUnsignedLong(a[1], 16));
		AddressSet range = new AddressSet(lo, hi);
		for (Symbol s : currentProgram.getSymbolTable().getSymbols(range, ghidra.program.model.symbol.SymbolType.LABEL, true)) {
			println("label " + s.getAddress() + " " + s.getName() + " " + s.getSource() + (s.isDynamic() ? " dynamic" : "")
					+ (s.isPrimary() ? " primary" : ""));
		}
		for (Data d : currentProgram.getListing().getDefinedData(range, true)) {
			println("data " + d.getMinAddress() + "-" + d.getMaxAddress() + " " + d.getDataType().getName());
		}
	}
}
