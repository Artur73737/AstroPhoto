plugins { id("com.android.application"); id("org.jetbrains.kotlin.android"); id("org.jetbrains.kotlin.plugin.compose") }
android {
    namespace = "com.astrophoto.remote"; compileSdk = 35
    defaultConfig { applicationId = "com.astrophoto.remote"; minSdk = 26; targetSdk = 35; versionCode = 2; versionName = "0.1.1"; testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner" }
    buildFeatures { compose = true }
    compileOptions { sourceCompatibility = JavaVersion.VERSION_17; targetCompatibility = JavaVersion.VERSION_17 }
    kotlinOptions { jvmTarget = "17" }
    signingConfigs {
        if (System.getenv("ASTRO_ANDROID_KEYSTORE") != null) create("distribution") {
            storeFile = file(System.getenv("ASTRO_ANDROID_KEYSTORE"))
            storePassword = System.getenv("ASTRO_ANDROID_STORE_PASSWORD")
            keyAlias = "astrophoto"
            keyPassword = System.getenv("ASTRO_ANDROID_KEY_PASSWORD")
        }
    }
    buildTypes { release { isMinifyEnabled = false; signingConfig = signingConfigs.findByName("distribution") } }
}
dependencies {
    androidTestImplementation(platform("androidx.compose:compose-bom:2024.12.01"))
    implementation(project(":feature:controller")); implementation(project(":core:network")); implementation(project(":core:designsystem"))
    implementation("androidx.activity:activity-compose:1.9.3")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.8.7")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.8.7")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("androidx.test:runner:1.7.0")
    androidTestImplementation("androidx.test.espresso:espresso-core:3.7.0")
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
    androidTestImplementation("com.google.zxing:core:3.4.1")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
}
