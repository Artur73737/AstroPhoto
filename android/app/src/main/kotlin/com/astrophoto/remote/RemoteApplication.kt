package com.astrophoto.remote

import android.app.Application
import android.content.Intent
import android.os.Handler
import android.os.Looper
import com.astrophoto.network.RemoteRepository
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob

/** Process-owned session: rotation and Activity recreation must not destroy pairing. */
class RemoteApplication : Application() {
    private val sessionScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    val repository by lazy {
        RemoteRepository(sessionScope) { active ->
            Handler(Looper.getMainLooper()).post {
                val intent = Intent(this, ConnectionService::class.java)
                if (active) startForegroundService(intent) else stopService(intent)
            }
        }
    }
}
