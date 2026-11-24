package com.astrophoto.remote

import android.app.Application
import android.content.Context
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.astrophoto.network.UpdateChecker
import com.astrophoto.network.UpdateInfo
import com.astrophoto.network.UpdateCheckResult
import kotlinx.coroutines.CancellationException
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
    data class Status(val installed: String, val checking: Boolean = false, val result: UpdateCheckResult? = null, val checkedAt: Long = 0L, val error: String? = null)
    private val _status = MutableStateFlow(Status(currentVersion()))
    val status = _status.asStateFlow()

    init {
        checkAutomatically()
    }

    private fun checkAutomatically() {
        val lastCheck = preferences.getLong(KEY_LAST_CHECK, 0L)
        if (System.currentTimeMillis() - lastCheck >= TimeUnit.HOURS.toMillis(24)) checkForUpdates(false)
    }

    /** Manual checks bypass both the daily throttle and skipped-release preference. */
    fun checkForUpdates(manual: Boolean = true) {
        if (_status.value.checking) return
        _status.value = _status.value.copy(checking = true, error = null)
        viewModelScope.launch {
            try {
                val result = checker.inspect(_status.value.installed)
                val now = System.currentTimeMillis()
                _status.value = _status.value.copy(checking = false, result = result, checkedAt = now)
                preferences.edit().putLong(KEY_LAST_CHECK, now).apply()
                if (!manual && result.updateAvailable && result.latest.version != preferences.getString(KEY_SKIPPED_VERSION, null)) {
                    _notice.value = Notice(result.latest, _status.value.installed)
                }
            } catch (e: CancellationException) { throw e }
            catch (e: Exception) { _status.value = _status.value.copy(checking = false, error = e.message ?: "Update check failed") }
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
