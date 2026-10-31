pluginManagement { repositories { google(); mavenCentral(); gradlePluginPortal() } }
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories { google(); mavenCentral() }
}
rootProject.name = "AstroPhotoRemote"
include(":app", ":core:protocol", ":core:network", ":core:designsystem", ":feature:controller", ":feature:settings", ":feature:gallery")
include(":feature:connection")
include(":feature:liveview", ":core:model")
