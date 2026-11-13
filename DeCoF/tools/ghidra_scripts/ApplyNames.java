// Apply a symbol map (CSV: va,name,kind[,evidence]) to the current program
// and demangle it.
//
// Every row adds a label with the exact decorated name (MSVC "?..." names
// and C "_name" names). C names also become the primary name without the
// underscore (what the delinker expects). Missing functions are created.
// Then the "Demangler Microsoft" analyzer runs once over the labelled
// addresses, which gives functions readable names and signatures (calling
// convention, parameter types) where the decorated name encodes them.
//
// Usage: ApplyNames.java <csv>
//@category CoF

import java.nio.file.Files;
import java.nio.file.Paths;

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.plugin.core.analysis.AutoAnalysisManager;
import ghidra.app.script.GhidraScript;
import ghidra.app.services.Analyzer;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;

public class ApplyNames extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		SymbolTable symtab = currentProgram.getSymbolTable();
		AddressSet touched = new AddressSet();
		int applied = 0, created = 0;
		int tx = currentProgram.startTransaction("Apply CoF names");
		try {
			for (String raw : Files.readAllLines(Paths.get(args[0]))) {
				String line = raw.strip();
				if (line.isEmpty() || line.startsWith("#") || line.startsWith("va,")) {
					continue;
				}
				String[] f = line.split(",", -1);
				Address addr = toAddr(Long.parseUnsignedLong(f[0], 16));
				String name = f[1];
				String kind = f.length > 2 ? f[2] : "func";
				if (name.isEmpty()) {
					continue;
				}
				if (kind.equals("func") && getFunctionAt(addr) == null) {
					if (new CreateFunctionCmd(addr).applyTo(currentProgram, monitor)) {
						created++;
					}
				}
				if (!hasLabel(symtab, addr, name)) {
					symtab.createLabel(addr, name, SourceType.IMPORTED);
				}
				if (name.startsWith("_")) {
					String plain = name.substring(1);
					Function fn = getFunctionAt(addr);
					try {
						if (fn != null && !fn.getName().equals(plain)) {
							fn.setName(plain, SourceType.IMPORTED);
						}
						else if (fn == null && !hasLabel(symtab, addr, plain)) {
							symtab.createLabel(addr, plain, SourceType.IMPORTED).setPrimary();
						}
					}
					catch (ghidra.util.exception.DuplicateNameException e) {
						// another address already uses the plain name (folded copy)
					}
				}
				touched.add(addr);
				applied++;
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
		AutoAnalysisManager aam = AutoAnalysisManager.getAnalysisManager(currentProgram);
		Analyzer dem = aam.getAnalyzer("Demangler Microsoft");
		if (dem != null) {
			aam.scheduleOneTimeAnalysis(dem, touched);
			aam.waitForAnalysis(null, monitor);
		}
		else {
			printerr("Demangler Microsoft analyzer not found");
		}
		println("ApplyNames: " + applied + " names, " + created + " functions created");
	}

	private static boolean hasLabel(SymbolTable symtab, Address addr, String name) {
		for (Symbol s : symtab.getSymbols(addr)) {
			if (s.getName().equals(name)) {
				return true;
			}
		}
		return false;
	}
}
