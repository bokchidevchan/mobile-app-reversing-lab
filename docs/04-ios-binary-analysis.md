# 04. iOS 바이너리 분석 (Mach-O)

Android를 어느 정도 익히고 iOS를 보면, 도구 이름보다 먼저 "왜 이렇게 손이 더 가나"를 이해해야 한다.
차이의 뿌리는 컴파일 결과물의 형태다.

## Android는 바이트코드, iOS는 네이티브 기계어

- Android의 DEX는 **바이트코드**다. 원본 구조가 꽤 남아서 jadx가 Java 소스에 가깝게 되돌린다. 진짜 디컴파일.
- iOS의 Mach-O는 처음부터 **ARM64 기계어**로 컴파일된다. 되돌릴 소스 정보가 없다. jadx 같은 게 존재할 수 없다.

그래서 iOS에선 "디컴파일"이라는 말을 조심해서 써야 한다. 실제로 하는 일은 대부분 이렇다.

- 구조 읽기: `otool -h`(헤더), `otool -l`(로드 커맨드, 세그먼트)
- 심볼 읽기: `nm`
- 문자열 읽기: `strings`
- 코드 로직 보기: **디스어셈블**(`otool -tV`, 또는 Ghidra/Hopper/IDA로 어셈블리를 의사 C로)

디컴파일(소스 복원)이 아니라 디스어셈블(기계어를 어셈블리로)이 기본이다.
Ghidra 같은 디컴파일러가 어셈블리를 C 비슷하게 올려주긴 하지만 원본 소스는 아니고 근사치다.

## Mach-O 구조

Mach-O 파일 첫 4바이트가 매직이다.

- `cf fa ed fe` = 64비트 Mach-O(MH_MAGIC_64). Android DEX의 `dex\n035\0`에 대응하는 자리.
- `ca fe ba be` = **Fat/Universal 바이너리**. 여러 아키텍처(arm64, x86_64)를 한 파일에 담은 것. `lipo`로 쪼갠다.

세그먼트(`otool -l`로 확인):

- `__TEXT`: 실행 코드(`__text`)와 읽기 전용 상수(`__const`, `__cstring`). 하드코딩 문자열이 여기 산다.
- `__DATA`, `__DATA_CONST`: 초기화된 데이터, 포인터.
- `__PAGEZERO`: 널 포인터 접근을 잡는 맨 앞 빈 영역.

## IPA 구조와 FairPlay 암호화

`.ipa`도 ZIP이다. 풀면 `Payload/앱이름.app/` 안에 Mach-O 실행 파일이 있다.

Android와 결정적으로 다른 점: **App Store에서 받은 앱은 FairPlay DRM으로 암호화돼 있다.**
디스크의 바이너리를 그냥 뜯으면 암호화된 상태라 코드가 안 보인다.
Mach-O 로드 커맨드의 `LC_ENCRYPTION_INFO`에서 `cryptid=1`이면 암호화된 것.

그래서 분석 전에 복호화해야 한다. 실행 중에는 메모리에서 복호화되니, 탈옥 기기에서 그 순간을 덤프한다
(`frida-ios-dump`, `dumpdecrypted`). 내가 직접 빌드하거나 개발자 서명으로 설치한 앱은 암호화가 없어서 이 단계가 필요 없다.

## 서명과 설치

- Android는 셀프 서명이라 아무나 서명해서 설치한다.
- iOS는 Apple 코드사이닝, 프로비저닝 프로파일, entitlements가 필요하다. 아무 IPA나 설치 못 한다.
  개발자 인증서로 재서명(`codesign`, `ldid`)하거나 탈옥 기기가 있어야 한다.

## 동적 분석 환경

여기가 iOS 실습의 제일 큰 벽이다.

- Android는 에뮬레이터(AVD)로 App Store 앱까지 다 돌린다.
- iOS는 **App Store 앱을 돌릴 공식 에뮬레이터가 없다.** Xcode 시뮬레이터는 시뮬레이터 전용 빌드만 돌리지,
  실기기용 ARM 바이너리(FairPlay 암호화된 그것)는 못 돌린다.

현실적인 iOS 동적 분석 경로는 세 가지다.

1. 시뮬레이터 + Frida: 오픈소스 취약 앱(iGoat-Swift, DVIA-v2)을 소스로 빌드해 시뮬레이터에서 돌리고 후킹. 탈옥 불필요.
2. 탈옥 실기기: App Store 앱을 복호화 덤프하고 후킹. 진짜 실무 경로.
3. Corellium: 클라우드 가상 iOS 기기(유료).

Frida/objection 자체는 Android와 똑같이 쓴다. 붙는 대상이 탈옥 기기나 시뮬레이터라는 것만 다르다.

## 이 저장소에서 한 실습 범위

`tools/macho-crackme.c`로 작은 Mach-O를 직접 빌드해서 세 가지를 해봤다.

- 정적: otool/nm/strings로 구조·심볼·문자열 읽고, __const의 XOR 상수를 꺼내 복원
- 디컴파일: Ghidra 헤드리스로 check 함수를 의사 C로 뽑음 (`tools/DecompileCheck.java`).
  Ghidra 12는 JDK 21이 필요하니 JAVA_HOME을 21로 잡아야 헤드리스가 돈다
- 동적: Frida로 strcmp 후킹해서 런타임에 정답 노출

실습 기록은 `notes/04-ios-macho-실습.md`.
단, 이건 macOS arm64 네이티브 바이너리라 메커니즘은 iOS와 같지만 실기기/시뮬레이터 앱 분석은 다음 단계다.
