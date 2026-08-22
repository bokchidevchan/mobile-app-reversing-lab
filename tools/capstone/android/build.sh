#!/usr/bin/env bash
# 캡스톤 난독화 Android 앱 수동 빌드 (Gradle 없이).
# NDK로 네이티브 .so 빌드 -> javac/d8 -> aapt2 link -> dex/so 삽입 -> zipalign -> apksigner.
# 경로는 각자 환경에 맞게. 산출물(.so/.apk)은 커밋하지 않는다.
set -e
SDK="${ANDROID_SDK_ROOT:-/opt/homebrew/share/android-commandlinetools}"
NDK="$SDK/ndk/29.0.14206865"
BT="$SDK/build-tools/35.0.1"
ANDROID_JAR="$SDK/platforms/android-34/android.jar"
TC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin"
HERE="$(cd "$(dirname "$0")" && pwd)"
B="$HERE/build"; mkdir -p "$B/lib/arm64-v8a" "$B/classes"

# 1) 네이티브 .so (최적화 + 심볼 스트립)
"$TC/aarch64-linux-android24-clang" -O2 -fvisibility=hidden -shared -fPIC \
  -o "$B/lib/arm64-v8a/libverify.so" "$HERE/jni/verify.c" -llog
"$TC/llvm-strip" --strip-all "$B/lib/arm64-v8a/libverify.so"

# 2) Java -> dex
javac -source 8 -target 8 -bootclasspath "$ANDROID_JAR" -classpath "$ANDROID_JAR" \
  -d "$B/classes" "$HERE/src/com/example/capstone/MainActivity.java"
"$BT/d8" --lib "$ANDROID_JAR" --output "$B" $(find "$B/classes" -name '*.class')

# 3) aapt2 link -> 매니페스트/리소스 담은 기본 apk
"$BT/aapt2" link -I "$ANDROID_JAR" --manifest "$HERE/AndroidManifest.xml" \
  -o "$B/app-base.apk" --min-sdk-version 24 --target-sdk-version 34

# 4) dex + so 삽입
cd "$B"; cp app-base.apk app-unsigned.apk
zip -q app-unsigned.apk classes.dex
zip -q app-unsigned.apk lib/arm64-v8a/libverify.so

# 5) 정렬 + 서명
"$BT/zipalign" -f 4 app-unsigned.apk app-aligned.apk
[ -f debug.keystore ] || keytool -genkeypair -keystore debug.keystore -storepass android \
  -keypass android -alias dk -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=capstone"
"$BT/apksigner" sign --ks debug.keystore --ks-pass pass:android --key-pass pass:android \
  --out capstone.apk app-aligned.apk
echo "빌드 완료: $B/capstone.apk"
