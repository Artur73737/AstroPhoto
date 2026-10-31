package com.astrophoto.remote

import android.app.*
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.*

/** User-visible connected-device service; never restarted after a kill/task dismissal. */
class ConnectionService : Service() {
    private var wakeLock: PowerManager.WakeLock? = null
    override fun onCreate() {
        super.onCreate()
        val notifications = getSystemService(NotificationManager::class.java)
        notifications.createNotificationChannel(NotificationChannel("camera_connection", "Camera connection", NotificationManager.IMPORTANCE_LOW))
        val open = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
        val disconnect = PendingIntent.getService(this, 1, Intent(this, ConnectionService::class.java).setAction("disconnect"), PendingIntent.FLAG_IMMUTABLE)
        val notification = Notification.Builder(this, "camera_connection")
            .setSmallIcon(R.drawable.ic_astrophoto).setContentTitle("AstroPhoto Remote")
            .setContentText("PC connection stays active in the background")
            .setContentIntent(open).setOngoing(true)
            .addAction(Notification.Action.Builder(null, "Disconnect", disconnect).build()).build()
        if (Build.VERSION.SDK_INT >= 29) startForeground(1, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        else startForeground(1, notification)
        // Released on disconnect/task removal; keeps WebSocket pings alive while screen is off.
        wakeLock = getSystemService(PowerManager::class.java).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "AstroPhoto:Connection").apply { acquire() }
    }
    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == "disconnect") { (application as RemoteApplication).repository.disconnect(); stopSelf() }
        return START_NOT_STICKY
    }
    override fun onTaskRemoved(rootIntent: Intent?) {
        (application as RemoteApplication).repository.disconnect()
        stopSelf()
        super.onTaskRemoved(rootIntent)
    }
    override fun onDestroy() { wakeLock?.let { if (it.isHeld) it.release() }; wakeLock = null; super.onDestroy() }
    override fun onBind(intent: Intent?) = null
}
