package org.drnas.simulide;

import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Looper;
import android.app.AlertDialog;
import android.content.SharedPreferences;
import android.util.DisplayMetrics;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.SeekBar;
import android.widget.CheckBox;
import java.util.concurrent.CountDownLatch;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import org.qtproject.qt5.android.bindings.QtActivity;

/** Fullscreen Qt host; Android continues to handle rotation without restarting Qt. */
public class DrnasActivity extends QtActivity {
    private volatile int insetLeft, insetTop, insetRight, insetBottom;
    private volatile boolean useSafeArea = false;

    /** Called on the Qt main thread BEFORE QApplication creates any screen. */
    public float chooseDisplayScale() {
        SharedPreferences prefs = getSharedPreferences("drnas_display", MODE_PRIVATE);
        float saved = prefs.getFloat("scale", 1.0f);
        if (Float.isNaN(saved) || Float.isInfinite(saved)) saved = 1.0f;
        final float initial = Math.max(0.6f, Math.min(1.4f, saved));
        final float[] selected = {initial};
        useSafeArea = !prefs.getBoolean("fitScreen", true);
        // Never wait on the Android UI thread itself.
        if (Looper.myLooper() == Looper.getMainLooper()) return initial;
        final CountDownLatch ready = new CountDownLatch(1);
        runOnUiThread(() -> {
            if (isFinishing()) { ready.countDown(); return; }
            DisplayMetrics metrics = new DisplayMetrics();
            getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
            int pad = Math.round(20 * metrics.density);
            LinearLayout body = new LinearLayout(this);
            body.setOrientation(LinearLayout.VERTICAL);
            body.setPadding(pad, pad / 2, pad, pad / 2);
            TextView help = new TextView(this);
            help.setText("Choose the workspace resolution. Smaller controls give more room for the circuit. You can change this at every start.");
            help.setTextSize(16);
            body.addView(help);
            TextView resolution = new TextView(this);
            resolution.setTextSize(18);
            body.addView(resolution);
            SeekBar scale = new SeekBar(this);
            scale.setMax(80);
            scale.setProgress(Math.round(initial * 100) - 60);
            scale.setMinimumHeight(Math.round(48 * metrics.density));
            body.addView(scale);
            Runnable update = () -> {
                selected[0] = (scale.getProgress() + 60) / 100.0f;
                int w = Math.round(metrics.widthPixels / metrics.density / selected[0]);
                int h = Math.round(metrics.heightPixels / metrics.density / selected[0]);
                resolution.setText(w + " × " + h + "  ·  " + Math.round(selected[0] * 100) + "%");
            };
            scale.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                public void onProgressChanged(SeekBar bar, int value, boolean user) { update.run(); }
                public void onStartTrackingTouch(SeekBar bar) {}
                public void onStopTrackingTouch(SeekBar bar) {}
            });
            update.run();
            CheckBox safe = new CheckBox(this);
            safe.setText("Fit entire screen (including camera area)");
            safe.setTextSize(16);
            safe.setChecked(!useSafeArea);
            body.addView(safe);
            AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle("DRNAS EMU · Display")
                .setView(body)
                .setPositiveButton("Apply and open", (d, which) -> {
                    useSafeArea = !safe.isChecked();
                    prefs.edit().putFloat("scale", selected[0]).putBoolean("fitScreen", !useSafeArea).apply();
                    ready.countDown();
                })
                .setNegativeButton("Use saved", (d, which) -> { selected[0] = initial; ready.countDown(); })
                .setNeutralButton("Automatic", (d, which) -> {
                    selected[0] = 1.0f; useSafeArea = false;
                    prefs.edit().putFloat("scale", 1.0f).putBoolean("fitScreen", true).apply();
                    ready.countDown();
                })
                .create();
            dialog.setOnCancelListener(d -> { selected[0] = initial; ready.countDown(); });
            dialog.setOnDismissListener(d -> ready.countDown());
            dialog.show();
            dialog.getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        });
        try { ready.await(); }
        catch (InterruptedException interrupted) { Thread.currentThread().interrupt(); }
        requestImmersive();
        return selected[0];
    }

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().getDecorView().getViewTreeObserver()
            .addOnGlobalLayoutListener(this::refreshSafeInsets);
        requestImmersive();
    }

    public void requestImmersive() {
        runOnUiThread(() -> {
            if (Build.VERSION.SDK_INT >= 28) {
                WindowManager.LayoutParams attrs = getWindow().getAttributes();
                attrs.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                getWindow().setAttributes(attrs);
            }
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            View decor = getWindow().getDecorView();
            decor.setFitsSystemWindows(false);
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
            // Android 11+ has a separate insets controller. Reflect so the Qt
            // 5.15 build can retain its pinned compile SDK 29.
            if (Build.VERSION.SDK_INT >= 30) {
                try {
                    Object controller = View.class.getMethod("getWindowInsetsController").invoke(decor);
                    Class<?> api = Class.forName("android.view.WindowInsetsController");
                    if (controller != null) {
                        api.getMethod("setSystemBarsBehavior", int.class).invoke(controller, 2);
                        api.getMethod("hide", int.class).invoke(controller, 3);
                    }
                } catch (ReflectiveOperationException ignored) { /* legacy immersive flags remain active */ }
            }
            refreshSafeInsets();
            decor.postDelayed(this::refreshSafeInsets, 250);
        });
    }

    private void refreshSafeInsets() {
        int left = 0, top = 0, right = 0, bottom = 0;
        if (Build.VERSION.SDK_INT >= 28) {
            WindowInsets insets = getWindow().getDecorView().getRootWindowInsets();
            DisplayCutout cutout = insets == null ? null : insets.getDisplayCutout();
            if (cutout != null) {
                left = cutout.getSafeInsetLeft(); top = cutout.getSafeInsetTop();
                right = cutout.getSafeInsetRight(); bottom = cutout.getSafeInsetBottom();
            }
        }
        insetLeft = left; insetTop = top; insetRight = right; insetBottom = bottom;
    }

    public int safeInsetLeft() { return useSafeArea ? insetLeft : 0; }
    public int safeInsetTop() { return useSafeArea ? insetTop : 0; }
    public int safeInsetRight() { return useSafeArea ? insetRight : 0; }
    public int safeInsetBottom() { return useSafeArea ? insetBottom : 0; }

    @Override public void onResume() { super.onResume(); requestImmersive(); }
    @Override public void onWindowFocusChanged(boolean focused) {
        super.onWindowFocusChanged(focused);
        if (focused) requestImmersive();
    }
    @Override public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        requestImmersive();
        getWindow().getDecorView().postDelayed(this::requestImmersive, 250);
    }
}
