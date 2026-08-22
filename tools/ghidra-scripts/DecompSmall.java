import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.util.task.ConsoleTaskMonitor;
public class DecompSmall extends GhidraScript {
    public void run() throws Exception {
        String[] names = {"116f0","1170c","11728","11744"};
        DecompInterface ifc = new DecompInterface(); ifc.openProgram(currentProgram);
        ConsoleTaskMonitor mon = new ConsoleTaskMonitor();
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            for (String n: names) if (f.getName().contains(n)) {
                DecompileResults r = ifc.decompileFunction(f, 60, mon);
                println("===FN:"+f.getName()+"===");
                println(r.getDecompiledFunction().getC());
                println("===END===");
            }
        }
    }
}
