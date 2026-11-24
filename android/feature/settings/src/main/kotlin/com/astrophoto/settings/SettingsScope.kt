package com.astrophoto.settings

enum class SettingsScope(val title: String, val primaryKeys: Set<String>, val cameraControls: Boolean = false) {
    General("General settings", setOf("output_directory", "session_name", "save_destination", "camera_module", "language", "light_theme", "night_theme")),
    Photo("Photo settings", setOf("single_capture"), true),
    Sequence("Sequence settings", setOf("sequence"), true),
    Camera("Camera controls", emptySet(), true),
    CaptureArea("Camera controls and format", setOf("zwo_capture"), true),
}
