# 05. Android vs iOS 분석은 어떻게 다른가

이 랩에서 Android(UnCrackable L1/L2)와 iOS(Mach-O 크랙미)를 각각 분석하면서 느낀 차이를 한 곳에 정리한다.
개별 개념은 02(Android 도구), 03(Frida), 04(iOS Mach-O)에 있고, 여기선 두 플랫폼을 나란히 놓고 본다.

## 한눈 비교

| 항목 | Android | iOS |
|------|---------|-----|
| 배포 패키지 | APK (ZIP) | IPA (ZIP) |
| 안의 실행 코드 | classes.dex (바이트코드) | 앱.app/앱 (Mach-O 네이티브) |
| 컴파일 형태 | DEX 바이트코드 | ARM64 기계어 |
| 소스 복원 | jadx로 Java에 가깝게 | 불가, 디스어셈블/디컴파일러 필요 |
| 대표 정적 도구 | jadx, apktool | otool, nm, strings, Ghidra |
| 이름(심볼) | DEX에 이름 풍부 | ObjC 남음 / Swift 뭉개짐 |
| 배포본 암호화 | 없음(평문 ZIP) | App Store는 FairPlay 암호화 |
| 서명/설치 | 셀프 서명, 자유 설치 | Apple 코드사이닝, 탈옥/재서명 필요 |
| 동적 분석 환경 | 에뮬레이터(AVD)로 다 됨 | 탈옥 기기 / 시뮬레이터 / gadget 주입 |
| Frida 후킹 대상 | Java Cipher, 앱 메서드 | CCCrypt, Keychain, ObjC/Swift |

## 1. 컴파일 결과물이 다르다 (차이의 뿌리)

- Android: Kotlin/Java → JVM 바이트코드 → DEX 바이트코드. CPU 독립적인 중간 표현이 APK에 실린다.
  메타데이터(클래스명, 메서드명, 타입)가 풍부해서 jadx가 Java 소스에 가깝게 되돌린다.
- iOS: Swift/ObjC → LLVM → 네이티브 ARM64 기계어로 바로 컴파일해서 Mach-O로 배포.
  바이트코드가 안 실린다(Bitcode는 Xcode 14에서 폐기). 메타데이터가 적어 jadx 같은 게 불가능.

이유: Android는 여러 제조사/CPU 이식성 때문에 VM+바이트코드, iOS는 단일 하드웨어라 성능/크기/통제 위해 네이티브 AOT.
그래서 iOS 리버싱이 한 단계 더 손이 간다.

## 2. 정적 분석 흐름이 다르다

Android(L1)에서는 이랬다.

- jadx로 열면 바로 읽을 수 있는 Java가 나온다. `if (a.a(str))` 같은 로직이 그대로 보인다.
- 정답이 AES로 숨겨져 있어도 키와 암호문이 코드에 있어서, openssl로 밖에서 복호화하면 끝.

iOS(Mach-O)에서는 이랬다.

- jadx 같은 게 없다. otool -h/-l로 구조, nm으로 심볼, strings로 문자열을 읽는다.
- 로직을 읽으려면 디스어셈블(otool -tV)하거나 Ghidra로 의사 C까지 올려야 한다.
- 어셈블리는 mov/ldr/bl 같은 CPU 명령이라, jadx의 Java와 읽는 난이도가 다르다.

## 3. 이름이 남느냐

- Android DEX: 이름이 많이 남아 읽기 좋다. 난독화하면 a/b/c로 뭉개지지만 구조는 보인다.
- iOS ObjC: 메서드 이름이 심볼에 남는다(`-[Class method:]`). objc_msgSend로 동적 디스패치라 이름으로 후킹도 쉽다.
- iOS Swift: 이름이 뭉개진다(`$s...`). swift-demangle로 풀어야 읽히고, 정적 디스패치라 후킹은 주소 기반으로 해야 한다.

같은 메서드를 ObjC와 Swift로 빌드해 nm으로 비교한 기록은 notes/04에 있다.

## 4. 동적 분석 환경이 다르다

- Android: 에뮬레이터(AVD)로 App Store 앱까지 다 돌린다. `adb root`되는 이미지에 frida-server 올리면 끝.
- iOS: App Store 앱을 돌릴 공식 에뮬레이터가 없다. 세 갈래다.
  - 탈옥 기기: frida-server를 root로. 가장 강력.
  - 비탈옥 기기: IPA에 FridaGadget.dylib 주입 후 재서명. 앱 하나만.
  - 시뮬레이터: 소스로 빌드한 앱을 macOS 프로세스로 후킹(이 랩의 네이티브 데모가 이 방식).

## 5. Frida 후킹 대상이 다르다

원리는 같다. 복호화하려면 키가 메모리에 떠야 하니 그 순간을 가로챈다. 표적만 다르다.

- Android: Java `Cipher`, 앱 자체 복호화 메서드(L1의 sg.vantagepoint.a.a.a 후킹해서 정답 추출).
- iOS: CommonCrypto `CCCrypt`(키가 인자로 들어옴), Keychain API, ObjC/Swift 메서드.
  이 랩에서 CCCrypt를 후킹해 AES 키를 런타임에 뽑는 것까지 해봤다(notes/04).

## 6. 배포본 암호화와 서명

- APK는 평문 ZIP이라 그냥 풀린다. 서명은 셀프 서명이고 v1~v4 스킴(01번 문서).
- App Store IPA는 FairPlay DRM으로 암호화돼 있다. 정적 분석 전에 탈옥 기기에서 복호화 덤프가 필요하다.
  설치도 Apple 코드사이닝/프로비저닝이 있어 아무 IPA나 못 넣는다.

## 이 랩에서 실제로 다르게 한 것 정리

- Android L1: jadx로 Java 확인 → 키/암호문 뽑아 openssl로 정적 복호화 → Frida로 동적 추출.
- Android L2: 검증이 네이티브라 jadx가 막힘 → Ghidra로 libfoo.so 디컴파일 → 조각난 정답 조립.
- iOS Mach-O: otool/nm/strings로 구조 → Ghidra로 의사 C → XOR 정적 복원 → Frida로 strcmp/CCCrypt 후킹.

한 줄 결론: Android는 바이트코드라 "고수준+메타데이터"라 빠르게 읽히고, iOS는 네이티브라 "저수준+메타데이터 빈약"이라
디스어셈블/디컴파일러를 한 겹 더 거친다. 동적 분석은 Android가 에뮬레이터로 수월하고, iOS는 실행 환경 확보 자체가 관문이다.
