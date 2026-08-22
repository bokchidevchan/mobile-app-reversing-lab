#!/usr/bin/env python3
"""APK 컴플라이언스 감사 - 정책/규제 검수 관점의 1차 스캐너.

앱을 실행하지 않고 정적으로 뽑는다:
  - 패키지/타깃 SDK
  - 위험 플래그(debuggable, allowBackup, cleartextTraffic)
  - 권한 목록 + 위험(dangerous) 권한 강조
  - exported 컴포넌트(공격면)
  - 알려진 트래커/광고 SDK(개인정보 수집 신호)
  - 하드코딩된 http(s) 엔드포인트

정책 위반을 단정하지 않는다. "검토할 후보"를 모아 사람이 판단하게 돕는 도구다.
사용: python3 apk-audit.py <app.apk>  (aapt2 필요)
"""
import sys, subprocess, re, zipfile, os, shutil

DANGEROUS = {
    "ACCESS_FINE_LOCATION","ACCESS_BACKGROUND_LOCATION","ACCESS_COARSE_LOCATION",
    "READ_CONTACTS","WRITE_CONTACTS","READ_SMS","SEND_SMS","RECEIVE_SMS",
    "READ_CALL_LOG","WRITE_CALL_LOG","RECORD_AUDIO","CAMERA","READ_PHONE_STATE",
    "READ_PHONE_NUMBERS","READ_EXTERNAL_STORAGE","WRITE_EXTERNAL_STORAGE",
    "READ_CALENDAR","WRITE_CALENDAR","BODY_SENSORS","GET_ACCOUNTS","QUERY_ALL_PACKAGES",
    "REQUEST_INSTALL_PACKAGES","SYSTEM_ALERT_WINDOW","READ_MEDIA_IMAGES","READ_MEDIA_VIDEO",
}
TRACKERS = {
    "com.google.android.gms.ads":"Google AdMob",
    "com.facebook":"Facebook SDK",
    "com.appsflyer":"AppsFlyer",
    "com.adjust.sdk":"Adjust",
    "io.branch":"Branch",
    "com.amplitude":"Amplitude",
    "com.mixpanel":"Mixpanel",
    "com.flurry":"Flurry",
    "com.unity3d.ads":"Unity Ads",
    "com.bytedance":"ByteDance/TikTok SDK",
    "com.tencent":"Tencent SDK",
}

def aapt2():
    p = shutil.which("aapt2")
    if p: return p
    sdk = os.environ.get("ANDROID_SDK_ROOT","/opt/homebrew/share/android-commandlinetools")
    import glob
    c = sorted(glob.glob(f"{sdk}/build-tools/*/aapt2"))
    return c[-1] if c else None

def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True).stdout

def main(apk):
    a2 = aapt2()
    if not a2: sys.exit("aapt2 없음 (Android build-tools 설치 필요)")
    badging = run([a2,"dump","badging",apk])
    perms = run([a2,"dump","permissions",apk])

    print(f"# APK 감사 리포트: {os.path.basename(apk)}\n")
    m = re.search(r"package: name='([^']+)'.*?versionName='([^']*)'", badging)
    if m: print(f"패키지: {m.group(1)}  버전: {m.group(2)}")
    for k,label in [("sdkVersion","minSdk"),("targetSdkVersion","targetSdk")]:
        mm = re.search(rf"{k}:'(\d+)'", badging)
        if mm: print(f"{label}: {mm.group(1)}")

    print("\n## 위험 플래그")
    flags = []
    if "application-debuggable" in badging: flags.append("debuggable=true (릴리스에 있으면 안 됨)")
    # allowBackup / cleartext 는 매니페스트에서
    mani = run([a2,"dump","xmltree",apk,"--file","AndroidManifest.xml"])
    if re.search(r"allowBackup.*=.*\(type 0x12\)0xffffffff", mani): flags.append("allowBackup=true (데이터 백업 유출 가능)")
    if re.search(r"usesCleartextTraffic.*=.*\(type 0x12\)0xffffffff", mani): flags.append("usesCleartextTraffic=true (평문 통신 허용)")
    print("\n".join(f"  - {f}" for f in flags) if flags else "  (특이 플래그 없음)")

    print("\n## 권한")
    plist = re.findall(r"uses-permission: name='android\.permission\.([^']+)'", perms)
    if not plist: print("  (선언된 권한 없음)")
    for p in plist:
        mark = "  [위험]" if p in DANGEROUS else ""
        print(f"  - {p}{mark}")

    # 파일 목록 (프레임워크 감지에 사용)
    try:
        with zipfile.ZipFile(apk) as z: names = z.namelist()
    except Exception: names = []
    def has(sub): return any(sub in n for n in names)

    print("\n## 앱 프레임워크 감지 (코드가 어디 있나)")
    fw = []
    if has("libflutter.so") or has("flutter_assets/") or has("libapp.so"):
        fw.append("Flutter — 코드는 Dart AOT 스냅샷(lib/*/libapp.so). jadx 무의미, blutter/reFlutter 필요")
    if has("index.android.bundle") or has("libreactnativejni.so") or has("libhermes"):
        fw.append("React Native — 로직은 assets/index.android.bundle(JS/Hermes). JS 번들 분석")
    if has("assets/www/") or has("cordova.js"):
        fw.append("Cordova/Ionic(하이브리드) — 로직은 assets/www의 HTML/JS")
    if has("libmonodroid.so") or has("assemblies/"):
        fw.append(".NET/Xamarin/MAUI — 로직은 .NET 어셈블리(assemblies/). dnSpy/ILSpy")
    if has("libunity.so") or has("bin/Data/"):
        fw.append("Unity — 로직은 IL2CPP(libil2cpp.so) 또는 Mono 어셈블리")
    if not fw:
        fw.append("네이티브 Java/Kotlin (기본) — classes.dex가 주 코드 → jadx로 분석")
    for f in fw: print(f"  - {f}")

    print("\n## exported 컴포넌트 (공격면)")
    exported = re.findall(r"E: (activity|service|receiver|provider).*?name.*?='([^']+)'", mani)
    # 간단 신호: launchable-activity + provider
    la = re.search(r"launchable-activity: name='([^']+)'", badging)
    if la: print(f"  - launcher: {la.group(1)}")
    print("  (정밀 분석은 apktool로 매니페스트 디코드해 android:exported 확인)")

    print("\n## 트래커/광고 SDK 신호 (dex 문자열 기반)")
    strings_blob = ""
    try:
        with zipfile.ZipFile(apk) as z:
            for n in z.namelist():
                if n.endswith(".dex"):
                    data = z.read(n)
                    strings_blob += data.decode("latin-1", "ignore")
    except Exception as e:
        print("  dex 읽기 실패:", e)
    found = [(pref,lab) for pref,lab in TRACKERS.items() if pref.replace(".","/") in strings_blob or pref in strings_blob]
    print("\n".join(f"  - {lab} ({pref})" for pref,lab in found) if found else "  (알려진 트래커 신호 없음)")

    print("\n## 하드코딩된 엔드포인트 (상위 15)")
    urls = sorted(set(re.findall(r"https?://[A-Za-z0-9._~:/?#\[\]@!$&'()*+,;=%-]{6,60}", strings_blob)))
    urls = [u for u in urls if not u.startswith(("http://schemas.android.com","http://www.w3.org","http://apache.org","http://xml.org"))]
    for u in urls[:15]: print(f"  - {u}")
    if not urls: print("  (없음)")

if __name__ == "__main__":
    if len(sys.argv) < 2: sys.exit("사용: python3 apk-audit.py <app.apk>")
    main(sys.argv[1])
