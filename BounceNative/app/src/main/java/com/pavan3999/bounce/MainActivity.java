package com.pavan3999.bounce;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

public final class MainActivity extends Activity {
    static { System.loadLibrary("bounce"); }
    private native void nativeKey(int key);
    private native void nativeInit();
    private native void nativeTouch(float x, float y, int action);

    @Override public void onCreate(Bundle b) {
        super.onCreate(b);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        View v = new View(this) {
            @Override public boolean onTouchEvent(android.view.MotionEvent e) {
                nativeTouch(e.getX(), e.getY(), e.getActionMasked()); return true;
            }
        };
        setContentView(v);
        nativeInit();
    }
    @Override public boolean onKeyDown(int keyCode, KeyEvent event) { nativeKey(event.getKeyCode()); return true; }
}
