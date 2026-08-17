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

## 다음

- 오픈소스 취약 앱(iGoat-Swift, DVIA-v2)을 시뮬레이터용으로 빌드해서 진짜 iOS 앱 분석 흐름 타보기.
- Ghidra로 check 함수를 의사 C까지 올려서 어셈블리 대신 읽어보기.
