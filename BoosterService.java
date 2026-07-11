package com.gamebooster;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.provider.Settings;

public class BoosterService extends Service {

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(1, buildNotification());
        applyBoost();
        return START_STICKY; // Giữ chạy ngầm liên tục
    }

    private void applyBoost() {
        // Tắt animation
        Settings.Global.putFloat(getContentResolver(),
            Settings.Global.WINDOW_ANIMATION_SCALE, 0f);
        Settings.Global.putFloat(getContentResolver(),
            Settings.Global.TRANSITION_ANIMATION_SCALE, 0f);
        Settings.Global.putFloat(getContentResolver(),
            Settings.Global.ANIMATOR_DURATION_SCALE, 0f);

        // Set 90Hz
        Settings.System.putInt(getContentResolver(), "peak_refresh_rate", 90);
        Settings.System.putInt(getContentResolver(), "min_refresh_rate", 90);
    }

    private Notification buildNotification() {
        String channelId = "booster_channel";
        if (Build.VERSION.SdkInt >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                channelId, "Game Booster", NotificationManager.IMPORTANCE_LOW);
            getSystemService(NotificationManager.class).createNotificationChannel(channel);
        }

        return new Notification.Builder(this, channelId)
            .setContentTitle("Game Booster đang chạy")
            .setContentText("90Hz | Animation OFF | Đang tối ưu...")
            .setSmallIcon(android.R.drawable.ic_media_play)
            .build();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }
}
