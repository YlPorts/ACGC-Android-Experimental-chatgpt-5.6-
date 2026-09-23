package com.acgc.port;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.util.SparseArray;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import org.libsdl.app.SDLActivity;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Transparent multi-touch controls that emit ordinary Android key events into SDL. */
public final class VirtualControllerView extends View {
    private static final String PREFS_NAME = "controller_settings";
    private static final String PREF_VISIBLE = "controls_visible";
    private static final String POSITION_PREFIX = "position_";
    private static final String[] CONTROL_IDS = {
            "STICK", "D", "C", "A", "B", "X", "Y", "L", "R", "Z", "START"
    };

    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint stroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint text = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final List<Control> controls = new ArrayList<Control>();
    private final SparseArray<Control> pointers = new SparseArray<Control>();
    private final Set<Integer> pressedKeys = new HashSet<Integer>();
    private final SharedPreferences preferences;

    private float unit;
    private int viewWidth;
    private int viewHeight;
    private boolean controlsVisible;
    private boolean editMode;
    private boolean hideWithGamepad;
    private boolean gamepadConnected;
    private int opacityPercent = 75;
    private int scalePercent = 100;
    private int stickDeadzone = 12;
    private int stickSensitivity = 100;
    private int cstickDeadzone = 12;
    private int cstickSensitivity = 100;
    private Control draggedControl;
    private int draggedPointer = -1;
    private float dragOffsetX;
    private float dragOffsetY;

    public VirtualControllerView(Context context) {
        super(context);
        preferences = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        controlsVisible = preferences.getBoolean(PREF_VISIBLE, true);
        setFocusable(false);
        setClickable(true);
        fill.setStyle(Paint.Style.FILL);
        stroke.setStyle(Paint.Style.STROKE);
        stroke.setStrokeWidth(3f * getResources().getDisplayMetrics().density);
        stroke.setColor(Color.argb(175, 255, 255, 255));
        text.setColor(Color.WHITE);
        text.setTypeface(Typeface.DEFAULT_BOLD);
        text.setTextAlign(Paint.Align.CENTER);
        setBackgroundColor(Color.TRANSPARENT);
    }

    public boolean areControlsVisible() {
        return controlsVisible;
    }

    public void setControlsVisible(boolean visible) {
        controlsVisible = visible;
        preferences.edit().putBoolean(PREF_VISIBLE, visible).apply();
        updateEffectiveVisibility();
    }

    public void applySettings(boolean visible, int opacity, int scale,
                              boolean hideWhenGamepadConnected,
                              int mainDeadzone, int mainSensitivity,
                              int secondaryDeadzone, int secondarySensitivity) {
        int clampedOpacity = Math.max(25, Math.min(100, opacity));
        int clampedScale = Math.max(75, Math.min(150, scale));
        boolean rebuild = clampedScale != scalePercent;

        controlsVisible = visible;
        opacityPercent = clampedOpacity;
        scalePercent = clampedScale;
        hideWithGamepad = hideWhenGamepadConnected;
        stickDeadzone = Math.max(0, Math.min(40, mainDeadzone));
        stickSensitivity = Math.max(50, Math.min(150, mainSensitivity));
        cstickDeadzone = Math.max(0, Math.min(40, secondaryDeadzone));
        cstickSensitivity = Math.max(50, Math.min(150, secondarySensitivity));
        preferences.edit().putBoolean(PREF_VISIBLE, visible).apply();

        if (rebuild && viewWidth > 0 && viewHeight > 0) {
            rebuildControls(viewWidth, viewHeight);
        }
        updateEffectiveVisibility();
    }

    public void setGamepadConnected(boolean connected) {
        if (gamepadConnected == connected) return;
        gamepadConnected = connected;
        updateEffectiveVisibility();
    }

    public boolean isEditMode() {
        return editMode;
    }

    public void setEditMode(boolean editing) {
        releaseAll();
        editMode = editing;
        draggedControl = null;
        draggedPointer = -1;
        updateEffectiveVisibility();
    }

