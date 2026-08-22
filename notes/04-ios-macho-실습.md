# iOS Mach-O 정적/동적 실습 기록

Android로 익힌 흐름을 iOS 쪽 바이너리(Mach-O)로 똑같이 해봤다.
실기기나 IPA 없이, 알맹이 실행 파일만 직접 빌드해서 정적과 동적을 둘 다.

주의: 여기서 만든 건 macOS arm64 네이티브 바이너리다. iOS 앱과 파일 포맷(Mach-O)과
분석 도구가 같아서 연습으론 충분하지만, 실제 iOS 앱(IPA, 코드사이닝, FairPlay)은 다음 단계다.

## 대상 만들기

`tools/macho-crackme.c`. 안드로이드 UnCrackable처럼 정답을 XOR로 숨겼다.

```
clang -arch arm64 -o macho-crackme tools/macho-crackme.c
```

## 정적 분석

Android는 jadx가 Java로 되돌려줬지만, iOS는 그런 게 없다. Mach-O는 네이티브 기계어라
소스로 못 되돌린다. 그래서 디컴파일이 아니라 구조/심볼/문자열을 읽는다.

- `file` / 앞 4바이트: `cf fa ed fe` = 64비트 Mach-O 매직. (Fat 바이너리는 `ca fe ba be`)
- `otool -h`, `otool -l`: 헤더랑 세그먼트(__TEXT, __DATA_CONST, __PAGEZERO). 상수는 __TEXT,__const에 산다.
- `nm`: 심볼. `_check` 함수랑 `_ENC` 데이터가 있다는 것 확인.
- `strings`: "correct/nope/usage"는 보이는데 정답은 안 보였다. XOR로 숨겨놔서.

코드 로직을 보고 싶으면 디스어셈블한다.

```
otool -tV macho-crackme
```

`mov`, `ldr`, `bl` 같은 어셈블리가 나온다. Android의 읽기 좋은 Java와 완전히 다르다.
사람이 읽기 좋게 하려면 Ghidra/Hopper로 의사 C까지 올려야 하는데 이번엔 안 갔다.

## 값 꺼내기 (정적)

정답은 __TEXT,__const에 XOR로 가려진 15바이트로 있었다.
파일에서 그 바이트를 직접 읽고 KEY(0x2b)로 XOR하니 풀렸다.

```
ENC: 42 44 58 74 43 42 4f 4f 4e 45 74 4d 47 4a 4c
각 바이트 ^ 0x2b:
  0x42^0x2b = 0x69 'i'
  0x44^0x2b = 0x6f 'o'
  0x58^0x2b = 0x73 's'
  ...
= ios_hidden_flag
```

XOR은 같은 키로 두 번 하면 원래대로 돌아온다. 앱은 "정답 → XOR → 저장",
나는 "저장된 값 → 같은 키로 XOR → 정답". Android AES를 openssl로 되돌린 것과 원리가 같고 더 단순한 버전.

otool로 __const를 덤프하면 32비트 워드가 리틀엔디언으로 표시돼서 바이트 순서가 헷갈린다.
파일에서 자연 순서로 직접 읽는 게 덜 헷갈렸다.

## 디컴파일처럼 보기 (Ghidra)

otool -tV는 어셈블리(mov/ldr/bl)까지만 보여준다. 로직을 읽기 좋게 보려면 디컴파일러를 쓴다.
Ghidra를 깔아서 헤드리스로 check 함수를 의사 C로 뽑아봤다.

```
analyzeHeadless <proj> <name> -import macho-crackme -postScript DecompileCheck.java -deleteProject
```

주의: Ghidra 12는 JDK 21이 필요하다. JAVA_HOME을 21로 안 잡으면 "no TTY detected"로 죽는다.

결과:

```c
bool _check(char *param_1)
{
  byte abStack_58 [15];
  ...
  for (local_70 = 0; local_70 < 0xf; local_70 = local_70 + 1) {
    abStack_58[local_70] = "BDXtCBOONEtMGJL"[local_70] ^ 0x2b;
  }
  iVar1 = _strcmp(param_1,(char *)abStack_58);
  return iVar1 == 0;
}
```

15번 돌면서 박아둔 문자열을 0x2b로 XOR하고 입력과 strcmp하는 로직이 그대로 보인다.
jadx의 Java와 비교하면 두 가지가 다르다.

- 진짜 소스가 아니라 근사치다. 변수명이 local_70, abStack_58처럼 기계가 붙인 이름.
  원래 이름(ENC, KEY, dec)은 컴파일 때 날아갔다.
- 그래도 XOR 상수(0x2b)랑 숨긴 문자열("BDXtCBOONEtMGJL")을 다 드러내줘서,
  손으로 바이트 읽던 걸 코드가 알아서 보여준다. 여기서 정답 복원은 그대로 XOR 하면 끝.

