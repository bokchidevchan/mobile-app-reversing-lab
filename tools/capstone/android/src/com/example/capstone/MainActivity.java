package com.example.capstone;
import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
public class MainActivity extends Activity {
    static { System.loadLibrary("verify"); }
    public native boolean check(String s);
    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        boolean ok = check("test");
        Log.i("CAPSTONE", "check(\"test\")=" + ok);
    }
}
