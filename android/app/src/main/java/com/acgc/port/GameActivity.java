package com.acgc.port;

import android.content.Intent;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.widget.Toast;

import org.libsdl.app.SDLActivity;

import java.util.concurrent.atomic.AtomicBoolean;

public final class GameActivity extends SDLActivity {
    static final String EXTRA_ROM_FD = "rom_fd";

    private static final long SAVE_SYNC_INTERVAL_MS = 60_000L;
    private static final long NATIVE_ACTION_POLL_MS = 150L;

    private static final int ACTION_APPLY_TOUCH = 1 << 0;
    private static final int ACTION_EDIT_CONTROLS = 1 << 1;
    private static final int ACTION_RESET_CONTROLS = 1 << 2;
    private static final int ACTION_SYNC_DATA = 1 << 3;
    private static final int ACTION_RELOAD_CONTENT = 1 << 4;
    private static final int ACTION_CHANGE_FOLDER = 1 << 5;
    private static final int ACTION_RESTART_GAME = 1 << 6;
    private static final int ACTION_CLOSE_APP = 1 << 7;

    private static final int SETTING_TOUCH_VISIBLE = 1;
    private static final int SETTING_TOUCH_OPACITY = 2;
    private static final int SETTING_TOUCH_SCALE = 3;
    private static final int SETTING_TOUCH_HIDE_WITH_GAMEPAD = 4;
    private static final int SETTING_STICK_DEADZONE = 5;
    private static final int SETTING_STICK_SENSITIVITY = 6;
    private static final int SETTING_CSTICK_DEADZONE = 7;
    private static final int SETTING_CSTICK_SENSITIVITY = 8;

    private static native int nativeTakePendingActions();
    private static native int nativeGetAndroidSetting(int key);

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final AtomicBoolean syncRunning = new AtomicBoolean(false);

    private VirtualControllerView controller;
    private boolean shuttingDown;
    private Intent pendingLauncherIntent;

    private final Runnable periodicSync = new Runnable() {
        @Override
        public void run() {
            if (!shuttingDown) {
                syncFolder(false);
                handler.postDelayed(this, SAVE_SYNC_INTERVAL_MS);
            }
        }
    };

    private final Runnable nativeActionPoll = new Runnable() {
        @Override
        public void run() {
            if (shuttingDown) return;

            int actions = 0;
            try {
                actions = nativeTakePendingActions();
                applyNativeControllerSettings();
            } catch (UnsatisfiedLinkError ignored) {
                // libmain may still be starting or may already be shutting down.
            }

            if ((actions & ACTION_APPLY_TOUCH) != 0) {
                applyNativeControllerSettings();
            }
            if ((actions & ACTION_EDIT_CONTROLS) != 0) {
                beginEditingControls();
            }
            if ((actions & ACTION_RESET_CONTROLS) != 0 && controller != null) {
                controller.resetLayout();
                Toast.makeText(GameActivity.this,
                        "Posiciones restauradas", Toast.LENGTH_SHORT).show();
            }
            if ((actions & ACTION_SYNC_DATA) != 0) {
                syncFolder(true);
            }

            // Actions that close SDL are mutually exclusive in normal use.
            if ((actions & ACTION_CHANGE_FOLDER) != 0) {
                returnToLauncher(true);
                return;
            }
            if ((actions & (ACTION_RELOAD_CONTENT | ACTION_RESTART_GAME)) != 0) {
                returnToLauncher(false);
                return;
            }
            if ((actions & ACTION_CLOSE_APP) != 0) {
                closeApplication();
                return;
            }

            handler.postDelayed(this, NATIVE_ACTION_POLL_MS);
        }
    };

    @Override
    protected String[] getArguments() {
        int fd = getIntent() == null ? -1 : getIntent().getIntExtra(EXTRA_ROM_FD, -1);
        if (fd < 0) fd = GameFolderManager.RuntimeSession.romFd();
        if (fd < 0) return new String[0];
        return new String[] {"--rom-fd", Integer.toString(fd)};
    }

    @Override
    protected void onCreate(Bundle state) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        configureWindow();
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        super.onCreate(state);
        configureWindow();

        controller = new VirtualControllerView(this);
        addContentView(controller, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        handler.postDelayed(nativeActionPoll, 300L);
        handler.postDelayed(periodicSync, SAVE_SYNC_INTERVAL_MS);
        immersive();
    }

    @Override
    protected void onResume() {
        super.onResume();
        immersive();
    }

    @Override
    protected void onPause() {
        syncFolder(false);
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        shuttingDown = true;
        handler.removeCallbacksAndMessages(null);
        Intent launcher = pendingLauncherIntent;
        pendingLauncherIntent = null;
        super.onDestroy();
        GameFolderManager.RuntimeSession.close();

        try {
            GameFolderManager.syncOutputs(getApplicationContext());
        } catch (Exception error) {
            android.util.Log.e("ACGC", "Final folder sync failed", error);
        }
        if (launcher != null) {
            new Handler(Looper.getMainLooper()).post(() -> startActivity(launcher));
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) immersive();
    }

