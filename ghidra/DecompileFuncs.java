// Decompile named functions to C pseudocode without whole-program analysis.
// Usage (postScript): DecompileFuncs.java <out_dir> <name> [<name>...]
// For each name: find the symbol, disassemble from it, create the function if needed,
// then write <out_dir>/<name>.c. Works on large images imported with -noanalysis.
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;
import java.io.File;
import java.io.PrintWriter;

public class DecompileFuncs extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        File out = new File(args[0]);
        out.mkdirs();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (int i = 1; i < args.length; i++) {
            String name = args[i];
            Symbol sym = null;
            for (Symbol s : currentProgram.getSymbolTable().getSymbols(name)) {
                sym = s;
                break;
            }
            if (sym == null) {
                println("MISSING " + name);
                continue;
            }
            Address a = sym.getAddress();
            new DisassembleCommand(new AddressSet(a), null, true).applyTo(currentProgram, monitor);
            Function f = getFunctionAt(a);
            if (f == null) {
                f = createFunction(a, name);
            }
            if (f == null) {
                println("NOFUNC " + name);
                continue;
            }
            DecompileResults r = di.decompileFunction(f, 300, monitor);
            try (PrintWriter w = new PrintWriter(new File(out, name + ".c"))) {
                if (r.decompileCompleted()) {
                    w.println(r.getDecompiledFunction().getC());
                } else {
                    w.println("/* decompile failed: " + r.getErrorMessage() + " */");
                }
            }
            println("OK " + name);
        }
        di.dispose();
    }
}
