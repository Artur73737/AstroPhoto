plugins { id("com.android.library"); id("org.jetbrains.kotlin.android") }
android {
    namespace = "com.astrophoto.network"; compileSdk = 35
    defaultConfig { minSdk = 26 }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
}
dependencies {
    api(project(":core:model"))
    api(project(":core:protocol"))
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    implementation("com.squareup.okhttp3:okhttp:4.12.0")
}
