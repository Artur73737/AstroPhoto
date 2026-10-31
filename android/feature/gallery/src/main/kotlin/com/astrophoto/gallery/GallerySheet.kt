package com.astrophoto.gallery

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import com.astrophoto.network.RemoteRepository
import com.astrophoto.protocol.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.json.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable fun GallerySheet(repository: RemoteRepository, dismiss: () -> Unit) {
    var files by remember { mutableStateOf(JsonArray(emptyList())) }
    var image by remember { mutableStateOf<android.graphics.Bitmap?>(null) }
    var error by remember { mutableStateOf<String?>(null) }
    var loading by remember { mutableStateOf(true) }
    var selected by remember { mutableStateOf("") }
    val scope = rememberCoroutineScope()
    LaunchedEffect(Unit) { try { files = repository.gallery() } catch (e: Exception) { error = e.message } finally { loading = false } }
    ModalBottomSheet(onDismissRequest = dismiss, sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)) {
        Column(Modifier.fillMaxWidth().fillMaxHeight(.9f).padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("Photos on PC", style = MaterialTheme.typography.headlineSmall)
            Text("Preview only · Nothing saved on this phone", style = MaterialTheme.typography.bodySmall)
            if (loading) LinearProgressIndicator(Modifier.fillMaxWidth())
            error?.let { Text(it, color = MaterialTheme.colorScheme.error) }
            image?.let { bitmap ->
                Text(selected, style = MaterialTheme.typography.labelMedium)
                Image(bitmap.asImageBitmap(), selected, Modifier.fillMaxWidth().weight(1f), contentScale = ContentScale.Fit)
                TextButton(onClick = { image = null }) { Text("Back to files") }
            } ?: LazyColumn(Modifier.weight(1f)) {
                if (files.isEmpty() && !loading) item { Text("No supported photos in the PC output folder.") }
                items(files.map { it.jsonObject }, key = { it.text("id") }) { file ->
                    ListItem(headlineContent = { Text(file.text("name")) }, supportingContent = { Text(file.text("relative_path")) }, trailingContent = {
                        TextButton(enabled = !loading, onClick = {
                            loading = true; selected = file.text("name"); error = null
                            scope.launch { try { image = withContext(Dispatchers.IO) { repository.galleryImage(file.text("id")) } } catch (e: Exception) { error = e.message } finally { loading = false } }
                        }) { Text("View") }
                    })
                    HorizontalDivider()
                }
            }
        }
    }
}
