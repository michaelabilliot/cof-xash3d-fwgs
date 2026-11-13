// Export per-function features, vtables and data labels of the current
// program as JSON lines, for cross-version matching (tools/match_versions.py).
//
// Usage: ExportFeatures.java <out.jsonl>
//
// Records:
//   {"t":"func","ea":..,"size":..,"name":"..","thunk":ea|null,"callees":[..],
//    "strings":[..],"data":[..],"consts":[..],"ninsn":..,"nblocks":..}
//   {"t":"vtable","ea":..,"cls":"..","entries":[..]}
//   {"t":"label","ea":..,"name":"..","data":true|false}
//
// Thunks (single JMP to another function, the incremental-link ILT stubs of
// the 2012 debug builds) are resolved in callees and vtable entries.
//@category CoF

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.block.BasicBlockModel;
import ghidra.program.model.block.CodeBlockIterator;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class ExportFeatures extends GhidraScript {
	private Memory mem;

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		mem = currentProgram.getMemory();
		BasicBlockModel bbm = new BasicBlockModel(currentProgram);
		try (PrintWriter out = new PrintWriter(args[0], "UTF-8")) {
			FunctionIterator fit = currentProgram.getFunctionManager().getFunctions(true);
			while (fit.hasNext() && !monitor.isCancelled()) {
				Function f = fit.next();
				if (f.isExternal()) {
					continue;
				}
				StringBuilder sb = new StringBuilder();
				sb.append("{\"t\":\"func\",\"ea\":").append(f.getEntryPoint().getOffset());
				AddressSetView body = f.getBody();
				sb.append(",\"size\":").append(body.getNumAddresses());
				sb.append(",\"name\":").append(js(f.getName(true)));
				sb.append(",\"src\":").append(js(f.getSymbol().getSource().toString()));
				sb.append(",\"src\":").append(js(f.getSymbol().getSource().toString()));
				Address thunk = thunkTarget(f);
				sb.append(",\"thunk\":").append(thunk == null ? "null" : Long.toString(thunk.getOffset()));

				List<Long> callees = new ArrayList<>();
				List<String> strings = new ArrayList<>();
				List<Long> data = new ArrayList<>();
				List<Long> consts = new ArrayList<>();
				int ninsn = 0;
				InstructionIterator it = currentProgram.getListing().getInstructions(body, true);
				while (it.hasNext()) {
					Instruction ins = it.next();
					ninsn++;
					for (Reference r : ins.getReferencesFrom()) {
						Address to = r.getToAddress();
						if (!to.isMemoryAddress()) {
							continue;
						}
						RefType rt = r.getReferenceType();
						if (rt.isCall()) {
							Function cf = getFunctionAt(to);
							Address target = to;
							if (cf != null && thunkTarget(cf) != null) {
								target = thunkTarget(cf);
							}
							callees.add(target.getOffset());
						}
						else if (rt.isData()) {
							Data d = getDataAt(to);
							if (d != null && d.hasStringValue()) {
								Object v = d.getValue();
								strings.add(v == null ? "" : v.toString());
							}
							else if (!inText(to)) {
								data.add(to.getOffset());
							}
							else {
								Function pf = getFunctionAt(to);
								if (pf != null) {
									Address t = thunkTarget(pf);
									callees.add(-(t != null ? t : to).getOffset());
								}
							}
						}
					}
					for (int op = 0; op < ins.getNumOperands(); op++) {
						for (Object o : ins.getOpObjects(op)) {
							if (o instanceof Scalar) {
								long v = ((Scalar) o).getUnsignedValue();
								if (v > 0xFF && v < 0x10000000L || v > 0x7FFFFFFFL && v != 0xFFFFFFFFL) {
									consts.add(v);
								}
							}
						}
					}
				}
				sb.append(",\"callees\":").append(nums(callees));
				sb.append(",\"strings\":[");
				for (int i = 0; i < strings.size(); i++) {
					sb.append(i > 0 ? "," : "").append(js(strings.get(i)));
				}
				sb.append("],\"data\":").append(nums(data));
				sb.append(",\"consts\":").append(nums(consts));
				sb.append(",\"ninsn\":").append(ninsn);
				int nb = 0;
				CodeBlockIterator bit = bbm.getCodeBlocksContaining(body, monitor);
				while (bit.hasNext()) {
					bit.next();
					nb++;
				}
				sb.append(",\"nblocks\":").append(nb).append("}");
				out.println(sb);
			}

			SymbolIterator sit = currentProgram.getSymbolTable().getAllSymbols(true);
			while (sit.hasNext() && !monitor.isCancelled()) {
				Symbol s = sit.next();
				if (!s.getAddress().isMemoryAddress()) {
					continue;
				}
				String name = s.getName();
				if (name.contains("vftable")) {
					String cls = s.getParentNamespace().getName(true);
					out.println("{\"t\":\"vtable\",\"ea\":" + s.getAddress().getOffset() +
						",\"cls\":" + js(cls) + ",\"entries\":" + nums(vtableEntries(s.getAddress())) + "}");
				}
				else if (s.isPrimary() && !inText(s.getAddress()) &&
					s.getSource() != ghidra.program.model.symbol.SourceType.DEFAULT) {
					out.println("{\"t\":\"label\",\"ea\":" + s.getAddress().getOffset() +
						",\"name\":" + js(s.getName(true)) + "}");
				}
			}
		}
		println("ExportFeatures: wrote " + args[0]);
	}

	private boolean inText(Address a) {
		MemoryBlock b = mem.getBlock(a);
		return b != null && b.isExecute();
	}

	private Address thunkTarget(Function f) {
		if (f.isThunk()) {
			Function t = f.getThunkedFunction(true);
			return t == null ? null : t.getEntryPoint();
		}
		Instruction ins = getInstructionAt(f.getEntryPoint());
		if (ins != null && ins.getMnemonicString().equals("JMP") && f.getBody().getNumAddresses() <= 5) {
			Address[] flows = ins.getFlows();
			if (flows.length == 1 && getFunctionAt(flows[0]) != null) {
				return flows[0];
			}
		}
		return null;
	}

	private List<Long> vtableEntries(Address start) throws Exception {
		List<Long> out = new ArrayList<>();
		Address a = start;
		for (int i = 0; i < 400; i++) {
			if (i > 0 && getSymbolAt(a) != null && getSymbolAt(a).getName().contains("vftable")) {
				break;
			}
			if (i > 0 && getSymbolAt(a) != null && getSymbolAt(a).getName().contains("meta")) {
				break;
			}
			long p;
			try {
				p = mem.getInt(a) & 0xFFFFFFFFL;
			}
			catch (Exception e) {
				break;
			}
			Address pa = toAddr(p);
			Function f = getFunctionAt(pa);
			if (f == null || !inText(pa)) {
				break;
			}
			Address t = thunkTarget(f);
			out.add((t != null ? t : pa).getOffset());
			a = a.add(4);
		}
		return out;
	}

	private static String nums(List<Long> l) {
		StringBuilder sb = new StringBuilder("[");
		for (int i = 0; i < l.size(); i++) {
			sb.append(i > 0 ? "," : "").append(l.get(i));
		}
		return sb.append("]").toString();
	}

	private static String js(String s) {
		StringBuilder sb = new StringBuilder("\"");
		for (char c : s.toCharArray()) {
			switch (c) {
				case '"': sb.append("\\\""); break;
				case '\\': sb.append("\\\\"); break;
				case '\n': sb.append("\\n"); break;
				case '\r': sb.append("\\r"); break;
				case '\t': sb.append("\\t"); break;
				default:
					if (c < 0x20 || c > 0x7E) {
						sb.append(String.format("\\u%04x", (int) c));
					}
					else {
						sb.append(c);
					}
			}
		}
		return sb.append("\"").toString();
	}
}
