package org.drnas.simulide;

import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowManager;
import org.qtproject.qt5.android.bindings.QtActivity;

/** Fullscreen Qt host; Android continues to handle rotation without restarting Qt. */
public class DrnasActivity extends QtActivity {
    private volatile int insetLeft, insetTop, insetRight, insetBottom;

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
            decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
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

    public int safeInsetLeft() { return insetLeft; }
    public int safeInsetTop() { return insetTop; }
    public int safeInsetRight() { return insetRight; }
    public int safeInsetBottom() { return insetBottom; }

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
