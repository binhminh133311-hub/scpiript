package com.gamebooster;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public class BootReceiver extends BroadcastReceiver {
    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_BOOT_COMPLETED.equals(intent.getAction())) {
            // Tự động chạy ngầm khi khởi động máy
            Intent service = new Intent(context, BoosterService.class);
            context.startService(service);
        }
    }
}
