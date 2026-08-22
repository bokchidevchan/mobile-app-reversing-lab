import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompFunc extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        String want = (a.length > 0) ? a[0] : "bar";
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        FunctionManager fm = currentProgram.getFunctionManager();
        for (Function f : fm.getFunctions(true)) {
            if (f.getName().contains(want)) {
                DecompileResults r = ifc.decompileFunction(f, 90, mon);
                println("===FN:" + f.getName() + "===");
                println(r.getDecompiledFunction().getC());
                println("===END===");
            }
        }
    }
}
