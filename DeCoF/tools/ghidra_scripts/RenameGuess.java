// Rename every guess_ prefixed symbol to the _guess suffix (tools/rename_guess.py).
// guess_X -> X_guess, _guess_X -> _X_guess, and the same inside decorated
// names (?guess_Foo@CBar@@... -> ?Foo_guess@CBar@@...). Saves the program.
//@category CoF

import ghidra.app.script.GhidraScript;
import ghidra.program.model.symbol.*;
import java.util.*;
import java.util.regex.*;

public class RenameGuess extends GhidraScript {
	static final Pattern PAT = Pattern.compile("(?<![a-z0-9_])(_?)guess_([A-Za-z0-9_]+)");
	static final Pattern PAT2 = Pattern.compile("(__CmdFunc_|__MsgFunc_|\\?\\?[0-9])guess_([A-Za-z0-9_]+)");

	@Override
	protected void run() throws Exception {
		SymbolTable st = currentProgram.getSymbolTable();
		List<Symbol> todo = new ArrayList<>();
		SymbolIterator it = st.getAllSymbols(true);
		while (it.hasNext()) {
			Symbol s = it.next();
			if (s.getName().contains("guess_")) todo.add(s);
		}
		int n = 0, fail = 0;
		for (Symbol s : todo) {
			String name0 = PAT2.matcher(s.getName()).replaceAll("$1$2_guess");
			Matcher m = PAT.matcher(name0);
			StringBuffer sb = new StringBuffer();
			while (m.find()) m.appendReplacement(sb, Matcher.quoteReplacement(m.group(1) + m.group(2) + "_guess"));
			m.appendTail(sb);
			String nn = sb.toString();
			if (nn.equals(s.getName())) continue;
			try {
				s.setName(nn, s.getSource());
				n++;
			} catch (Exception e) {
				fail++;
			}
		}
		println("RenameGuess: " + n + " renamed, " + fail + " failed, " + todo.size() + " with guess_");
	}
}
