param([string]$JavaHome=$env:JAVA_HOME, [string]$AndroidSdk=$env:ANDROID_HOME)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(!$JavaHome -or !(Test-Path "$JavaHome/bin/keytool.exe")){throw 'Set JAVA_HOME to a JDK 17 installation'}
if(!$AndroidSdk){$AndroidSdk=Join-Path $env:LOCALAPPDATA 'Android/Sdk'}
$signing=Join-Path $root 'android/.signing'
$credentials=Join-Path $signing 'credentials.json'
$keystore=Join-Path $signing 'astrophoto-release.p12'
New-Item -ItemType Directory -Force $signing | Out-Null
if(!(Test-Path $credentials)) {
    if(Test-Path $keystore){throw 'Existing key found without credentials. Restore credentials; do not replace the signing identity.'}
    $password=[Convert]::ToHexString([Security.Cryptography.RandomNumberGenerator]::GetBytes(32))
    @{password=$password} | ConvertTo-Json | Set-Content -LiteralPath $credentials
}
$secret=Get-Content -LiteralPath $credentials -Raw | ConvertFrom-Json
$env:JAVA_HOME=$JavaHome
$env:ASTRO_ANDROID_KEYSTORE=$keystore
$env:ASTRO_ANDROID_STORE_PASSWORD=$secret.password
$env:ASTRO_ANDROID_KEY_PASSWORD=$secret.password
try {
    if(!(Test-Path $keystore)) {
        & "$JavaHome/bin/keytool.exe" -genkeypair -keystore $keystore -storetype PKCS12 -alias astrophoto -keyalg RSA -keysize 3072 -validity 10000 -dname 'CN=AstroPhoto Remote' -storepass:env ASTRO_ANDROID_STORE_PASSWORD -keypass:env ASTRO_ANDROID_KEY_PASSWORD
        if($LASTEXITCODE -ne 0){throw 'Release signing key creation failed'}
    }
    & "$root/android/gradlew.bat" -p "$root/android" :app:assembleRelease :app:lintRelease
    if($LASTEXITCODE -ne 0){throw 'Android release build failed'}
    $signer=Get-ChildItem "$AndroidSdk/build-tools/*/apksigner.bat" | Sort-Object FullName -Descending | Select-Object -First 1
    if(!$signer){throw 'Android apksigner not found'}
    $apk=Join-Path $root 'android/app/build/outputs/apk/release/app-release.apk'
    & $signer.FullName verify --verbose --print-certs $apk
    if($LASTEXITCODE -ne 0){throw 'APK signature verification failed'}
    $metadata=Get-Content "$root/android/app/build/outputs/apk/release/output-metadata.json" -Raw | ConvertFrom-Json
    $version=$metadata.elements[0].versionName
    $output=Join-Path $root 'out/Android'
    New-Item -ItemType Directory -Force $output | Out-Null
    $target=Join-Path $output "AstroPhotoRemote-$version.apk"
    Copy-Item -LiteralPath $apk -Destination $target -Force
    $hash=(Get-FileHash $target -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText("$target.sha256",$hash+'  '+[IO.Path]::GetFileName($target)+[Environment]::NewLine)
    Write-Output "Release APK: $target"
    Write-Output 'Back up android/.signing securely. It is excluded from Git and required for future Android updates.'
} finally {
    Remove-Item Env:ASTRO_ANDROID_KEYSTORE,Env:ASTRO_ANDROID_STORE_PASSWORD,Env:ASTRO_ANDROID_KEY_PASSWORD -ErrorAction SilentlyContinue
}
