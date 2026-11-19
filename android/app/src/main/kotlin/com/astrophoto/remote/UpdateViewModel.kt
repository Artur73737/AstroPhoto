package com.astrophoto.remote

import android.app.Application
import android.content.Context
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.astrophoto.network.UpdateChecker
import com.astrophoto.network.UpdateInfo
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.util.concurrent.TimeUnit

class UpdateViewModel(application: Application) : AndroidViewModel(application) {
    data class Notice(val info: UpdateInfo, val currentVersion: String)

    private val preferences = application.getSharedPreferences("update-check", Context.MODE_PRIVATE)
    private val checker = UpdateChecker(BuildConfig.UPDATE_REPOSITORY)
    private val _notice = MutableStateFlow<Notice?>(null)
    val notice = _notice.asStateFlow()

    init {
        checkAutomatically()
    }

    private fun checkAutomatically() {
        viewModelScope.launch {
            val lastCheck = preferences.getLong(KEY_LAST_CHECK, 0L)
            if (System.currentTimeMillis() - lastCheck < TimeUnit.HOURS.toMillis(24)) return@launch
            val currentVersion = currentVersion()
            val update = runCatching { checker.check(currentVersion) }.getOrNull() ?: return@launch
            preferences.edit().putLong(KEY_LAST_CHECK, System.currentTimeMillis()).apply()
            if (update.version != preferences.getString(KEY_SKIPPED_VERSION, null)) {
                _notice.value = Notice(update, currentVersion)
            }
        }
    }

    fun later() {
        _notice.value = null
    }

    fun skip(version: String) {
        preferences.edit().putString(KEY_SKIPPED_VERSION, version).apply()
        _notice.value = null
    }

    private fun currentVersion(): String = runCatching {
        val application = getApplication<Application>()
        application.packageManager.getPackageInfo(application.packageName, 0).versionName ?: "0.0.0"
    }.getOrDefault("0.0.0")

    private companion object {
        const val KEY_LAST_CHECK = "last_check"
        const val KEY_SKIPPED_VERSION = "skipped_version"
    }
}