    private void updateEffectiveVisibility() {
        boolean visible = editMode ||
                (controlsVisible && !(hideWithGamepad && gamepadConnected));
        if (!visible) releaseAll();
        setVisibility(visible ? View.VISIBLE : View.GONE);
        invalidate();
    }

    public void resetLayout() {
        SharedPreferences.Editor editor = preferences.edit();
        for (String id : CONTROL_IDS) {
            editor.remove(positionKey(id, "x"));
            editor.remove(positionKey(id, "y"));
        }
        editor.apply();
        if (viewWidth > 0 && viewHeight > 0) rebuildControls(viewWidth, viewHeight);
        invalidate();
    }

    @Override
    protected void onSizeChanged(int width, int height, int oldWidth, int oldHeight) {
        super.onSizeChanged(width, height, oldWidth, oldHeight);
        viewWidth = width;
        viewHeight = height;
        rebuildControls(width, height);
    }

    private void rebuildControls(int width, int height) {
        releaseAll();
        controls.clear();
        draggedControl = null;
        draggedPointer = -1;
        unit = Math.min(width, height);

        float scale = scalePercent / 100.0f;
        float mainRadius = unit * 0.145f * scale;
        float smallRadius = unit * 0.087f * scale;
        float buttonRadius = unit * 0.066f * scale;

        addControl(Control.pad("STICK", mainRadius,
                KeyEvent.KEYCODE_W, KeyEvent.KEYCODE_S,
                KeyEvent.KEYCODE_A, KeyEvent.KEYCODE_D), 0.15f, 0.70f);
        addControl(Control.pad("D", smallRadius,
                KeyEvent.KEYCODE_I, KeyEvent.KEYCODE_K,
                KeyEvent.KEYCODE_J, KeyEvent.KEYCODE_L), 0.36f, 0.75f);
        addControl(Control.pad("C", smallRadius,
                KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_DOWN,
                KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_RIGHT), 0.64f, 0.76f);

        addControl(Control.button("A", buttonRadius * 1.12f, KeyEvent.KEYCODE_SPACE),
                0.88f, 0.68f);
        addControl(Control.button("B", buttonRadius, KeyEvent.KEYCODE_SHIFT_LEFT),
                0.78f, 0.79f);
        addControl(Control.button("X", buttonRadius, KeyEvent.KEYCODE_X),
                0.79f, 0.57f);
        addControl(Control.button("Y", buttonRadius, KeyEvent.KEYCODE_Y),
                0.69f, 0.68f);

        addControl(Control.button("L", buttonRadius * 0.92f, KeyEvent.KEYCODE_Q),
                0.11f, 0.12f);
        addControl(Control.button("R", buttonRadius * 0.92f, KeyEvent.KEYCODE_E),
                0.89f, 0.12f);
        addControl(Control.button("Z", buttonRadius * 0.78f, KeyEvent.KEYCODE_Z),
                0.76f, 0.12f);
        addControl(Control.button("START", buttonRadius * 0.82f, KeyEvent.KEYCODE_ENTER),
                0.50f, 0.12f);
    }

    private void addControl(Control control, float defaultX, float defaultY) {
        float normalizedX = preferences.getFloat(positionKey(control.label, "x"), defaultX);
        float normalizedY = preferences.getFloat(positionKey(control.label, "y"), defaultY);
        control.cx = normalizedX * viewWidth;
        control.cy = normalizedY * viewHeight;
        clampToScreen(control);
        controls.add(control);
    }

    private String positionKey(String id, String axis) {
        return POSITION_PREFIX + id + "_" + axis;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        text.setTextSize(unit * 0.034f);
        for (Control control : controls) drawControl(canvas, control);
    }

