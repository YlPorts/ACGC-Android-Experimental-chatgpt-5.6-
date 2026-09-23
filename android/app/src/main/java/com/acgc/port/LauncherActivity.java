package com.acgc.port;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

public final class LauncherActivity extends Activity {
    static final String EXTRA_FORCE_RELOAD = "force_reload";
    static final String EXTRA_CHANGE_FOLDER = "change_folder";

    private TextView status;
    private Button selectFolderButton;
    private ProgressBar progress;
    private boolean busy;
    private boolean autoAttempted;
    private boolean retryWhenResumed;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        buildUi();
        handleIntent(getIntent(), state == null);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (retryWhenResumed && !busy && GameFolderManager.getSelectedTree(this) != null) {
            retryWhenResumed = false;
            prepareAndLaunch();
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        handleIntent(intent, true);
    }

    private void handleIntent(Intent intent, boolean fresh) {
        boolean changeFolder = intent != null && intent.getBooleanExtra(EXTRA_CHANGE_FOLDER, false);
        boolean forceReload = intent != null && intent.getBooleanExtra(EXTRA_FORCE_RELOAD, false);

        if (changeFolder) {
            GameFolderManager.forgetSelection(this);
            autoAttempted = true;
            updateIdleText("Seleccionar carpeta");
            openFolderPicker();
            return;
        }

        Uri selected = GameFolderManager.getSelectedTree(this);
        if (selected == null) {
            updateIdleText("Seleccionar carpeta");
            return;
        }

        String label = GameFolderManager.selectedFolderLabel(this);
        updateIdleText("Carpeta: " + label + "\nBuscando la ROM y preparando los archivos…");
        if ((fresh && !autoAttempted) || forceReload) {
            autoAttempted = true;
            prepareAndLaunch();
        }
    }

    private void buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(dp(30), dp(36), dp(30), dp(36));
        root.setBackgroundColor(Color.rgb(15, 23, 21));

        TextView title = new TextView(this);
        title.setText("Animal Crossing");
        title.setTextColor(Color.WHITE);
        title.setTextSize(24);
        title.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        title.setGravity(Gravity.CENTER);
        root.addView(title, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));

        status = new TextView(this);
        status.setTextColor(Color.rgb(203, 217, 211));
        status.setTextSize(15);
        status.setGravity(Gravity.CENTER);
        status.setPadding(dp(18), dp(17), dp(18), dp(17));
        status.setBackground(rounded(Color.rgb(29, 43, 39), 15));
        LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        statusParams.setMargins(0, dp(28), 0, dp(18));
        root.addView(status, statusParams);

        progress = new ProgressBar(this);
        progress.setIndeterminate(true);
        progress.setVisibility(View.GONE);
        LinearLayout.LayoutParams progressParams = new LinearLayout.LayoutParams(dp(46), dp(46));
        progressParams.setMargins(0, 0, 0, dp(14));
        root.addView(progress, progressParams);

        // The initial interface deliberately has one action only.
        selectFolderButton = new Button(this);
        selectFolderButton.setText("Seleccionar carpeta");
        selectFolderButton.setTextSize(17);
        selectFolderButton.setTextColor(Color.rgb(12, 27, 21));
        selectFolderButton.setAllCaps(false);
        selectFolderButton.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        selectFolderButton.setBackground(rounded(Color.rgb(118, 218, 165), 17));
        selectFolderButton.setOnClickListener(view -> openFolderPicker());
        LinearLayout.LayoutParams buttonParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, dp(60));
        root.addView(selectFolderButton, buttonParams);



        setContentView(root);
    }

    private void openFolderPicker() {
        if (busy) return;
        Uri current = GameFolderManager.getSelectedTree(this);
        startActivityForResult(
                GameFolderManager.createFolderPickerIntent(current),
                GameFolderManager.TREE_REQUEST_CODE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != GameFolderManager.TREE_REQUEST_CODE
                || resultCode != RESULT_OK
                || data == null
                || data.getData() == null) {
            return;
        }

        Uri tree = data.getData();
        if (!GameFolderManager.persistSelection(this, tree, data.getFlags())) {
            Toast.makeText(this,
                    "Android no permitió conservar el acceso a esa carpeta.",
                    Toast.LENGTH_LONG).show();
            return;
        }
        autoAttempted = true;
        prepareAndLaunch();
    }

    private void prepareAndLaunch() {
        if (busy) return;
        selectFolderButton.setVisibility(View.GONE);
        setBusy(true, "Preparando carpeta, idiomas, texturas, ROMs de NES y partidas…");
        new Thread(() -> {
            try {
                GameFolderManager.PreparedGame prepared =
                        GameFolderManager.prepareRuntime(getApplicationContext());
                runOnUiThread(() -> {
                    setBusy(false, "ROM lista: " + prepared.romName);
                    Intent game = new Intent(LauncherActivity.this, GameActivity.class);
                    game.putExtra(GameActivity.EXTRA_ROM_FD, prepared.romFd);
                    startActivity(game);
                    finish();
                });
            } catch (Exception error) {
                runOnUiThread(() -> {
                    String message = error.getMessage() == null
                            ? error.toString() : error.getMessage();
                    retryWhenResumed = true;
                    selectFolderButton.setVisibility(View.VISIBLE);
                    setBusy(false, message + "\n\nAñade o corrige los archivos y vuelve a la app.");
                });
            }
        }, "acgc-folder-prepare").start();
    }

    private void setBusy(boolean value, String message) {
        busy = value;
        progress.setVisibility(value ? View.VISIBLE : View.GONE);
        selectFolderButton.setEnabled(!value);
        selectFolderButton.setAlpha(value ? 0.55f : 1.0f);
        status.setText(message);
    }

    private void updateIdleText(String message) {
        setBusy(false, message);
    }

    private GradientDrawable rounded(int color, float radiusDp) {
        GradientDrawable drawable = new GradientDrawable();
        drawable.setColor(color);
        drawable.setCornerRadius(dp(radiusDp));
        return drawable;
    }

    private int dp(float value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
