package com.acgc.port;

import android.app.Activity;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.ApplicationExitInfo;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.widget.Toast;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.List;

/** Reads this app's own Android crash history; nothing is sent over the network. */
final class CrashReports {
    private static final int MAX_TRACE = 4 * 1024 * 1024;
    private CrashReports() {}

    static void checkPreviousExit(Activity activity, Runnable proceed) {
        if (Build.VERSION.SDK_INT < 30) { proceed.run(); return; }
        new Thread(() -> {
            String report = null;
            long timestamp = 0;
            SharedPreferences prefs = activity.getSharedPreferences("crash_reports", Context.MODE_PRIVATE);
            try {
                ActivityManager manager = (ActivityManager)activity.getSystemService(Context.ACTIVITY_SERVICE);
                List<ApplicationExitInfo> exits = manager.getHistoricalProcessExitReasons(activity.getPackageName(), 0, 8);
                for (ApplicationExitInfo exit : exits) {
                    if (exit.getTimestamp() <= prefs.getLong("last_seen", 0)) continue;
                    int reason = exit.getReason();
                    if (reason != ApplicationExitInfo.REASON_CRASH_NATIVE
                            && reason != ApplicationExitInfo.REASON_CRASH
                            && reason != ApplicationExitInfo.REASON_SIGNALED
                            && reason != ApplicationExitInfo.REASON_LOW_MEMORY
                            && reason != ApplicationExitInfo.REASON_INITIALIZATION_FAILURE
                            && !(reason == ApplicationExitInfo.REASON_EXIT_SELF && exit.getStatus() != 0)) continue;
                    timestamp = exit.getTimestamp();
                    StringBuilder text = new StringBuilder("Animal Crossing Android — cierre anterior\n");
                    text.append("Instalada: ").append(activity.getPackageManager().getPackageInfo(activity.getPackageName(), 0).versionName);
                    text.append("\nAndroid ").append(Build.VERSION.RELEASE).append(" / ").append(Build.MODEL);
                    text.append("\nProceso de informe: ").append(exit.getProcessName());
                    text.append("\nFecha del cierre: ").append(new java.util.Date(timestamp));
                    text.append("\nMotivo: ").append(reason).append(" status: ").append(exit.getStatus());
                    text.append("\n").append(exit.getDescription()).append('\n');
                    File stage = new File(activity.getFilesDir(), "save/startup-stage.txt");
                    if (stage.isFile()) try (InputStream input = new FileInputStream(stage)) {
                        text.append("Última etapa guardada: ").append(new String(read(input, 1024), StandardCharsets.UTF_8));
                    }
                    try (InputStream input = exit.getTraceInputStream()) {
                        if (input == null) text.append("\nAndroid no conserva una traza para este cierre.\n");
                        else if (reason == ApplicationExitInfo.REASON_CRASH_NATIVE && Build.VERSION.SDK_INT >= 31)
                            text.append('\n').append(TombstoneTrace.format(read(input, MAX_TRACE)));
                        else text.append('\n').append(new String(read(input, 64 * 1024), StandardCharsets.UTF_8));
                    } catch (IOException error) { text.append("\nTraza no disponible: ").append(error.getMessage()); }
                    report = text.toString();
                    try (FileOutputStream file = new FileOutputStream(new File(activity.getFilesDir(), "last-crash.txt"))) {
                        file.write(report.getBytes(StandardCharsets.UTF_8));
                    }
                    break;
                }
            } catch (Exception error) { android.util.Log.w("ACGC", "Could not read own crash history", error); }
            final String detail = report;
            final long seen = timestamp;
            activity.runOnUiThread(() -> {
                if (activity.isFinishing() || activity.isDestroyed()) return;
                if (detail == null) { proceed.run(); return; }
                prefs.edit().putLong("last_seen", seen).apply();
                new AlertDialog.Builder(activity)
                        .setTitle("El juego se cerró en el intento anterior")
                        .setMessage("Puedes copiar el informe para identificar el fallo. Se guarda sólo en este dispositivo; tus partidas no se modifican.")
                        .setPositiveButton("Copiar informe", (dialog, which) -> {
                            ClipboardManager clipboard = (ClipboardManager)activity.getSystemService(Context.CLIPBOARD_SERVICE);
                            String copy = detail.length() > 60000
                                    ? detail.substring(0, 60000) + "\n[Informe recortado para el portapapeles]" : detail;
                            clipboard.setPrimaryClip(ClipData.newPlainText("ACGC cierre", copy));
                            Toast.makeText(activity, "Informe copiado; puedes pegarlo en el chat.", Toast.LENGTH_LONG).show();
                            ((LauncherActivity)activity).showRetryAfterCrash(proceed);
                        })
                        .setNeutralButton("Reintentar", (dialog, which) -> proceed.run())
                        .setNegativeButton("Cerrar", (dialog, which) -> activity.finish())
                        .setCancelable(false).show();
            });
        }, "acgc-crash-history").start();
    }

    private static byte[] read(InputStream input, int limit) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[4096];
        int count;
        while ((count = input.read(buffer, 0, Math.min(buffer.length, limit + 1 - output.size()))) > 0) {
            output.write(buffer, 0, count);
            if (output.size() > limit) throw new IOException("Traza supera el límite de lectura");
        }
        return output.toByteArray();
    }
}
