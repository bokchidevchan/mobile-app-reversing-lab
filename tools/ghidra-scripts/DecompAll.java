import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompAll extends GhidraScript {
    public void run() throws Exception {
        DecompInterface ifc = new DecompInterface();
        ifc.openProgram(currentProgram);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        FunctionManager fm = currentProgram.getFunctionManager();
        for (Function f : fm.getFunctions(true)) {
            if (f.isThunk() || f.isExternal()) continue;
            if (f.getBody().getNumAddresses() < 20) continue;  // 자잘한 건 스킵
            DecompileResults r = ifc.decompileFunction(f, 90, mon);
            if (r.decompileCompleted()) {
                println("===FN:" + f.getName() + "===");
                println(r.getDecompiledFunction().getC());
                println("===END===");
            }
        }
    }
}
