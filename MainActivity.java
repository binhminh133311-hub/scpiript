package com.gamebooster;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("gamebooster"); // Load C++ library
    }

    // Khai báo hàm C++
    public native void enableGameMode(Object context, Object surface);
    public native void disableGameMode(Object context);
    public native float getCPUUsage();
    public native long getAvailableRAM(Object context);
    public native float getBatteryLevel(Object context);
    public native float getTemperature(Object context);

    private TextView tvStatus;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvStatus = findViewById(R.id.tvStatus);
        Button btnOn  = findViewById(R.id.btnOn);
        Button btnOff = findViewById(R.id.btnOff);

        // Cấp quyền WRITE_SETTINGS
        if (!Settings.System.canWrite(this)) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_WRITE_SETTINGS);
            startActivity(intent);
        }

        // Bật Game Mode
        btnOn.setOnClickListener(v -> {
            // Tắt animation qua Java
            Settings.Global.putFloat(getContentResolver(),
                Settings.Global.WINDOW_ANIMATION_SCALE, 0f);
            Settings.Global.putFloat(getContentResolver(),
                Settings.Global.TRANSITION_ANIMATION_SCALE, 0f);
            Settings.Global.putFloat(getContentResolver(),
                Settings.Global.ANIMATOR_DURATION_SCALE, 0f);

            // Set 90Hz
            Settings.System.putInt(getContentResolver(), "peak_refresh_rate", 90);
            Settings.System.putInt(getContentResolver(), "min_refresh_rate", 90);

            // Bật service chạy ngầm
            startService(new Intent(this, BoosterService.class));

            tvStatus.setText("Game Mode: BẬT\n90Hz | Animation: OFF");
            Toast.makeText(this, "Game Mode ON!", Toast.LENGTH_SHORT).show();
        });

        // Tắt Game Mode
        btnOff.setOnClickListener(v -> {
            Settings.Global.putFloat(getContentResolver(),
                Settings.Global.WINDOW_ANIMATION_SCALE, 1f);
            Settings.Global.putFloat(getContentResolver(),
                Settings.Global.TRANSITION_ANIMATION_SCALE, 1f);
            Settings.Global.putFloat(getContentResolver(),
                Settings.Global.ANIMATOR_DURATION_SCALE, 1f);

            Settings.System.putInt(getContentResolver(), "peak_refresh_rate", 60);
            Settings.System.putInt(getContentResolver(), "min_refresh_rate", 60);

            stopService(new Intent(this, BoosterService.class));

            tvStatus.setText("Game Mode: TẮT");
            Toast.makeText(this, "Game Mode OFF!", Toast.LENGTH_SHORT).show();
        });
    }
}
