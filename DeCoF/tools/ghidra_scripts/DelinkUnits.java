// Apply a symbol map and delink translation units into COFF objects.
//
// One argument: a job file with one directive per line:
//   symbols <csv>      apply a symbol map (va,name,kind) before exporting
//   unit <out.obj>     start a unit written to out.obj
//   include <name>     add the function or data labelled <name> to the unit
//   include <name> <va>  the same for the copy of <name> at <va>: a name local
//                      to its object (a file static, tools/symbols/<dll>_local.csv)
//                      labels one address per object, and build.py tells
//                      each unit which one is its own
//
// Symbol map rules: MSVC C++ names ("?...") are added as secondary labels so
// readable primary names survive. C names ("_Foo", "_Foo@8") become the
// primary name without the underscore plus a secondary label with the full
// decorated name, which is what the delinker's MSVC symbol preference needs.
// Missing functions are created.
//
// Units are exported with the ghidra-delinker-extension: the "Relocation
// table synthesizer" analyzer runs on the unit's address set, then the
// "COFF relocatable object" exporter writes the file. Works under
// analyzeHeadless (-postScript DelinkUnits.java <job>) and through the
// GhidraMCP /run_ghidra_script endpoint.
//@category CoF

import java.io.File;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.app.plugin.core.analysis.AutoAnalysisManager;
import ghidra.app.script.GhidraScript;
import ghidra.app.services.Analyzer;
import ghidra.app.util.Option;
import ghidra.app.util.exporter.Exporter;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.Undefined;
import ghidra.program.model.data.Undefined1DataType;
import ghidra.program.model.listing.CodeUnit;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.program.model.symbol.SymbolType;
import ghidra.util.classfinder.ClassSearcher;

