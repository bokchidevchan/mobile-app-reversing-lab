import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompileCheck extends GhidraScript {
    public void run() throws Exception {
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        FunctionManager fm = currentProgram.getFunctionManager();
        for (Function f : fm.getFunctions(true)) {
            String n = f.getName();
            if (n.equals("check") || n.equals("_check")) {
                DecompileResults r = ifc.decompileFunction(f, 60, mon);
                println("===DECOMP-START===");
                println(r.getDecompiledFunction().getC());
                println("===DECOMP-END===");
            }
        }
    }
}
