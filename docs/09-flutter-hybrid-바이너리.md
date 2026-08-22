# 09. Flutter / Hybrid 앱의 바이너리 특성

APK를 열었는데 `classes.dex`가 거의 비어 있으면, 코드가 다른 데 있다는 신호다.
Flutter나 하이브리드 프레임워크로 만든 앱이 그렇다. jadx만 보면 "앱이 아무것도 안 한다"고 착각한다.

검수 관점에서 제일 먼저 할 일: "이 앱이 무엇으로 만들어졌나(코드가 어디 있나)"를 판별하는 것.
apk-audit.py에 프레임워크 감지를 넣었다.

## 프레임워크별로 코드가 있는 곳

| 프레임워크 | 감지 파일 | 코드 위치 | 분석 도구 |
|---|---|---|---|
| 네이티브 Java/Kotlin | classes.dex | DEX 바이트코드 | jadx |
| Flutter | libflutter.so, libapp.so, flutter_assets/ | Dart AOT 스냅샷(libapp.so) | blutter, reFlutter |
| React Native | index.android.bundle, libhermes/libreactnativejni | JS 번들(또는 Hermes 바이트코드) | JS beautify, hermes-dec |
| Cordova/Ionic | assets/www/, cordova.js | HTML/JS | 그냥 열어보면 됨 |
| .NET/Xamarin/MAUI | libmonodroid.so, assemblies/ | .NET 어셈블리 | dnSpy, ILSpy |
| Unity | libunity.so, libil2cpp.so, bin/Data/ | IL2CPP 또는 Mono | Il2CppDumper |

감지 예시(합성 Flutter APK):

```
## 앱 프레임워크 감지 (코드가 어디 있나)
  - Flutter — 코드는 Dart AOT 스냅샷(lib/*/libapp.so). jadx 무의미, blutter/reFlutter 필요
```

## Flutter가 특히 까다로운 이유

- **libflutter.so** = 엔진(공통, 분석 대상 아님). **libapp.so** = 우리 앱의 Dart 코드가
  AOT 컴파일된 **스냅샷**. 이게 표준 ELF 코드가 아니라 Dart 런타임이 읽는 커스텀 포맷이다.
- 스냅샷 포맷이 **Flutter/Dart 버전마다 바뀐다.** 그래서 Ghidra로 열어도 함수 경계가 잘 안 잡히고,
  전용 파서(blutter)가 그 버전에 맞아야 클래스/메서드 이름이 복원된다.
- reFlutter는 엔진(libflutter.so)을 패치해서 런타임에 스냅샷 구조를 덤프하거나 로그를 남기게 만든다.

## 네트워크 검수의 함정: Flutter는 프록시를 무시한다

이게 실무에서 제일 잘 걸린다.

- Flutter의 Dart 네트워킹(dart:io HttpClient)은 **시스템 프록시 설정을 기본으로 안 탄다.**
  그래서 mitmproxy를 켜고 기기 프록시를 걸어도 Flutter 앱 트래픽은 안 잡히는 경우가 많다.
- 게다가 Dart는 **자체 BoringSSL + 자체 CA 번들**을 써서, 시스템 CA에 mitm 인증서를 넣어도 안 통한다.
- 대응: reFlutter로 프록시/CA를 강제하도록 엔진을 패치하거나, Frida로 Dart의 SSL 검증 함수를 후킹한다.

즉 "mitmproxy 켰는데 아무것도 안 잡힌다"면 앱이 Flutter인지부터 의심해야 한다.

## 하이브리드(WebView) 앱

Cordova/Ionic 같은 건 로직이 `assets/www`의 HTML/JS라 오히려 읽기 쉽다.
대신 검수 포인트가 다르다: 원격 URL을 로드하는지(원격 코드 실행), addJavascriptInterface로
네이티브 기능을 JS에 과하게 노출하는지, 로컬 파일 접근 범위 등.

## 정리

- APK 열면 먼저 프레임워크부터 판별한다. classes.dex만 보고 판단하면 틀린다.
- Flutter면 jadx가 무의미 → libapp.so를 blutter/reFlutter로.
- Flutter는 프록시·시스템 CA를 우회하므로 네트워크 검수에 별도 처리가 필요하다.
- 하이브리드는 코드가 JS/HTML이라 읽기 쉽지만 원격 로드·브릿지 노출이 검수 포인트.

## 다음

- 실제 Flutter 앱을 직접 빌드(Flutter SDK)하거나 공개 샘플로 libapp.so를 blutter로 파싱해보기.
- reFlutter로 엔진 패치 후 mitmproxy로 Flutter 트래픽 잡기.
