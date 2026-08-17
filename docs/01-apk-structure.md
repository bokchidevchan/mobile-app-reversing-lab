# 01. APK 구조와 빌드 산출물

## APK는 그냥 ZIP이다

`.apk`는 압축 컨테이너다. 확장자만 바꾸면 `unzip`으로 그대로 풀린다.

```
unzip -l app.apk
```

풀면 대략 이런 구조가 나온다.

```
app.apk
├── AndroidManifest.xml     # 바이너리 XML (평문 아님)
├── classes.dex             # 컴파일된 바이트코드 (여러 개일 수 있음: classes2.dex ...)
├── resources.arsc          # 컴파일된 리소스 테이블
├── res/                    # 이미지, 레이아웃 등 (일부는 arsc에 인덱싱)
├── lib/                    # 네이티브 .so (arm64-v8a, armeabi-v7a, x86_64 ...)
├── assets/                 # 앱이 직접 읽는 원본 파일 (Flutter의 libapp.so, 모델 파일 등)
└── META-INF/               # 서명 정보 (MANIFEST.MF, CERT.SF, CERT.RSA ...)
```

핵심은 `AndroidManifest.xml`이 사람이 읽는 XML이 아니라 **바이너리 XML**이라는 점이다.
그냥 열면 깨져 보이고, `apktool`이나 `aapt2 dump xmltree`로 디코딩해야 읽힌다.

## classes.dex (Dalvik Executable)

Java/Kotlin 소스가 `.class`(JVM 바이트코드)로 컴파일된 다음, `d8`/`r8`을 거쳐 `.dex`로 변환된다.
JVM은 스택 기반, DEX는 **레지스터 기반** 바이트코드라는 게 큰 차이다.

- 메서드 참조 한계(64K, 이른바 "65536 methods" 문제) 때문에 멀티덱스(`classes2.dex`, `classes3.dex`)로 쪼개진다.
- 정적 분석의 출발점. `jadx`가 dex를 다시 Java 비슷한 소스로 역컴파일해 보여준다.

확인:

```
jadx-gui app.apk         # GUI로 열어서 클래스 트리 탐색
jadx -d out/ app.apk     # 소스로 추출
```

## resources.arsc

문자열, 색상, 치수 같은 리소스를 ID로 매핑한 테이블. `R.string.app_name` 같은 참조가 여기로 연결된다.
문자열이 여기 평문으로 박혀 있는 경우가 많아서, 하드코딩된 URL이나 키 문자열을 찾을 때 먼저 본다.

## AAB (Android App Bundle) vs APK

- **APK**: 디바이스에 실제로 설치되는 최종 산출물.
- **AAB**: Play Store에 올리는 **발행 포맷**. 그 자체로는 설치 불가.
  Google Play가 AAB를 받아서 디바이스별로 최적화된 **split APK**(언어별, 화면 밀도별, ABI별)를 생성해 내려준다.
- 로컬에서 AAB를 설치 가능한 APK 세트로 만들려면 `bundletool`을 쓴다.

```
bundletool build-apks --bundle=app.aab --output=app.apks
```

분석 관점에서는, 스토어에서 받은 앱은 이미 split APK로 쪼개져 있을 수 있다는 점을 기억해야 한다.
`base.apk` 하나만 봐서는 리소스나 네이티브 라이브러리가 다른 split에 있어 안 보일 수 있다.

## 서명 (Signing) v1 ~ v4

APK는 서명 없이는 설치되지 않는다. 서명 스킴은 네 세대가 있다.

| 스킴 | 도입 | 서명 대상 | 특징 |
|------|------|-----------|------|
| v1 (JAR) | 초기 | 각 파일 개별 (META-INF) | ZIP 메타데이터는 보호 안 됨, 변조 여지 |
| v2 | Android 7.0 | APK 전체 블록 | 파일 단위가 아니라 통짜로 무결성 검증 |
| v3 | Android 9 | v2 + 키 회전 | 서명 키 교체(rotation) 지원 |
| v4 | Android 11 | 별도 `.idsig` 파일 | 스트리밍 설치(증분) 지원 |

검수/분석 관점에서 볼 것:

- 어떤 스킴으로 서명됐는지 (`apksigner verify -v --print-certs app.apk`)
- 서명 인증서의 주체(CN, 조직). 리패키징된 앱은 원 개발사 인증서와 다르다.
- v1만 있는 앱은 상대적으로 오래됐거나 변조 방어가 약할 수 있다.

```
apksigner verify -v --print-certs app.apk
keytool -printcert -jarfile app.apk
```

## 오늘 확인한 것 / 남은 의문

- APK를 unzip해서 6개 주요 구성요소를 눈으로 확인.
- AndroidManifest가 바이너리 XML이라 디코딩 도구가 필요하다는 것.
- 서명 스킴 v1과 v2+의 무결성 보장 범위 차이.

남은 의문:
- split APK로 쪼개진 앱을 하나로 합쳐서 분석하는 실무 흐름 (다음에 `bundletool`이나 병합 도구로 실습).
- v3 키 회전이 실제로 검증에서 어떻게 드러나는지.
