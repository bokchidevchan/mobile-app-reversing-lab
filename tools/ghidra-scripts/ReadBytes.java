import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class ReadBytes extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long addr = Long.decode(a[0]);
        int n = Integer.decode(a[1]);
        Address ad = currentProgram.getImageBase().getNewAddress(addr);
        byte[] b = new byte[n];
        currentProgram.getMemory().getBytes(ad, b);
        StringBuilder sb = new StringBuilder("BYTES:");
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        println(sb.toString());
    }
}
