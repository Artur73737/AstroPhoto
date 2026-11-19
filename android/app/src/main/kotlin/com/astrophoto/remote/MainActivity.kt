package com.astrophoto.remote

import android.app.Application
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.compose.runtime.getValue
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.lifecycle.viewModelScope
import com.astrophoto.controller.ControllerScreen
import com.astrophoto.designsystem.AstroTheme
import com.astrophoto.network.RemoteRepository

class ControllerModel(application: Application) : AndroidViewModel(application) {
    val repository = (application as RemoteApplication).repository

}
class MainActivity : ComponentActivity() {
    private val model by viewModels<ControllerModel>()
    private val updateModel by viewModels<UpdateViewModel>()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState); enableEdgeToEdge()
        // Like a camera viewfinder, stay awake only while this window is visible.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setContent {
            AstroTheme {
                val updateNotice by updateModel.notice.collectAsStateWithLifecycle()
                ControllerScreen(model.repository)
                updateNotice?.let { notice ->
                    UpdateDialog(
                        notice = notice,
                        onOpenRelease = { url -> startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url))) },
                        onLater = updateModel::later,
                        onSkip = updateModel::skip,
                    )
                }
            }
        }
    }
    override fun onStart() { super.onStart(); model.repository.foreground(true) }
    override fun onStop() { model.repository.foreground(false); super.onStop() }
}
