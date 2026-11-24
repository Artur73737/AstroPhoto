package com.astrophoto.controller.chrome

import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.snapping.rememberSnapFlingBehavior
import androidx.compose.foundation.interaction.collectIsDraggedAsState
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.unit.dp
import com.astrophoto.controller.model.CameraMode
import com.astrophoto.designsystem.AstroCyan
import kotlin.math.abs

@Composable internal fun ModeCarousel(modes: List<CameraMode>, selected: CameraMode, enabled: Boolean, onSelect: (CameraMode) -> Unit) {
    val index = modes.indexOf(selected).coerceAtLeast(0)
    val list = rememberLazyListState(initialFirstVisibleItemIndex = index)
    val dragged by list.interactionSource.collectIsDraggedAsState()
    LaunchedEffect(selected, modes) { list.animateScrollToItem(index) }
    LaunchedEffect(list, modes, enabled) {
        var userGesture = false
        snapshotFlow { list.isScrollInProgress to dragged }.collect { (scrolling, dragging) ->
            if (dragging) userGesture = true
            if (!scrolling && userGesture) {
                userGesture = false
                val layout = list.layoutInfo
                val center = (layout.viewportStartOffset + layout.viewportEndOffset) / 2
                val nearest = layout.visibleItemsInfo.minByOrNull { abs(it.offset + it.size / 2 - center) }
                if (enabled && nearest != null) modes.getOrNull(nearest.index)?.let(onSelect)
            }
        }
    }
    BoxWithConstraints(Modifier.fillMaxWidth().height(48.dp).testTag("mode-carousel")) {
        val itemWidth = 96.dp
        val inset = ((maxWidth - itemWidth) / 2).coerceAtLeast(0.dp)
        LazyRow(state = list, flingBehavior = rememberSnapFlingBehavior(list), userScrollEnabled = enabled,
            contentPadding = PaddingValues(horizontal = inset), horizontalArrangement = Arrangement.spacedBy(6.dp),
            verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxSize()) {
            itemsIndexed(modes, key = { _, mode -> mode.name }) { _, mode ->
                Box(Modifier.width(itemWidth).height(44.dp).selectable(mode == selected, enabled, Role.Tab) { onSelect(mode) }
                    .padding(vertical = 5.dp).background(if (mode == selected) AstroCyan else Color.Transparent, CircleShape), contentAlignment = Alignment.Center) {
                    Text(mode.title, color = if (mode == selected) Color.Black else Color.White.copy(alpha = if (enabled) .85f else .5f), style = MaterialTheme.typography.labelLarge)
                }
            }
        }
    }
}