정리: iOS도 디컴파일처럼 되지만 jadx처럼 바로가 아니라 Ghidra/Hopper를 한 단계 더 거친다.

## 동적 분석 (Frida)

Frida는 macOS 네이티브 프로세스에도 바로 붙는다. 그래서 이 바이너리를 spawn하고,
`check()`가 부르는 `strcmp(입력, 복호화된정답)`을 후킹해서 두 번째 인자를 읽었다.

```
[frida] strcmp("test", "ios_hidden_flag") -> 런타임에 정답 노출: ios_hidden_flag
```

XOR을 내가 안 풀어도, 앱이 비교하려고 이미 풀어놓은 값을 그대로 훔쳐봤다.
Android에서 AES 결과 가로챈 것과 똑같은 그림.

## APK vs IPA 정리

| 계층 | Android | iOS |
|---|---|---|
| 배포 패키지(ZIP) | APK | IPA |
| 안의 실행 코드 | classes.dex (바이트코드) | 앱.app/앱 (Mach-O 기계어) |
| 이번에 분석한 것 | 실제 APK | Mach-O 알맹이만 (직접 빌드) |

## iOS 방식 AES 키 탈취 (CCCrypt + Frida)

iOS 앱은 대부분 CommonCrypto의 CCCrypt로 AES를 한다. 복호화할 때 키가 인자로 들어가니,
그 함수를 후킹해서 인자만 읽으면 키가 털린다. CommonCrypto는 macOS에도 있어서 iOS 없이 연습했다.

`tools/ios-demos/cc-demo.c`로 CCCrypt로 암/복호화하는 데모를 만들고,
`tools/cccrypt-hook.js`로 후킹했다.

```
[+] CCCrypt(암호화) -> 키(16B): "s3cr3t_ios_key!!"
[+] CCCrypt(복호화) -> 키(16B): "s3cr3t_ios_key!!"
```

키를 코드에서 안 찾고, 앱이 CCCrypt를 부르는 순간 인자로 들어온 걸 가로챘다.
Android에서 Cipher/복호화 함수 후킹한 것과 발상이 같다. 실무 표적은 CCCrypt 말고도
Keychain API(SecItemCopyMatching), 앱 자체 암호 메서드, SSL_read/write(네트워크 평문)가 있다.

## ObjC는 이름으로, Swift는 뭉개져서

같은 CryptoManager.decrypt(withKey:)를 ObjC와 Swift로 각각 빌드해서 nm으로 심볼을 비교했다.
(`tools/ios-demos/objc-demo.m`, `tools/ios-demos/swift-demo.swift`)

ObjC:

```
-[CryptoManager decryptWithKey:]
_objc_msgSend$decryptWithKey:
```

메서드 이름이 그대로 심볼에 있고 objc_msgSend로 동적 디스패치를 한다.
그래서 Frida가 이름만으로 잡는다: `ObjC.classes.CryptoManager['- decryptWithKey:']`.

Swift:

```
_$s4main13CryptoManagerC7decrypt7withKeyS2S_tF
```

이름이 뭉개져서 뭔지 모른다. swift-demangle로 풀면:

```
main.CryptoManager.decrypt(withKey: Swift.String) -> Swift.String
```

Swift는 정적 디스패치라 objc_msgSend를 안 거친다. 그래서 이름 기반 후킹이 안 되고
심볼 주소를 찾아 Interceptor.attach로 붙여야 한다. 요즘 Swift 앱 후킹이 더 번거로운 이유.

## 왜 Android와 iOS가 이렇게 다른가 (컴파일 차이)

만들어지는 결과물 자체가 다르다.

- Android: Kotlin/Java -> JVM 바이트코드 -> DEX 바이트코드. CPU 독립적인 중간표현이 APK에 실린다.
  메타데이터(이름, 타입)가 풍부해서 jadx가 소스 가깝게 복원한다. 실행은 기기에서 ART가 컴파일.
- iOS: Swift/ObjC -> LLVM -> 네이티브 ARM64 기계어로 바로 컴파일해서 Mach-O로 배포.
  바이트코드가 안 실린다(Bitcode는 Xcode 14에서 폐기). 메타데이터가 적어 jadx 같은 게 불가능.
  단 ObjC 런타임 이름은 남아서 class-dump과 이름 기반 후킹이 된다.

이유: Android는 여러 제조사/CPU 이식성 때문에 VM+바이트코드, iOS는 단일 하드웨어라
성능·크기·통제 위해 네이티브 AOT. 그래서 iOS 리버싱이 한 단계 더 손이 간다.

## 다음

- 오픈소스 취약 앱(iGoat-Swift, DVIA-v2)을 시뮬레이터용으로 빌드해서 진짜 iOS 앱 분석 흐름 타보기.
- Swift로 짠 함수를 주소 기반으로 실제 후킹해보기.
