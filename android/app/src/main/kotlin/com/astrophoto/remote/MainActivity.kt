package com.astrophoto.remote

import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.astrophoto.controller.ControllerScreen
import com.astrophoto.designsystem.AstroTheme
import com.astrophoto.network.RemoteRepository

class ControllerModel(application: Application) : AndroidViewModel(application) {
    val repository = (application as RemoteApplication).repository

}
class MainActivity : ComponentActivity() {
    private val model by viewModels<ControllerModel>()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState); enableEdgeToEdge()
        // Like a camera viewfinder, stay awake only while this window is visible.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        setContent { AstroTheme { ControllerScreen(model.repository) } }
    }
    override fun onStart() { super.onStart(); model.repository.foreground(true) }
    override fun onStop() { model.repository.foreground(false); super.onStop() }
}