public class DelinkUnits extends GhidraScript {
	private static final String EXPORTER = "COFF relocatable object";
	private static final String ANALYZER = "Relocation table synthesizer";

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length != 1) {
			throw new IllegalArgumentException("usage: DelinkUnits.java <job file>");
		}
		File out = null;
		AddressSet set = null;
		// object-local names (_$E1, $LN...) listed before any other function of
		// the unit: resolved once the unit holds something to measure against
		List<String> pending = new ArrayList<>();
		int units = 0;
		for (String raw : Files.readAllLines(Paths.get(args[0]))) {
			String line = raw.strip();
			if (line.isEmpty() || line.startsWith("#")) {
				continue;
			}
			int sp = line.indexOf(' ');
			String cmd = line.substring(0, sp);
			String arg = line.substring(sp + 1).strip();
			switch (cmd) {
				case "symbols":
					dropCompilerLabels();
					useImportedNames();
					nameSecondaryVtables();
					applySymbols(arg);
					break;
				case "unit":
					if (out != null) {
						includePending(pending, set);
						exportSafe(out, set);
						units++;
					}
					out = new File(arg);
					set = new AddressSet();
					break;
				case "forget":
					forgetSymbols(arg);
					break;
				case "include":
					if (arg.indexOf(' ') > 0) {
						String[] at = arg.split("\\s+");
						try {
							set.add(findSymbolAt(at[0], toAddr(Long.parseUnsignedLong(at[1], 16))));
						}
						catch (IllegalArgumentException e) {
							printerr("DelinkUnits: skipping " + arg + " (" + e.getMessage() + ")");
						}
						if (!set.isEmpty()) {
							includePending(pending, set);
						}
						break;
					}
					if (set.isEmpty() && (arg.startsWith("$") || arg.startsWith("_$E"))) {
						pending.add(arg);
						break;
					}
					try {
						set.add(findSymbol(arg, set));
					}
					catch (IllegalArgumentException e) {
						printerr("DelinkUnits: skipping " + arg + " (" + e.getMessage() + ")");
					}
					if (!set.isEmpty()) {
						includePending(pending, set);
					}
					break;
				default:
					throw new IllegalArgumentException("unknown directive: " + cmd);
			}
		}
		if (out != null) {
			includePending(pending, set);
			exportSafe(out, set);
			units++;
		}
		println("DelinkUnits: exported " + units + " unit(s)");
	}

	private void includePending(List<String> pending, AddressSet set) {
		for (String name : pending) {
			try {
				set.add(findSymbol(name, set));
			}
			catch (IllegalArgumentException e) {
				printerr("DelinkUnits: skipping " + name + " (" + e.getMessage() + ")");
			}
		}
		pending.clear();
	}

	// A class with several vtables (multiple inheritance) gets the same
	// Ghidra label "CFoo::vftable" on each, which the COFF export refuses.
	// MSVC names each after its base subobject: ??_7CFoo@@6BCBase@@@. The
	// RTTI complete object locator before the vtable gives the subobject
	// offset, and the base class array the base at that offset.
	private void nameSecondaryVtables() {
		SymbolTable symtab = currentProgram.getSymbolTable();
		java.util.Map<String, List<Symbol>> byName = new java.util.HashMap<>();
		ghidra.program.model.symbol.SymbolIterator it = symtab.getAllSymbols(true);
		while (it.hasNext()) {
			Symbol sym = it.next();
			if (sym.getName().equals("vftable") && !sym.isExternal()) {
				byName.computeIfAbsent(sym.getName(true), k -> new ArrayList<>()).add(sym);
			}
		}
		int tx = currentProgram.startTransaction("Vtable names");
		int renamed = 0;
		try {
			for (List<Symbol> group : byName.values()) {
				if (group.size() < 2) {
					continue;
				}
				for (Symbol sym : group) {
					try {
						Address vt = sym.getAddress();
						Address col = toAddr(getInt(vt.subtract(4)) & 0xffffffffL);
						int subOffset = getInt(col.add(4));
						Address td = toAddr(getInt(col.add(12)) & 0xffffffffL);
						Address chd = toAddr(getInt(col.add(16)) & 0xffffffffL);
						int nBases = getInt(chd.add(8));
						Address bases = toAddr(getInt(chd.add(12)) & 0xffffffffL);
						String cls = readTypeName(td);
						String base = null;
						for (int i = 1; i < nBases && base == null; i++) {
							Address bcd = toAddr(getInt(bases.add(4 * i)) & 0xffffffffL);
							if (getInt(bcd.add(8)) == subOffset) {
								base = readTypeName(toAddr(getInt(bcd) & 0xffffffffL));
							}
						}
						if (cls == null || base == null) {
							continue;
						}
						String name = "??_7" + cls + "6B" + base + "@";
						sym.setName(name, SourceType.ANALYSIS);
						sym.setNamespace(currentProgram.getGlobalNamespace());
						renamed++;
					}
					catch (Exception e) {
						printerr("DelinkUnits: vtable at " + sym.getAddress() + ": " + e.getMessage());
					}
				}
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
		println("DelinkUnits: " + renamed + " vtables of multiple inheritance classes named");
	}

	// ".?AVCVoiceStatus@@" -> "CVoiceStatus@@"
	private String readTypeName(Address td) throws Exception {
		StringBuilder sb = new StringBuilder();
		Address a = td.add(8);
		for (int i = 0; i < 256; i++) {
			byte b = getByte(a.add(i));
			if (b == 0) {
				break;
			}
			sb.append((char) b);
		}
		String n = sb.toString();
		return n.startsWith(".?AV") || n.startsWith(".?AU") ? n.substring(4) : null;
	}

	// Ghidra names imports by their demangled name: two overloads of one
	// imported method (vgui::Label::Label) then share a name and the COFF
	// export refuses the unit. Give every import its original (decorated)
	// imported name.
	private void useImportedNames() {
		int tx = currentProgram.startTransaction("Imported names");
		int renamed = 0;
		try {
			ghidra.program.model.symbol.ExternalManager em = currentProgram.getExternalManager();
			// imports in nested namespaces (VGUI.DLL::vgui::Label) are not
			// direct children of the library: walk every external symbol
			List<Symbol> ext = new ArrayList<>();
			currentProgram.getSymbolTable().getExternalSymbols().forEach(ext::add);
			for (Symbol sym : ext) {
				ghidra.program.model.symbol.ExternalLocation loc = em.getExternalLocation(sym);
				if (loc == null) {
					continue;
				}
				String orig = loc.getOriginalImportedName();
				if (orig == null || orig.equals(loc.getLabel())) {
					continue;
				}
				try {
					loc.setName(em.getExternalLibrary(loc.getLibraryName()), orig, SourceType.IMPORTED);
					renamed++;
				}
				catch (Exception e) {
					// a name already taken: leave this one
				}
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
		println("DelinkUnits: " + renamed + " imports given their decorated names");
	}

	// $LN labels are local jump table labels of our objects; on the retail
	// side they came from earlier harvests and split a function from its
	// own switch tables (which are now part of the function): remove all
	private void dropCompilerLabels() throws Exception {
		SymbolTable symtab = currentProgram.getSymbolTable();
		List<Symbol> labels = new ArrayList<>();
		for (Symbol s : symtab.getAllSymbols(false)) {
			if (s.getName().startsWith("$LN") && s.getName().substring(3).matches("[0-9]+")
					&& s.getSymbolType() == SymbolType.LABEL) {
				labels.add(s);
			}
		}
		int tx = currentProgram.startTransaction("Drop $LN labels");
		try {
			for (Symbol s : labels) {
				s.delete();
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
		println("DelinkUnits: dropped " + labels.size() + " $LN labels");
	}

	// Remove names that the symbol map dropped at an address (CSV va,name):
	// labels are deleted, a function carrying the name gets its default name
	private void forgetSymbols(String csv) throws Exception {
		if (!new File(csv).exists()) {
			return;
		}
		SymbolTable symtab = currentProgram.getSymbolTable();
		int tx = currentProgram.startTransaction("Forget CoF names");
		int n = 0;
		try {
			for (String raw : Files.readAllLines(Paths.get(csv))) {
				String line = raw.strip();
				if (line.isEmpty() || line.startsWith("va,")) {
					continue;
				}
				String[] f = line.split(",", -1);
				Address addr = toAddr(Long.parseUnsignedLong(f[0], 16));
				String name = f[1];
				String plain = name.startsWith("_") ? name.substring(1) : name;
				List<Symbol> here = new ArrayList<>();
				for (Symbol s : symtab.getSymbols(addr)) {
					here.add(s);
				}
				for (Symbol s : here) {
					// a namespaced label (CSliderCtrl::SetRange, a Ghidra signature
					// name) is written with its full path
					if (!s.getName().equals(name) && !s.getName().equals(plain) && !s.getName(true).equals(name)) {
						continue;
					}
					Function fn = getFunctionAt(addr);
					try {
						if (fn != null && fn.getSymbol().equals(s)) {
							fn.setName(null, SourceType.DEFAULT);
						}
						else {
							s.delete();
						}
						n++;
					}
					catch (Exception e) {
						printerr("could not forget " + name + " at " + addr);
					}
				}
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
		println("DelinkUnits: forgot " + n + " names");
	}

	private void applySymbols(String csv) throws Exception {
		SymbolTable symtab = currentProgram.getSymbolTable();
		int tx = currentProgram.startTransaction("Apply CoF symbol map");
		int applied = 0;
		// the primary name the map gives each address: C names lose their
		// leading "_" ("__PM_PlayerTrace" is the C function _PM_PlayerTrace,
		// whose primary name "_PM_PlayerTrace" is also how "_PM_PlayerTrace"
		// the decorated C name PM_PlayerTrace is spelled), so a stale-copy
		// cleanup must not clear a name the map puts at that other address
		Map<Address, String> mapPrimary = new HashMap<>();
		Map<Address, String> mapFull = new HashMap<>();
		for (String raw : Files.readAllLines(Paths.get(csv))) {
			String[] f = raw.strip().split(",", -1);
			if (f.length < 2 || f[0].isEmpty() || f[0].startsWith("va") || f[0].startsWith("#")) {
				continue;
			}
			mapFull.put(toAddr(Long.parseUnsignedLong(f[0], 16)), f[1]);
			mapPrimary.put(toAddr(Long.parseUnsignedLong(f[0], 16)),
				f[1].startsWith("_") ? f[1].substring(1) : f[1]);
		}
		try {
			for (String raw : Files.readAllLines(Paths.get(csv))) {
				String line = raw.strip();
				if (line.isEmpty() || line.startsWith("#") || line.startsWith("va,")) {
					continue;
				}
				String[] f = line.split(",", -1);
				Address addr = toAddr(Long.parseUnsignedLong(f[0], 16));
				String name = f[1];
				String kind = f.length > 2 ? f[2] : "func";

				if (kind.equals("func") && getFunctionAt(addr) == null) {
					if (!new CreateFunctionCmd(addr).applyTo(currentProgram, monitor)) {
						printerr("could not create function at " + addr);
					}
				}
				// a name lives at one address: drop stale copies elsewhere (the
				// exporter refuses duplicate names), for the decorated name and
				// for the plain primary name of C symbols
				// ($LN jump table labels and the VC6 dynamic initializers _$E1,
				// _$E2, ... are local to their object: the same name may label
				// several addresses)
				String[] spellings = name.startsWith("$") || name.startsWith("_$E") ? new String[0]
						: name.startsWith("_") ? new String[] { name, name.substring(1) }
						: new String[] { name };
				for (String spelling : spellings) {
					List<Symbol> stale = new ArrayList<>();
					symtab.getSymbols(spelling).forEach(stale::add);
					for (Symbol other : stale) {
						if (other.getAddress().equals(addr) || other.getSource() == SourceType.DEFAULT
								|| spelling.equals(mapPrimary.get(other.getAddress()))
								|| spelling.equals(mapFull.get(other.getAddress()))) {
							continue;
						}
						Function ofn = getFunctionAt(other.getAddress());
						try {
							if (ofn != null && ofn.getSymbol().equals(other)) {
								ofn.setName(null, SourceType.DEFAULT);
							}
							else {
								other.delete();
							}
						}
						catch (Exception e) {
							printerr("could not clear " + spelling + " at " + other.getAddress());
						}
					}
				}
				try {
					Function fn = getFunctionAt(addr);
					if (name.startsWith("_")) {
						// the delinker looks for "_" + primary name
						makePrimary(symtab, fn, addr, name.substring(1));
					}
					else if (!name.startsWith("?")) {
						// "@name@n" (fastcall), "$LN" jump table labels and other
						// names are emitted as the primary name itself
						makePrimary(symtab, fn, addr, name);
					}
					if (!hasLabel(symtab, addr, name)) {
						symtab.createLabel(addr, name, SourceType.USER_DEFINED);
					}
				}
				catch (ghidra.util.exception.DuplicateNameException e) {
					// name already used by a folded copy or applied earlier
				}
				// the delinker prefers a "?" label: drop C++ names left at this
				// address by earlier, wrong naming passes
				Function cur = getFunctionAt(addr);
				// a function still named with an older, wrong C++ name takes the map's
				if (cur != null && name.startsWith("?") && cur.getName().startsWith("?") && !cur.getName().equals(name)) {
					try {
						for (Symbol s : symtab.getSymbols(addr)) {
							if (s.getName().equals(name) && !s.equals(cur.getSymbol())) {
								s.delete();
								break;
							}
						}
						cur.setName(name, SourceType.USER_DEFINED);
					}
					catch (Exception e) {
						printerr("could not rename " + cur.getName() + " to " + name);
					}
				}
				List<Symbol> here = new ArrayList<>();
				for (Symbol s : symtab.getSymbols(addr)) {
					here.add(s);
				}
				for (Symbol s : here) {
					if (s.getName().startsWith("?") && !s.getName().equals(name) && !s.isDynamic()
							&& (cur == null || !cur.getSymbol().equals(s))) {
						s.delete();
					}
				}
				if (kind.equals("data") && f.length > 3 && !f[3].isEmpty()) {
					defineExtent(symtab, addr, Integer.parseInt(f[3]));
				}
				applied++;
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
		println("DelinkUnits: " + applied + " symbols applied from " + csv);
	}

	// Define a named global as undefined1[size] so that references into its
	// middle are offsets from its symbol. The extent stops before the next
	// named symbol; ranges holding instructions or typed data (pointers,
	// strings: their references must survive) are left alone.
	private void defineExtent(SymbolTable symtab, Address addr, int size) {
		if (size <= 1) {
			return;
		}
		Listing listing = currentProgram.getListing();
		Address end = addr.add(size - 1);
		Data first = listing.getDataAt(addr);
		if (first != null && first.getLength() >= size) {
			return;
		}
		for (Symbol s : symtab.getSymbols(new AddressSet(addr.add(1), end), SymbolType.LABEL, true)) {
			if (!s.isDynamic() && s.getSource() != SourceType.DEFAULT) {
				end = s.getAddress().subtract(1);
				break;
			}
		}
		if (end.compareTo(addr) <= 0) {
			return;
		}
		AddressSet range = new AddressSet(addr, end);
		if (listing.getInstructions(range, true).hasNext()) {
			return;
		}
		// a zero-initialized global (CHud gHUD) stores no pointers or strings:
		// the types Ghidra guessed inside it carry no references to keep
		boolean zeroFilled = allZero(addr, end);
		for (Data d : listing.getDefinedData(range, true)) {
			boolean undefinedType = Undefined.isUndefined(d.getDataType()) || d.getDataType() instanceof ArrayDataType
					&& Undefined.isUndefined(((ArrayDataType) d.getDataType()).getDataType());
			// a typed item without references (a float of an initialized
			// table such as player_maxs) has nothing to keep either
			boolean noRefs = !currentProgram.getReferenceManager()
					.getReferenceSourceIterator(new AddressSet(d.getMinAddress(), d.getMaxAddress()), true)
					.hasNext();
			if (!d.getMinAddress().equals(addr) && !undefinedType && !zeroFilled && !noRefs) {
				return;
			}
			// an item running past the end: only one without references can
			// go (a "string" Ghidra found from the last byte of a float table)
			if (d.getMaxAddress().compareTo(end) > 0 && !noRefs) {
				return;
			}
		}
		try {
			clearListing(addr, end);
			int n = (int) end.subtract(addr) + 1;
			createData(addr, new ArrayDataType(Undefined1DataType.dataType, n, 1));
		}
		catch (Exception e) {
			printerr("could not define extent at " + addr + ": " + e.getMessage());
		}
	}

	private boolean allZero(Address from, Address to) {
		try {
			int n = (int) to.subtract(from) + 1;
			byte[] b = new byte[n];
			if (currentProgram.getMemory().getBytes(from, b) != n) {
				return true; // uninitialized memory reads short
			}
			for (byte x : b) {
				if (x != 0) {
					return false;
				}
			}
			return true;
		}
		catch (Exception e) {
			return true; // uninitialized block
		}
	}

	private static void makePrimary(SymbolTable symtab, Function fn, Address addr, String primary)
			throws Exception {
		if (fn != null) {
			if (!fn.getName().equals(primary)) {
				// a secondary label with that name would make the rename fail
				for (Symbol s : symtab.getSymbols(addr)) {
					if (s.getName().equals(primary) && !s.equals(fn.getSymbol())) {
						s.delete();
						break;
					}
				}
				fn.setName(primary, SourceType.USER_DEFINED);
			}
			return;
		}
		for (Symbol s : symtab.getSymbols(addr)) {
			if (s.getName().equals(primary)) {
				if (!s.isPrimary()) {
					s.setPrimary();
				}
				return;
			}
		}
		symtab.createLabel(addr, primary, SourceType.USER_DEFINED).setPrimary();
	}

	private static boolean hasLabel(SymbolTable symtab, Address addr, String name) {
		for (Symbol s : symtab.getSymbols(addr)) {
			if (s.getName().equals(name)) {
				return true;
			}
		}
		return false;
	}

	private AddressSet findSymbol(String name, AddressSet unit) {
		// C names are stored without their leading underscore as primary,
		// and with it as a secondary label; either spelling resolves.
		SymbolTable symtab = currentProgram.getSymbolTable();
		List<Symbol> found = new ArrayList<>();
		symtab.getSymbols(name).forEach(found::add);
		if (found.isEmpty()) {
			throw new IllegalArgumentException("symbol not found: " + name);
		}
		// a name local to its object (_$E2) labels one address per object:
		// take the one next to what the unit already holds
		if (found.size() > 1 && (name.startsWith("$") || name.startsWith("_$E")) && !unit.isEmpty()) {
			Symbol best = null;
			long bestDist = Long.MAX_VALUE;
			for (Symbol s : found) {
				long d = Math.min(Math.abs(s.getAddress().subtract(unit.getMinAddress())),
					Math.abs(s.getAddress().subtract(unit.getMaxAddress())));
				if (d < bestDist) {
					bestDist = d;
					best = s;
				}
			}
			found.clear();
			found.add(best);
		}
		return symbolExtent(found, name);
	}

	// the copy of an object-local name (a file static that several objects
	// define, tools/symbols/<dll>_local.csv) at the address the job gives
	private AddressSet findSymbolAt(String name, Address addr) {
		List<Symbol> found = new ArrayList<>();
		for (Symbol s : currentProgram.getSymbolTable().getSymbols(name)) {
			if (s.getAddress().equals(addr)) {
				found.add(s);
				break;
			}
		}
		if (found.isEmpty()) {
			throw new IllegalArgumentException("symbol not found at " + addr);
		}
		return symbolExtent(found, name);
	}

	private AddressSet symbolExtent(List<Symbol> found, String name) {
		AddressSet set = new AddressSet();
		for (Symbol s : found) {
			Function fn = getFunctionAt(s.getAddress());
			if (fn != null) {
				set.add(fn.getBody());
				addAlignmentGaps(set, fn);
				addDestructorPadding(set, fn, name);
				addTrailingTables(set, fn);
			}
			else {
				CodeUnit cu = currentProgram.getListing().getCodeUnitAt(s.getAddress());
				set.add(cu.getMinAddress(), cu.getMaxAddress());
			}
		}
		return set;
	}

	// alignment padding inside a function (a multi-byte nop before a loop
	// head) is not reached by any flow, so Ghidra leaves it out of the body;
	// MSVC counts it as part of the function: fill holes of up to 15 bytes
	private void addAlignmentGaps(AddressSet set, Function fn) {
		Address prevEnd = null;
		for (ghidra.program.model.address.AddressRange r : fn.getBody().getAddressRanges()) {
			if (prevEnd != null) {
				long gap = r.getMinAddress().subtract(prevEnd) - 1;
				if (gap > 0 && gap < 16) {
					set.add(prevEnd.add(1), r.getMinAddress().subtract(1));
				}
			}
			prevEnd = r.getMaxAddress();
		}
		// a function that ends in a call to a no-return function (the /GS
		// range check failure) is followed by an int3 that MSVC counts as
		// part of the function
		Instruction last = currentProgram.getListing().getInstructionContaining(fn.getBody().getMaxAddress());
		if (last != null && last.getFlowType().isCall()) {
			for (Address target : last.getFlows()) {
				Function callee = getFunctionAt(target);
				if (callee != null && callee.hasNoReturn()) {
					Address next = last.getMaxAddress().add(1);
					try {
						if (getByte(next) == (byte) 0xCC && getFunctionAt(next) == null) {
							set.add(next, next);
						}
					}
					catch (Exception e) {
						// end of the block
					}
				}
			}
		}
	}

	// MSVC 19.27 ends a destructor that has an EH frame (it is noexcept, so
	// an exception that escapes it must reach terminate) with 5 bytes of
	// int3 inside its own COMDAT; the linker fill comes after them
	private void addDestructorPadding(AddressSet set, Function fn, String name) {
		if (!name.startsWith("??1")) {
			return;
		}
		try {
			byte[] head = getBytes(fn.getEntryPoint(), 6);
			byte[] eh = { 0x55, (byte) 0x8B, (byte) 0xEC, 0x6A, (byte) 0xFF, 0x68 };
			if (!java.util.Arrays.equals(head, eh)) {
				return;
			}
			Address next = fn.getBody().getMaxAddress().add(1);
			for (int i = 0; i < 5; i++) {
				if (getByte(next.add(i)) != (byte) 0xCC || getFunctionAt(next.add(i)) != null) {
					return;
				}
			}
			set.add(next, next.add(4));
		}
		catch (Exception e) {
			// end of the block
		}
	}

	// hl.dll is a DLL: every absolute address in its code has a base
	// relocation. A data reference from an instruction that has no base
	// relocation is an immediate that only looks like an address (the
	// flag 0x10000000 equals the image base): drop it, or the relocation
	// synthesizer turns the constant into a relocation.
	private void dropFalseAddressReferences(AddressSet set) {
		ghidra.program.model.reloc.RelocationTable relocs = currentProgram.getRelocationTable();
		if (relocs.getSize() == 0) {
			return;
		}
		int tx = currentProgram.startTransaction("Drop false address references");
		try {
			for (Instruction ins : currentProgram.getListing().getInstructions(set, true)) {
				Reference[] refs = ins.getReferencesFrom();
				boolean relocated = false;
				for (int k = 0; k < ins.getLength() && !relocated; k++) {
					relocated = !relocs.getRelocations(ins.getAddress().add(k)).isEmpty();
				}
				if (relocated) {
					continue;
				}
				for (Reference r : refs) {
					if (!r.getReferenceType().isFlow() && r.isMemoryReference() && r.getOperandIndex() >= 0) {
						currentProgram.getReferenceManager().delete(r);
					}
				}
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
	}

	// MSVC emits the jump tables of a switch (the dword target table and the
	// byte index table) right after the function, inside its COMDAT; Ghidra
	// keeps them out of the function body. Add the bytes between the body
	// and the end of the last data the function references there, up to the
	// next function, so the table is part of the function like in our object.
	private void addTrailingTables(AddressSet set, Function fn) {
		Address end = fn.getBody().getMaxAddress();
		Function next = getFunctionAfter(end);
		if (next == null) {
			return;
		}
		Address limit = next.getEntryPoint();
		Address last = null;
		Listing listing = currentProgram.getListing();
		for (Address from : fn.getBody().getAddresses(true)) {
			for (Reference r : currentProgram.getReferenceManager().getReferencesFrom(from)) {
				Address to = r.getToAddress();
				// only data reads (the table load of the switch jump), never
				// calls or jumps into code Ghidra left outside any function
				if (!r.getReferenceType().isData() || listing.getInstructionContaining(to) != null) {
					continue;
				}
				if (to.compareTo(end) > 0 && to.compareTo(limit) < 0) {
					CodeUnit cu = listing.getCodeUnitContaining(to);
					Address max = cu != null ? cu.getMaxAddress() : to;
					if (max.compareTo(limit) >= 0) {
						max = limit.subtract(1);
					}
					if (last == null || max.compareTo(last) > 0) {
						last = max;
					}
				}
			}
		}
		if (last == null) {
			return;
		}
		// the referenced item is only the start of a table (Ghidra defines
		// the entries one by one): take everything up to the next function
		// except the int3 fill the linker puts between functions
		Address stop = limit.subtract(1);
		try {
			while (stop.compareTo(last) > 0 && getByte(stop) == (byte) 0xCC) {
				stop = stop.subtract(1);
			}
		}
		catch (Exception e) {
			stop = last;
		}
		set.add(end.add(1), stop);
		// the byte index table of a two-level switch is read with
		// movzx r, byte ptr [r + table], an operand Ghidra often leaves
		// without a reference: add it, or the relocation synthesizer keeps
		// the raw absolute address
		int tx = currentProgram.startTransaction("Switch table references");
		try {
			for (Instruction ins : listing.getInstructions(fn.getBody(), true)) {
				for (int op = 0; op < ins.getNumOperands(); op++) {
					for (Object o : ins.getOpObjects(op)) {
						if (!(o instanceof Scalar)) {
							continue;
						}
						long v = ((Scalar) o).getUnsignedValue();
						Address to;
						try {
							to = toAddr(v);
						}
						catch (Exception e) {
							continue;
						}
						if (to.compareTo(end) <= 0 || to.compareTo(stop) > 0) {
							continue;
						}
						// the switch analysis may have referenced the table
						// minus the index bias (after dec eax): the synthesizer
						// only relocates a primary reference that equals the
						// encoded address
						Reference exact = null;
						for (Reference r : ins.getOperandReferences(op)) {
							if (r.getToAddress().equals(to)) {
								exact = r;
							}
						}
						if (exact == null) {
							exact = currentProgram.getReferenceManager().addMemoryReference(ins.getAddress(), to,
								RefType.DATA, SourceType.ANALYSIS, op);
						}
						if (!exact.isPrimary()) {
							currentProgram.getReferenceManager().setPrimary(exact, true);
						}
						// the biased reference would be exported as an offcut
						// label inside the table ("PTR_...+3") or inside a
						// case block ("caseD_4+1"), which objdiff cannot
						// handle: drop it
						for (Reference r : ins.getOperandReferences(op)) {
							Address a = r.getToAddress();
							if (!a.equals(to) && a.compareTo(fn.getEntryPoint()) >= 0
									&& a.compareTo(stop) <= 0) {
								currentProgram.getReferenceManager().delete(r);
							}
						}
					}
				}
			}
		}
		finally {
			currentProgram.endTransaction(tx, true);
		}
	}

	// a failing unit must not abort the script: that would roll back the
	// symbol changes of the whole run
	private void exportSafe(File file, AddressSet set) {
		try {
			export(file, set);
		}
		catch (Exception e) {
			printerr("DelinkUnits: " + e.getMessage());
		}
	}

	private void export(File file, AddressSet set) throws Exception {
		dropFalseAddressReferences(set);
		AutoAnalysisManager aam = AutoAnalysisManager.getAnalysisManager(currentProgram);
		Analyzer analyzer = aam.getAnalyzer(ANALYZER);
		if (analyzer == null) {
			throw new IllegalStateException("analyzer not found: " + ANALYZER);
		}
		aam.scheduleOneTimeAnalysis(analyzer, set);
		aam.waitForAnalysis(null, monitor);

		Exporter exporter = null;
		for (Exporter e : ClassSearcher.getInstances(Exporter.class)) {
			if (e.getName().equals(EXPORTER)) {
				exporter = e;
			}
		}
		if (exporter == null) {
			throw new IllegalStateException("exporter not found: " + EXPORTER);
		}
		List<Option> options = new ArrayList<>(exporter.getOptions(() -> currentProgram));
		exporter.setOptions(options);
		file.getParentFile().mkdirs();
		if (!exporter.export(file, currentProgram, set, monitor)) {
			throw new RuntimeException("failed to export " + file + ": " + exporter.getMessageLog());
		}
	}
}