    @Override
    public void onBackPressed() {
        if (controller != null && controller.isEditMode()) {
            finishEditingControls();
            return;
        }

        // Always provide a way back into the in-game pause menu, even when
        // touch controls are hidden or a gamepad is connected.
        try {
            SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_ESCAPE);
            SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_ESCAPE);
        } catch (UnsatisfiedLinkError error) {
            super.onBackPressed();
        }
    }

    private void applyNativeControllerSettings() {
        if (controller == null) return;

        int visible = nativeGetAndroidSetting(SETTING_TOUCH_VISIBLE);
        int opacity = nativeGetAndroidSetting(SETTING_TOUCH_OPACITY);
        int scale = nativeGetAndroidSetting(SETTING_TOUCH_SCALE);
        int hideWithGamepad = nativeGetAndroidSetting(SETTING_TOUCH_HIDE_WITH_GAMEPAD);
        int stickDeadzone = nativeGetAndroidSetting(SETTING_STICK_DEADZONE);
        int stickSensitivity = nativeGetAndroidSetting(SETTING_STICK_SENSITIVITY);
        int cstickDeadzone = nativeGetAndroidSetting(SETTING_CSTICK_DEADZONE);
        int cstickSensitivity = nativeGetAndroidSetting(SETTING_CSTICK_SENSITIVITY);

        controller.applySettings(
                visible != 0,
                opacity,
                scale,
                hideWithGamepad != 0,
                stickDeadzone,
                stickSensitivity,
                cstickDeadzone,
                cstickSensitivity);
        controller.setGamepadConnected(hasPhysicalGamepad());
    }

    private boolean hasPhysicalGamepad() {
        int[] ids = InputDevice.getDeviceIds();
        for (int id : ids) {
            InputDevice device = InputDevice.getDevice(id);
            if (device == null || device.isVirtual()) continue;
            int sources = device.getSources();
            if ((sources & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                    (sources & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
                return true;
            }
        }
        return false;
    }

    private void beginEditingControls() {
        if (controller == null) return;
        controller.setControlsVisible(true);
        controller.setEditMode(true);
        Toast.makeText(this,
                "Arrastra los botones. Usa Atrás para guardar y terminar.",
                Toast.LENGTH_LONG).show();
    }

    private void finishEditingControls() {
        controller.setEditMode(false);
        applyNativeControllerSettings();
        Toast.makeText(this, "Posiciones guardadas", Toast.LENGTH_SHORT).show();
        immersiveSoon();
    }

    private void returnToLauncher(boolean changeFolder) {
        if (shuttingDown) return;
        shuttingDown = true;
        handler.removeCallbacksAndMessages(null);
        Intent launcher = new Intent(this, LauncherActivity.class);
        launcher.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
        launcher.putExtra(changeFolder
                ? LauncherActivity.EXTRA_CHANGE_FOLDER
                : LauncherActivity.EXTRA_FORCE_RELOAD, true);

        pendingLauncherIntent = launcher;
        finish();
    }

    private void closeApplication() {
        if (shuttingDown) return;
        shuttingDown = true;
        handler.removeCallbacksAndMessages(null);
        syncFolder(false);
        finishAffinity();
    }

    private void syncFolder(boolean showResult) {
        if (!syncRunning.compareAndSet(false, true)) return;
        new Thread(() -> {
            String error = null;
            try {
                GameFolderManager.syncOutputs(getApplicationContext());
            } catch (Exception exception) {
                error = exception.getMessage() == null ? exception.toString() : exception.getMessage();
            } finally {
                syncRunning.set(false);
            }
            if (showResult) {
                String finalError = error;
                runOnUiThread(() -> Toast.makeText(this,
                        finalError == null ? "Datos sincronizados con la carpeta" : finalError,
                        Toast.LENGTH_LONG).show());
            }
        }, "acgc-save-sync").start();
    }

    private void configureWindow() {
        Window window = getWindow();
        window.setStatusBarColor(Color.TRANSPARENT);
        window.setNavigationBarColor(Color.TRANSPARENT);

        if (Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams attributes = window.getAttributes();
            attributes.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            window.setAttributes(attributes);
        }
        if (Build.VERSION.SDK_INT >= 29) {
            window.setStatusBarContrastEnforced(false);
            window.setNavigationBarContrastEnforced(false);
        }
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false);
        }
    }

    private void immersiveSoon() {
        getWindow().getDecorView().postDelayed(this::immersive, 120);
    }

    private void immersive() {
        configureWindow();
        View decor = getWindow().getDecorView();
        decor.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                        View.SYSTEM_UI_FLAG_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_STABLE);

        if (Build.VERSION.SDK_INT >= 30 && getWindow().getInsetsController() != null) {
            getWindow().getInsetsController().hide(
                    WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
            getWindow().getInsetsController().setSystemBarsBehavior(
                    android.view.WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        }
    }
}
