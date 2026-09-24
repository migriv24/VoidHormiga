# build_apk.ps1 -- build, package and sign the Void Hormiga APK, without Gradle:
# NDK CMake -> aapt2 link (manifest + assets) -> aapt add (lib, dex) ->
# zipalign -> apksigner. Interaction Combinators' script is the template.
# Requires an Android SDK (ndk, platforms, build-tools, cmake) and Android
# Studio's bundled JBR (keytool, apksigner). Run from anywhere:
#   powershell -File android\build_apk.ps1 [-Install]
#
# THE SIGNING KEY. The first run mints android\hormiga-release.keystore and a
# random password in android\hormiga-release.keystore.pass. Both are gitignored
# and must be BACKED UP: Android accepts an update only when it is signed by the
# same key, so a lost key means every phone uninstalls to update. See
# android\README.md.
param([switch]$Install)
# Native tools print progress to stderr (keytool does), and Windows PowerShell 5.1
# turns stderr into a terminating error under "Stop" whenever output is
# redirected: the first run died halfway through minting the key that way. So
# native calls are judged by $LASTEXITCODE (checked after every one), and the
# file operations that matter stop on error explicitly.
$ErrorActionPreference = "Continue"
$Abi = "arm64-v8a" # the one ABI vendor\libsodium is built for

$here = $PSScriptRoot
$repo = Split-Path $here -Parent

# the version: the root CMakeLists' project() (mago doctor keeps void.json equal)
$m = Select-String -Path "$repo\CMakeLists.txt" -Pattern 'project\(voidhormiga VERSION ([0-9]+)\.([0-9]+)\.([0-9]+)'
if (-not $m) { throw "no version in CMakeLists.txt" }
$g = $m.Matches[0].Groups
$version = "$($g[1].Value).$($g[2].Value).$($g[3].Value)"
$versionCode = [int]$g[1].Value * 10000 + [int]$g[2].Value * 100 + [int]$g[3].Value

$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
if (-not (Test-Path $sdk)) { throw "Android SDK not found at $sdk (set ANDROID_HOME)" }
$ndk = Get-ChildItem "$sdk\ndk" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$bt = Get-ChildItem "$sdk\build-tools" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$platform = Get-ChildItem "$sdk\platforms" -Directory |
    Sort-Object { [double]($_.Name -replace "android-", "") } -Descending | Select-Object -First 1
$cmakeDir = Get-ChildItem "$sdk\cmake" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$cmake = "$($cmakeDir.FullName)\bin\cmake.exe"
$ninja = "$($cmakeDir.FullName)\bin\ninja.exe"
$jbr = "C:\Program Files\Android\Android Studio\jbr"
if (-not (Test-Path "$jbr\bin\java.exe")) { throw "JBR not found at $jbr" }
Write-Host "Void Hormiga $version ($versionCode) | ndk $($ndk.Name) | build-tools $($bt.Name) | $($platform.Name) | $Abi"

