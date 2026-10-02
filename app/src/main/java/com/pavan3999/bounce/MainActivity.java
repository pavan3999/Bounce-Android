package com.pavan3999.bounce;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import java.io.InputStream;

public final class MainActivity extends Activity {
    static { System.loadLibrary("bounce"); }
    private native void nativeInit(int[] atlasPixels, byte[] levelBytes);
    private native void nativeLoadLevel(byte[] levelBytes);
    private native int[] nativeFrame();
    private native int nativeKey(int key);
    private native void nativeTouch(float x, float y, int action);

    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Bitmap frame;
    private GameView view;

    private int currentLevel = 1;

    private byte[] readAsset(String name) throws Exception {
        try (InputStream in = getAssets().open(name)) {
            byte[] data = new byte[in.available()];
            int off=0, n;
            while(off<data.length && (n=in.read(data,off,data.length-off))>0) off+=n;
            return data;
        }
    }

    @Override public void onCreate(Bundle b) {
        super.onCreate(b);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN, WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);

        try {
            Bitmap atlas=BitmapFactoryCompat.decode(getAssets(), "icons/objects_nm.png");
            int[] pixels=new int[48*72]; atlas.getPixels(pixels,0,48,0,0,48,72);
            byte[] level=readAsset("levels/J2MElvl.001");
            nativeInit(pixels,level);
        } catch(Exception e) { throw new RuntimeException(e); }

        paint.setFilterBitmap(false);
        paint.setDither(false);
        view=new GameView();
        setContentView(view);
    }

    @Override public boolean onKeyUp(int keyCode, KeyEvent event) {
        nativeKey(keyCode == KeyEvent.KEYCODE_DPAD_LEFT ? -21 :
                  keyCode == KeyEvent.KEYCODE_DPAD_RIGHT ? -22 :
                  keyCode);
        return true;
    }

    @Override public boolean onKeyDown(int keyCode, KeyEvent event) {
        int n=keyCodeToNokia(keyCode);
        int command = nativeKey(n != 0 ? n : keyCode);
        if (command != 0) {
            try {
                int next = Math.max(1, Math.min(11, currentLevel + command));
                if (next != currentLevel) {
                    nativeLoadLevel(readAsset(String.format(java.util.Locale.US, "levels/J2MElvl.%03d", next)));
                    currentLevel = next;
                }
            } catch (Exception e) { throw new RuntimeException(e); }
        }
        view.invalidate(); return true;
    }
    private int keyCodeToNokia(int k) {
        switch(k){
            case KeyEvent.KEYCODE_7:return 7; case KeyEvent.KEYCODE_8:return 8; case KeyEvent.KEYCODE_9:return 9;
            case KeyEvent.KEYCODE_1:return 1; case KeyEvent.KEYCODE_3:return 3; case KeyEvent.KEYCODE_5:return 5;
            case KeyEvent.KEYCODE_POUND:return 12;
            default:return 0;
        }
    }

    private final class GameView extends View {
        GameView(){ super(MainActivity.this); setFocusable(true); }
        @Override protected void onDraw(Canvas c) {
            super.onDraw(c);
            int[] p=nativeFrame();
            if(frame==null) frame=Bitmap.createBitmap(128,128,Bitmap.Config.ARGB_8888);
            frame.setPixels(p,0,128,0,0,128,128);
            float scale=Math.min(getWidth()/128f,getHeight()/128f);
            float dw=128*scale, dh=128*scale;
            float left=(getWidth()-dw)/2f, top=(getHeight()-dh)/2f;
            c.drawBitmap(frame,null,new android.graphics.RectF(left,top,left+dw,top+dh),paint);
            postInvalidateDelayed(40);
        }
        @Override public boolean onTouchEvent(MotionEvent e){
            float scale=Math.min(getWidth()/128f,getHeight()/128f);
            float left=(getWidth()-128f*scale)/2f;
            float top=(getHeight()-128f*scale)/2f;
            float lx=(e.getX()-left)/scale;
            float ly=(e.getY()-top)/scale;
            if (lx < 0 || lx >= 128 || ly < 0 || ly >= 128) {
                if (e.getActionMasked()==MotionEvent.ACTION_UP || e.getActionMasked()==MotionEvent.ACTION_CANCEL) nativeTouch(0,0,e.getActionMasked());
            } else {
                nativeTouch(lx,ly,e.getActionMasked());
            }
            invalidate(); return true;
        }
    }

    static final class BitmapFactoryCompat {
        static Bitmap decode(android.content.res.AssetManager am,String name) throws Exception {
            try(InputStream in=am.open(name)) { return android.graphics.BitmapFactory.decodeStream(in); }
        }
    }
}
