package com.pavan3999.bounce;

import android.app.Activity;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.Window;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Color;
import android.graphics.RectF;
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

    private static final int SCREEN_MENU = 0;
    private static final int SCREEN_GAME = 1;
    private static final int SCREEN_HIGH_SCORE = 2;
    private static final int SCREEN_INSTRUCTIONS = 3;
    private static final int SCREEN_CHEATS = 4;

    private int screen = SCREEN_MENU;
    private int menuSelection = 0;

    private boolean overflowOpen = false;
    private boolean leftPressed = false;
    private boolean rightPressed = false;

    private final StringBuilder cheatInput = new StringBuilder();

    private byte[] readAsset(String name) throws Exception {
        try (InputStream in = getAssets().open(name)) {
            byte[] data = new byte[in.available()];
            int off = 0;
            int n;

            while (off < data.length &&
                    (n = in.read(data, off, data.length - off)) > 0) {
                off += n;
            }

            return data;
        }
    }

    @Override public void onCreate(Bundle b) {
        super.onCreate(b);

        requestWindowFeature(Window.FEATURE_NO_TITLE);

        getWindow().setStatusBarColor(Color.BLACK);
        getWindow().setNavigationBarColor(Color.rgb(38, 48, 58));

        showMenuBars();

        try {
            Bitmap atlas = BitmapFactoryCompat.decode(
                    getAssets(),
                    "icons/objects_nm.png"
            );

            int[] pixels = new int[48 * 72];
            atlas.getPixels(pixels, 0, 48, 0, 0, 48, 72);

            byte[] level = readAsset("levels/J2MElvl.001");

            nativeInit(pixels, level);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        paint.setFilterBitmap(false);
        paint.setDither(false);

        view = new GameView();
        setContentView(view);
    }

    private void showMenuBars() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    private void showGameFullscreen() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        );
    }

    private void enterGame() {
        overflowOpen = false;
        screen = SCREEN_GAME;
        showGameFullscreen();
        view.invalidate();
    }

    private void enterMenu() {
        screen = SCREEN_MENU;
        overflowOpen = false;
        leftPressed = false;
        rightPressed = false;
        showMenuBars();
        view.invalidate();
    }

    private void startNewGame() {
        try {
            nativeLoadLevel(readAsset(
                    "levels/J2MElvl.001"
            ));
            currentLevel = 1;
            enterGame();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    private void loadLevel(int level) {
        try {
            level = Math.max(1, Math.min(11, level));

            nativeLoadLevel(readAsset(
                    String.format(
                            java.util.Locale.US,
                            "levels/J2MElvl.%03d",
                            level
                    )
            ));

            currentLevel = level;
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Override public void onBackPressed() {
        if (screen == SCREEN_GAME) {
            enterMenu();
            return;
        }

        if (screen == SCREEN_CHEATS ||
                screen == SCREEN_HIGH_SCORE ||
                screen == SCREEN_INSTRUCTIONS) {
            enterMenu();
            return;
        }

        if (overflowOpen) {
            overflowOpen = false;
            view.invalidate();
            return;
        }

        super.onBackPressed();
    }

    @Override public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (screen == SCREEN_GAME) {
            nativeKey(
                    keyCode == KeyEvent.KEYCODE_DPAD_LEFT ? -21 :
                    keyCode == KeyEvent.KEYCODE_DPAD_RIGHT ? -22 :
                    keyCode
            );
        }

        return true;
    }

    @Override public boolean onKeyDown(int keyCode, KeyEvent event) {

        if (screen == SCREEN_MENU) {
            if (overflowOpen) {
                if (keyCode == KeyEvent.KEYCODE_DPAD_UP ||
                        keyCode == KeyEvent.KEYCODE_DPAD_DOWN) {
                    overflowOpen = false;
                    view.invalidate();
                    return true;
                }

                if (keyCode == KeyEvent.KEYCODE_ENTER ||
                        keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
                    screen = SCREEN_CHEATS;
                    cheatInput.setLength(0);
                    overflowOpen = false;
                    view.invalidate();
                    return true;
                }
            }

            if (keyCode == KeyEvent.KEYCODE_DPAD_UP) {
                menuSelection--;
                if (menuSelection < 0)
                    menuSelection = 3;

                view.invalidate();
                return true;
            }

            if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) {
                menuSelection++;
                if (menuSelection > 3)
                    menuSelection = 0;

                view.invalidate();
                return true;
            }

            if (keyCode == KeyEvent.KEYCODE_ENTER ||
                    keyCode == KeyEvent.KEYCODE_DPAD_CENTER ||
                    keyCode == KeyEvent.KEYCODE_SPACE) {
                activateMenuItem();
                return true;
            }

            if (keyCode == KeyEvent.KEYCODE_MENU) {
                overflowOpen = true;
                view.invalidate();
                return true;
            }
        }

        if (screen == SCREEN_CHEATS) {
            int digit = keyCodeToDigit(keyCode);

            if (digit >= 0) {
                appendCheatDigit((char) ('0' + digit));
                view.invalidate();
                return true;
            }

            if (keyCode == KeyEvent.KEYCODE_DEL) {
                if (cheatInput.length() > 0)
                    cheatInput.deleteCharAt(cheatInput.length() - 1);

                view.invalidate();
                return true;
            }

            if (keyCode == KeyEvent.KEYCODE_ENTER ||
                    keyCode == KeyEvent.KEYCODE_DPAD_CENTER) {
                submitCheat();
                return true;
            }
        }

        if (screen == SCREEN_GAME) {
            int n = keyCodeToNokia(keyCode);

            int command = nativeKey(
                    n != 0 ? n : keyCode
            );

            if (command != 0) {
                loadLevel(currentLevel + command);
            }

            view.invalidate();
            return true;
        }

        return true;
    }

    private void activateMenuItem() {
        switch (menuSelection) {
            case 0:
                enterGame();
                break;

            case 1:
                startNewGame();
                break;

            case 2:
                screen = SCREEN_HIGH_SCORE;
                view.invalidate();
                break;

            case 3:
                screen = SCREEN_INSTRUCTIONS;
                view.invalidate();
                break;
        }
    }

    private int keyCodeToDigit(int k) {
        switch (k) {
            case KeyEvent.KEYCODE_0: return 0;
            case KeyEvent.KEYCODE_1: return 1;
            case KeyEvent.KEYCODE_2: return 2;
            case KeyEvent.KEYCODE_3: return 3;
            case KeyEvent.KEYCODE_4: return 4;
            case KeyEvent.KEYCODE_5: return 5;
            case KeyEvent.KEYCODE_6: return 6;
            case KeyEvent.KEYCODE_7: return 7;
            case KeyEvent.KEYCODE_8: return 8;
            case KeyEvent.KEYCODE_9: return 9;
            default: return -1;
        }
    }

    private void appendCheatDigit(char digit) {
        if (cheatInput.length() < 12)
            cheatInput.append(digit);
    }

    private void submitCheat() {
        String code = cheatInput.toString();

        if ("787898".equals(code)) {
            nativeKey(8);
        } else if ("787899".equals(code)) {
            nativeKey(9);
        }

        cheatInput.setLength(0);
        enterGame();
    }

    private int keyCodeToNokia(int k) {
        switch (k) {
            case KeyEvent.KEYCODE_7: return 7;
            case KeyEvent.KEYCODE_8: return 8;
            case KeyEvent.KEYCODE_9: return 9;
            case KeyEvent.KEYCODE_1: return 1;
            case KeyEvent.KEYCODE_3: return 3;
            case KeyEvent.KEYCODE_5: return 5;
            case KeyEvent.KEYCODE_POUND: return 12;
            default: return 0;
        }
    }

    private final class GameView extends View {
        GameView() {
            super(MainActivity.this);
            setFocusable(true);
        }

        @Override protected void onDraw(Canvas c) {
            super.onDraw(c);

            if (screen == SCREEN_MENU) {
                drawMainMenu(c);
                postInvalidateDelayed(40);
                return;
            }

            if (screen == SCREEN_HIGH_SCORE) {
                drawSimpleScreen(
                        c,
                        "High score",
                        "No saved high scores yet."
                );
                postInvalidateDelayed(40);
                return;
            }

            if (screen == SCREEN_INSTRUCTIONS) {
                drawInstructions(c);
                postInvalidateDelayed(40);
                return;
            }

            if (screen == SCREEN_CHEATS) {
                drawCheatScreen(c);
                postInvalidateDelayed(40);
                return;
            }

            drawGame(c);
            postInvalidateDelayed(40);
        }

        private void drawMainMenu(Canvas c) {
            c.drawColor(Color.BLACK);

            float d = getResources().getDisplayMetrics().density;

            float toolbar = 64f * d;
            float rowHeight = 84f * d;

            paint.setStyle(Paint.Style.FILL);
            paint.setColor(Color.rgb(43, 53, 63));
            c.drawRect(0, 0, getWidth(), toolbar, paint);

            paint.setTextAlign(Paint.Align.LEFT);
            paint.setTextSize(28f * d);
            paint.setColor(Color.WHITE);

            c.drawText(
                    "Bounce",
                    28f * d,
                    42f * d,
                    paint
            );

            // Overflow button.
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTextSize(28f * d);

            c.drawText(
                    "⋮",
                    getWidth() - 38f * d,
                    42f * d,
                    paint
            );

            String[] items = {
                    "Continue",
                    "New game",
                    "High score",
                    "Instructions"
            };

            float y = toolbar;

            for (int i = 0; i < items.length; i++) {
                if (i == menuSelection) {
                    paint.setColor(Color.rgb(52, 52, 52));
                    c.drawRect(
                            0,
                            y,
                            getWidth(),
                            y + rowHeight,
                            paint
                    );
                }

                paint.setColor(Color.rgb(220, 220, 220));
                c.drawRect(
                        0,
                        y + rowHeight - 1,
                        getWidth(),
                        y + rowHeight,
                        paint
                );

                paint.setTextAlign(Paint.Align.LEFT);
                paint.setTextSize(28f * d);
                paint.setColor(Color.WHITE);

                c.drawText(
                        items[i],
                        28f * d,
                        y + 53f * d,
                        paint
                );

                y += rowHeight;
            }

            paint.setTextSize(25f * d);
            paint.setColor(Color.rgb(3, 145, 235));

            c.drawText(
                    "EXIT",
                    28f * d,
                    getHeight() - 38f * d,
                    paint
            );

            if (overflowOpen)
                drawOverflow(c, d);
        }

        private void drawOverflow(Canvas c, float d) {
            float w = 190f * d;
            float h = 56f * d;

            float left = getWidth() - w - 12f * d;
            float top = 54f * d;

            paint.setColor(Color.rgb(55, 55, 55));
            c.drawRect(
                    left,
                    top,
                    left + w,
                    top + h,
                    paint
            );

            paint.setColor(Color.WHITE);
            paint.setTextAlign(Paint.Align.LEFT);
            paint.setTextSize(18f * d);

            c.drawText(
                    "Cheat codes",
                    left + 18f * d,
                    top + 36f * d,
                    paint
            );
        }

        private void drawSimpleScreen(Canvas c,
                                      String title,
                                      String text) {
            c.drawColor(Color.BLACK);

            float d = getResources()
                    .getDisplayMetrics().density;

            drawHeader(c, title, d);

            paint.setTextAlign(Paint.Align.LEFT);
            paint.setTextSize(18f * d);
            paint.setColor(Color.WHITE);

            c.drawText(
                    text,
                    28f * d,
                    125f * d,
                    paint
            );

            drawBack(c, d);
        }

        private void drawInstructions(Canvas c) {
            c.drawColor(Color.BLACK);

            float d = getResources()
                    .getDisplayMetrics().density;

            drawHeader(c, "Instructions", d);

            paint.setTextAlign(Paint.Align.LEFT);
            paint.setTextSize(18f * d);
            paint.setColor(Color.WHITE);

            c.drawText(
                    "Use the left and right controls",
                    28f * d,
                    125f * d,
                    paint
            );

            c.drawText(
                    "to move Bounce.",
                    28f * d,
                    155f * d,
                    paint
            );

            c.drawText(
                    "Tap the screen controls to play.",
                    28f * d,
                    205f * d,
                    paint
            );

            drawBack(c, d);
        }

        private void drawHeader(Canvas c,
                                String title,
                                float d) {
            paint.setColor(Color.rgb(43, 53, 63));
            c.drawRect(
                    0,
                    0,
                    getWidth(),
                    64f * d,
                    paint
            );

            paint.setTextAlign(Paint.Align.LEFT);
            paint.setTextSize(27f * d);
            paint.setColor(Color.WHITE);

            c.drawText(
                    title,
                    28f * d,
                    42f * d,
                    paint
            );
        }

        private void drawBack(Canvas c, float d) {
            paint.setTextAlign(Paint.Align.LEFT);
            paint.setTextSize(25f * d);
            paint.setColor(Color.rgb(3, 145, 235));

            c.drawText(
                    "BACK",
                    28f * d,
                    getHeight() - 38f * d,
                    paint
            );
        }

        private void drawCheatScreen(Canvas c) {
            c.drawColor(Color.BLACK);

            float d = getResources()
                    .getDisplayMetrics().density;

            drawHeader(c, "Cheat codes", d);

            paint.setTextAlign(Paint.Align.CENTER);
            paint.setColor(Color.WHITE);
            paint.setTextSize(22f * d);

            String shown = cheatInput.length() == 0
                    ? "_"
                    : cheatInput.toString();

            c.drawText(
                    shown,
                    getWidth() / 2f,
                    115f * d,
                    paint
            );

            String[][] keys = {
                    {"1", "2", "3"},
                    {"4", "5", "6"},
                    {"7", "8", "9"},
                    {"←", "0", "✓"}
            };

            float bw = 70f * d;
            float bh = 55f * d;
            float gap = 12f * d;

            float startX =
                    (getWidth() - (3f * bw + 2f * gap)) / 2f;

            float startY = 160f * d;

            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(2f * d);

            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 3; col++) {
                    float x = startX +
                            col * (bw + gap);

                    float y = startY +
                            row * (bh + gap);

                    paint.setColor(Color.LTGRAY);

                    c.drawRect(
                            x,
                            y,
                            x + bw,
                            y + bh,
                            paint
                    );

                    paint.setStyle(Paint.Style.FILL);
                    paint.setTextSize(19f * d);
                    paint.setColor(Color.WHITE);

                    c.drawText(
                            keys[row][col],
                            x + bw / 2f,
                            y + bh / 2f + 7f * d,
                            paint
                    );

                    paint.setStyle(Paint.Style.STROKE);
                }
            }

            paint.setStyle(Paint.Style.FILL);
            paint.setTextSize(15f * d);
            paint.setColor(Color.LTGRAY);

            c.drawText(
                    "787898  Invincible",
                    getWidth() / 2f,
                    startY + 4f * (bh + gap) + 35f * d,
                    paint
            );

            c.drawText(
                    "787899  Advanced",
                    getWidth() / 2f,
                    startY + 4f * (bh + gap) + 65f * d,
                    paint
            );

            paint.setTextAlign(Paint.Align.LEFT);
            paint.setColor(Color.rgb(3, 145, 235));
            paint.setTextSize(20f * d);

            c.drawText(
                    "BACK",
                    28f * d,
                    getHeight() - 38f * d,
                    paint
            );
        }

        private void drawGame(Canvas c) {
            int[] p = nativeFrame();

            if (p == null)
                return;

            if (frame == null) {
                frame = Bitmap.createBitmap(
                        128,
                        128,
                        Bitmap.Config.ARGB_8888
                );
            }

            frame.setPixels(
                    p,
                    0,
                    128,
                    0,
                    0,
                    128,
                    128
            );

            float scale = Math.min(
                    getWidth() / 128f,
                    getHeight() / 128f
            );

            float dw = 128f * scale;
            float dh = 128f * scale;

            float left =
                    (getWidth() - dw) / 2f;

            float top =
                    (getHeight() - dh) / 2f;

            c.drawBitmap(
                    frame,
                    null,
                    new RectF(
                            left,
                            top,
                            left + dw,
                            top + dh
                    ),
                    paint
            );

            // Visible touch controls.
            paint.setStyle(Paint.Style.FILL);
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTextSize(10f * scale);

            paint.setColor(
                    leftPressed
                            ? Color.WHITE
                            : Color.LTGRAY
            );

            c.drawRect(
                    left,
                    top + 96f * scale,
                    left + 42f * scale,
                    top + 128f * scale,
                    paint
            );

            paint.setColor(
                    rightPressed
                            ? Color.WHITE
                            : Color.LTGRAY
            );

            c.drawRect(
                    left + 86f * scale,
                    top + 96f * scale,
                    left + 128f * scale,
                    top + 128f * scale,
                    paint
            );

            paint.setColor(Color.BLACK);

            c.drawText(
                    "◀ LEFT",
                    left + 21f * scale,
                    top + 114f * scale,
                    paint
            );

            c.drawText(
                    "RIGHT ▶",
                    left + 107f * scale,
                    top + 114f * scale,
                    paint
            );
        }

        @Override public boolean onTouchEvent(MotionEvent e) {
            float d =
                    getResources()
                            .getDisplayMetrics()
                            .density;

            if (screen == SCREEN_MENU) {
                return handleMenuTouch(e, d);
            }

            if (screen == SCREEN_HIGH_SCORE ||
                    screen == SCREEN_INSTRUCTIONS) {
                if (e.getActionMasked() ==
                        MotionEvent.ACTION_UP) {
                    if (e.getY() >
                            getHeight() - 90f * d) {
                        enterMenu();
                    }
                }

                return true;
            }

            if (screen == SCREEN_CHEATS) {
                return handleCheatTouch(e, d);
            }

            return handleGameTouch(e);
        }

        private boolean handleMenuTouch(MotionEvent e,
                                        float d) {
            if (e.getActionMasked() !=
                    MotionEvent.ACTION_UP) {
                return true;
            }

            float x = e.getX();
            float y = e.getY();

            // Overflow.
            if (x > getWidth() - 80f * d &&
                    y < 70f * d) {
                overflowOpen = !overflowOpen;
                invalidate();
                return true;
            }

            // Overflow cheat item.
            if (overflowOpen) {
                float w = 190f * d;
                float left = getWidth() - w - 12f * d;

                if (x >= left &&
                        x <= left + w &&
                        y >= 54f * d &&
                        y <= 110f * d) {
                    screen = SCREEN_CHEATS;
                    overflowOpen = false;
                    cheatInput.setLength(0);
                    invalidate();
                    return true;
                }

                overflowOpen = false;
                invalidate();
                return true;
            }

            float toolbar = 64f * d;
            float rowHeight = 84f * d;

            if (y >= toolbar &&
                    y < toolbar + 4f * rowHeight) {
                int item =
                        (int)((y - toolbar) /
                                rowHeight);

                menuSelection =
                        Math.max(0, Math.min(3, item));

                activateMenuItem();
                return true;
            }

            if (y > getHeight() - 80f * d) {
                finish();
                return true;
            }

            return true;
        }

        private boolean handleCheatTouch(MotionEvent e,
                                         float d) {
            if (e.getActionMasked() !=
                    MotionEvent.ACTION_UP) {
                return true;
            }

            float bw = 70f * d;
            float bh = 55f * d;
            float gap = 12f * d;

            float startX =
                    (getWidth() -
                            (3f * bw + 2f * gap)) / 2f;

            float startY = 160f * d;

            float x = e.getX();
            float y = e.getY();

            for (int row = 0; row < 4; row++) {
                for (int col = 0; col < 3; col++) {
                    float bx =
                            startX +
                            col * (bw + gap);

                    float by =
                            startY +
                            row * (bh + gap);

                    if (x >= bx &&
                            x <= bx + bw &&
                            y >= by &&
                            y <= by + bh) {

                        if (row == 3 && col == 0) {
                            if (cheatInput.length() > 0)
                                cheatInput.deleteCharAt(
                                        cheatInput.length() - 1
                                );
                        } else if (row == 3 &&
                                   col == 2) {
                            submitCheat();
                        } else {
                            char[][] keys = {
                                    {'1','2','3'},
                                    {'4','5','6'},
                                    {'7','8','9'},
                                    {'x','0','x'}
                            };

                            char ch = keys[row][col];

                            if (ch != 'x')
                                appendCheatDigit(ch);
                        }

                        invalidate();
                        return true;
                    }
                }
            }

            if (y > getHeight() - 90f * d) {
                enterMenu();
            }

            return true;
        }

        private boolean handleGameTouch(MotionEvent e) {
            float scale = Math.min(
                    getWidth() / 128f,
                    getHeight() / 128f
            );

            float left =
                    (getWidth() - 128f * scale) / 2f;

            float top =
                    (getHeight() - 128f * scale) / 2f;

            float lx =
                    (e.getX() - left) / scale;

            float ly =
                    (e.getY() - top) / scale;

            int action = e.getActionMasked();

            if (action == MotionEvent.ACTION_DOWN ||
                    action == MotionEvent.ACTION_MOVE) {

                leftPressed =
                        ly >= 96 && lx < 42;

                rightPressed =
                        ly >= 96 && lx >= 86;

                nativeTouch(lx, ly, action);

            } else if (action == MotionEvent.ACTION_UP ||
                    action == MotionEvent.ACTION_CANCEL) {

                leftPressed = false;
                rightPressed = false;

                nativeTouch(
                        lx,
                        ly,
                        MotionEvent.ACTION_UP
                );
            }

            invalidate();
            return true;
        }
    }

    static final class BitmapFactoryCompat {
        static Bitmap decode(
                android.content.res.AssetManager am,
                String name
        ) throws Exception {
            try (InputStream in = am.open(name)) {
                return android.graphics.BitmapFactory
                        .decodeStream(in);
            }
        }
    }
}