# 1. the native library: the ROOT CMakeLists, which on ANDROID builds only
#    libvoidhormiga.so (android\hormiga_android.cmake)
$build = "$here\build"
& $cmake -S $repo -B $build -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$($ndk.FullName)\build\cmake\android.toolchain.cmake" `
    "-DANDROID_ABI=$Abi" "-DANDROID_PLATFORM=android-26" "-DANDROID_STL=c++_static" `
    "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE) { throw "cmake configure failed" }
& $cmake --build $build --target voidhormiga
if ($LASTEXITCODE) { throw "native build failed" }

# 2. stage: the library at its APK path, and the fonts as assets (the desktop
#    reads vendor\fonts beside its binary; an APK has no such folder)
$stage = "$build\apk"
Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue # absent on a first run
New-Item -ItemType Directory -Force "$stage\lib\$Abi", "$stage\assets\fonts" -ErrorAction Stop | Out-Null
$so = Get-ChildItem $build -Recurse -Filter "libvoidhormiga.so" | Select-Object -First 1
if (-not $so) { throw "libvoidhormiga.so not built" }
# stripped: the NDK compiles with -g even in Release (Gradle strips at packaging;
# we have no Gradle), and the debug info is 9/10ths of an unstripped library
$strip = "$($ndk.FullName)\toolchains\llvm\prebuilt\windows-x86_64\bin\llvm-strip.exe"
& $strip --strip-unneeded -o "$stage\lib\$Abi\libvoidhormiga.so" $so.FullName
if ($LASTEXITCODE) { throw "llvm-strip failed" }
foreach ($f in "Lato-Regular.ttf", "fa-solid-900.ttf", "JetBrainsMono.ttf") {
    Copy-Item "$repo\vendor\fonts\$f" "$stage\assets\fonts\" -Force -ErrorAction Stop
}

# 3. manifest + assets -> base apk
$unaligned = "$build\unaligned.apk"
& "$($bt.FullName)\aapt2.exe" link -o $unaligned `
    --manifest "$here\AndroidManifest.xml" -A "$stage\assets" `
    -I "$($platform.FullName)\android.jar" `
    --min-sdk-version 26 --target-sdk-version 34 `
    --version-code $versionCode --version-name $version
if ($LASTEXITCODE) { throw "aapt2 link failed" }

# 4. the library (a relative path with forward slashes is the in-APK path)
Push-Location $stage
& "$($bt.FullName)\aapt.exe" add $unaligned "lib/$Abi/libvoidhormiga.so"
$aaptExit = $LASTEXITCODE
Pop-Location
if ($aaptExit) { throw "aapt add failed" }

# 4b. Void Maiz's one Java class (org.voidmaiz.MaizActivity: Android's own
#     keyboard), compiled to classes.dex
$maiz = if ($env:VOIDMAIZ_ROOT) { $env:VOIDMAIZ_ROOT } else { "$repo\..\VoidMaiz" }
& "$maiz\android\build_java.ps1" -OutDir "$build\dex"
Push-Location "$build\dex"
& "$($bt.FullName)\aapt.exe" add $unaligned "classes.dex"
$aaptExit = $LASTEXITCODE
Pop-Location
if ($aaptExit) { throw "aapt add classes.dex failed" }

# 5. align (16 KB pages: an Android 15 requirement)
$aligned = "$build\aligned.apk"
& "$($bt.FullName)\zipalign.exe" -f -P 16 4 $unaligned $aligned
if ($LASTEXITCODE) { throw "zipalign failed" }

# 6. the release key: minted ONCE, then reused for every release forever
$ks = "$here\hormiga-release.keystore"
$passFile = "$ks.pass"
if (-not (Test-Path $ks)) {
    if (Test-Path $passFile) { throw "$passFile exists without its keystore: restore the keystore from backup" }
    $bytes = New-Object byte[] 24
    [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($bytes)
    $pass = [Convert]::ToBase64String($bytes) -replace '[+/=]', ''
    Set-Content -Path $passFile -Value $pass -NoNewline -Encoding ascii -ErrorAction Stop
    & "$jbr\bin\keytool.exe" -genkeypair -keystore $ks -alias hormiga `
        -storepass $pass -keypass $pass -keyalg RSA -keysize 4096 `
        -validity 36500 -dname "CN=Void Hormiga, O=Void"
    if ($LASTEXITCODE) { throw "keytool failed" }
    Write-Host ""
    Write-Host "MINTED the Void Hormiga release key: $ks"
    Write-Host "  and its password: $passFile"
    Write-Host "  BACK BOTH UP NOW. Every future update must be signed with this key."
    Write-Host ""
}
if (-not (Test-Path $passFile)) { throw "$passFile is missing: restore it from backup" }
$pass = (Get-Content $passFile -Raw).Trim()

# 7. sign
$env:JAVA_HOME = $jbr
$out = "$here\VoidHormiga-$version.apk"
& "$($bt.FullName)\apksigner.bat" sign --ks $ks --ks-key-alias hormiga `
    --ks-pass "pass:$pass" --key-pass "pass:$pass" --out $out $aligned
if ($LASTEXITCODE) { throw "apksigner failed" }
# (captured, not piped to Select-Object -First: cutting the pipe short made the
# script exit 255 after a good signature)
$certs = & "$($bt.FullName)\apksigner.bat" verify --print-certs $out
if ($LASTEXITCODE) { throw "the signed APK does not verify" }
$certs | Select-Object -First 3
Write-Host "APK: $out ($([math]::Round((Get-Item $out).Length / 1MB, 2)) MB)"

# 8. optional: install to a connected device
if ($Install) { & "$sdk\platform-tools\adb.exe" install -r $out }