    private void drawControl(Canvas canvas, Control control) {
        int alpha = editMode ? (control.activePointer >= 0 ? 205 : 135)
                : (control.activePointer >= 0 ? 150 : 78);
        alpha = alpha * opacityPercent / 100;
        int strokeAlpha = (editMode ? 230 : 175) * opacityPercent / 100;
        fill.setColor(Color.argb(alpha, 17, 27, 25));
        stroke.setColor(editMode
                ? Color.argb(strokeAlpha, 114, 214, 161)
                : Color.argb(strokeAlpha, 255, 255, 255));
        canvas.drawCircle(control.cx, control.cy, control.radius, fill);
        canvas.drawCircle(control.cx, control.cy, control.radius, stroke);

        if (control.kind == Control.PAD) {
            int knobAlpha = (control.activePointer >= 0 ? 190 : 115) * opacityPercent / 100;
            fill.setColor(Color.argb(knobAlpha, 114, 214, 161));
            canvas.drawCircle(control.cx + control.knobX, control.cy + control.knobY,
                    control.radius * 0.40f, fill);
        }

        Paint.FontMetrics fm = text.getFontMetrics();
        float baseline = control.cy - (fm.ascent + fm.descent) * 0.5f;
        canvas.drawText(control.label, control.cx, baseline, text);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (editMode) return handleEditTouch(event);

        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            int index = event.getActionIndex();
            int pointerId = event.getPointerId(index);
            Control control = findControl(event.getX(index), event.getY(index));
            if (control != null) {
                control.activePointer = pointerId;
                pointers.put(pointerId, control);
                control.update(event.getX(index), event.getY(index), this);
                invalidate();
            }
        } else if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < event.getPointerCount(); i++) {
                Control control = pointers.get(event.getPointerId(i));
                if (control != null) control.update(event.getX(i), event.getY(i), this);
            }
            invalidate();
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            int index = event.getActionIndex();
            releasePointer(event.getPointerId(index));
            invalidate();
        } else if (action == MotionEvent.ACTION_CANCEL) {
            releaseAll();
            invalidate();
        }
        return true;
    }

    private boolean handleEditTouch(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            int index = event.getActionIndex();
            draggedPointer = event.getPointerId(index);
            draggedControl = findControl(event.getX(index), event.getY(index));
            if (draggedControl != null) {
                dragOffsetX = draggedControl.cx - event.getX(index);
                dragOffsetY = draggedControl.cy - event.getY(index);
                draggedControl.activePointer = draggedPointer;
                invalidate();
            }
        } else if (action == MotionEvent.ACTION_MOVE && draggedControl != null) {
            int index = event.findPointerIndex(draggedPointer);
            if (index >= 0) {
                draggedControl.cx = event.getX(index) + dragOffsetX;
                draggedControl.cy = event.getY(index) + dragOffsetY;
                clampToScreen(draggedControl);
                saveDraggedPosition();
                invalidate();
            }
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            finishDrag(true);
        }
        return true;
    }

    private void saveDraggedPosition() {
        if (draggedControl == null || viewWidth <= 0 || viewHeight <= 0) return;
        preferences.edit()
                .putFloat(positionKey(draggedControl.label, "x"), draggedControl.cx / viewWidth)
                .putFloat(positionKey(draggedControl.label, "y"), draggedControl.cy / viewHeight)
                .apply();
    }

    private void finishDrag(boolean save) {
        if (draggedControl != null) {
            draggedControl.activePointer = -1;
            if (save) saveDraggedPosition();
        }
        draggedControl = null;
        draggedPointer = -1;
        invalidate();
    }

    private void clampToScreen(Control control) {
        float padding = 4f * getResources().getDisplayMetrics().density;
        float marginX = Math.min(control.radius + padding, viewWidth * 0.48f);
        float marginY = Math.min(control.radius + padding, viewHeight * 0.48f);
        control.cx = Math.max(marginX, Math.min(viewWidth - marginX, control.cx));
        control.cy = Math.max(marginY, Math.min(viewHeight - marginY, control.cy));
    }

    private float directionThreshold(Control control) {
        if ("STICK".equals(control.label)) {
            return tunedThreshold(stickDeadzone, stickSensitivity);
        }
        if ("C".equals(control.label)) {
            return tunedThreshold(cstickDeadzone, cstickSensitivity);
        }
        return 0.23f;
    }

    private float tunedThreshold(int deadzone, int sensitivity) {
        // Defaults (12% and 100%) preserve the former 0.23 radius threshold.
        float threshold = (0.11f + deadzone / 100.0f) * 100.0f / sensitivity;
        return Math.max(0.08f, Math.min(0.55f, threshold));
    }

    private Control findControl(float x, float y) {
        Control best = null;
        float bestDistance = Float.MAX_VALUE;
        for (Control control : controls) {
            if (control.activePointer >= 0) continue;
            float dx = x - control.cx;
            float dy = y - control.cy;
            float distance = dx * dx + dy * dy;
            float hitRadius = control.radius * (control.kind == Control.PAD ? 1.35f : 1.18f);
            if (distance <= hitRadius * hitRadius && distance < bestDistance) {
                best = control;
                bestDistance = distance;
            }
        }
        return best;
    }

    private void releasePointer(int pointerId) {
        Control control = pointers.get(pointerId);
        if (control == null) return;
        control.release(this);
        pointers.remove(pointerId);
    }

    private void press(int keyCode) {
        if (pressedKeys.add(keyCode)) {
            try {
                SDLActivity.onNativeKeyDown(keyCode);
            } catch (UnsatisfiedLinkError ignored) {
                pressedKeys.remove(keyCode);
            }
        }
    }

    private void release(int keyCode) {
        if (pressedKeys.remove(keyCode)) {
            try {
                SDLActivity.onNativeKeyUp(keyCode);
            } catch (UnsatisfiedLinkError ignored) {
                // Native shutdown can race the final touch-up event.
            }
        }
    }

    private void releaseAll() {
        for (Control control : controls) control.release(this);
        Integer[] keys = pressedKeys.toArray(new Integer[0]);
        for (Integer key : keys) release(key);
        pointers.clear();
    }

    @Override
    protected void onDetachedFromWindow() {
        releaseAll();
        super.onDetachedFromWindow();
    }

    private static final class Control {
        static final int BUTTON = 0;
        static final int PAD = 1;

        final int kind;
        final String label;
        float cx;
        float cy;
        final float radius;
        final int key;
        final int up;
        final int down;
        final int left;
        final int right;

        int activePointer = -1;
        float knobX;
        float knobY;
        boolean upHeld;
        boolean downHeld;
        boolean leftHeld;
        boolean rightHeld;

        private Control(int kind, String label, float cx, float cy, float radius,
                        int key, int up, int down, int left, int right) {
            this.kind = kind;
            this.label = label;
            this.cx = cx;
            this.cy = cy;
            this.radius = radius;
            this.key = key;
            this.up = up;
            this.down = down;
            this.left = left;
            this.right = right;
        }

        static Control button(String label, float radius, int key) {
            return new Control(BUTTON, label, 0f, 0f, radius, key, 0, 0, 0, 0);
        }

        static Control pad(String label, float radius,
                           int up, int down, int left, int right) {
            return new Control(PAD, label, 0f, 0f, radius, 0, up, down, left, right);
        }

        void update(float x, float y, VirtualControllerView view) {
            if (kind == BUTTON) {
                view.press(key);
                return;
            }

            float dx = x - cx;
            float dy = y - cy;
            float length = (float)Math.sqrt(dx * dx + dy * dy);
            float max = radius * 0.62f;
            if (length > max && length > 0f) {
                dx = dx * max / length;
                dy = dy * max / length;
            }
            knobX = dx;
            knobY = dy;

            float threshold = radius * view.directionThreshold(this);
            setDirections(dy < -threshold, dy > threshold,
                    dx < -threshold, dx > threshold, view);
        }

        private void setDirections(boolean newUp, boolean newDown, boolean newLeft, boolean newRight,
                                   VirtualControllerView view) {
            if (newUp != upHeld) { if (newUp) view.press(up); else view.release(up); upHeld = newUp; }
            if (newDown != downHeld) { if (newDown) view.press(down); else view.release(down); downHeld = newDown; }
            if (newLeft != leftHeld) { if (newLeft) view.press(left); else view.release(left); leftHeld = newLeft; }
            if (newRight != rightHeld) { if (newRight) view.press(right); else view.release(right); rightHeld = newRight; }
        }

        void release(VirtualControllerView view) {
            if (kind == BUTTON) {
                view.release(key);
            } else {
                setDirections(false, false, false, false, view);
                knobX = 0f;
                knobY = 0f;
            }
            activePointer = -1;
        }
    }
}
