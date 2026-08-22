# 캡스톤: 난독화 iOS(Mach-O) 앱 빌드하고 크랙

Android 캡스톤에 이어 iOS 쪽. Mach-O 네이티브 크랙미를 직접 만들고 다시 뚫었다.
Android에서 배운 것(-O2 상수폴딩)을 이번엔 처음부터 반영했다.

소스는 tools/capstone/ios/. 빌드 산출물은 커밋 안 함.

## 쌓은 방어 층

1. 문자열 XOR 암호화 + **volatile 키** (Android에서 당한 상수폴딩을 처음부터 방지)
2. `ptrace(PT_DENY_ATTACH)` 안티디버깅 (iOS/macOS 커널이 디버거 attach 거부)
3. 수동 상수시간 비교 (strcmp 안 씀)
4. 심볼 스트립 (`strip -x`)

## 빌드

```
clang -O2 -arch arm64 -o ios-capstone verify.c
strip -x ios-capstone
```

정답이 strings에 안 뜨는 것 확인. 정상 동작(correct/nope)도 확인.
Android 때와 달리 volatile 키를 처음부터 써서 평문 누수가 없었다.

## 크랙

1. strings: 정답 안 보임.
2. Ghidra로 check 함수 디컴파일:
   - `ptrace(0x1f, ...)` -> 0x1f=31=PT_DENY_ATTACH 안티디버깅
   - `dec[i] = 암호문[0x100003f8f + i] ^ 0x3d` (25바이트) -> 정답 = 암호문 XOR 0x3d
3. Mach-O __TEXT 베이스 0x100000000 -> vaddr 0x100003f8f는 파일오프셋 0x3f8f.
   거기서 25바이트 읽어 XOR 0x3d.

복원 정답: `ios_capstone_cracked_2026`

## Android 캡스톤과 비교

| | Android 캡스톤 | iOS 캡스톤 |
|---|---|---|
| 형태 | .so (JNI) | Mach-O 실행 |
| 안티디버깅 | ptrace(PTRACE_TRACEME) | ptrace(PT_DENY_ATTACH) |
| 문자열 누수 | 뒤 6바이트 평문 폴딩 누수 | 없음(처음부터 volatile) |
| 크랙 | Ghidra + 암호문 XOR + 즉시값 조합 | Ghidra + 암호문 XOR |

## 공통 교훈

- 안티디버깅(ptrace)은 정적 분석엔 무의미하다. 실행을 안 하면 게이트를 안 마주친다.
  Ghidra로 읽으면 ptrace 호출 자체가 그냥 보인다.
- XOR 문자열 암호화는 컴파일러 최적화에 취약하다. volatile 등으로 폴딩을 막아도,
  결국 복호화 로직과 암호문이 바이너리에 다 있으니 정적으로 역산된다.
- 내가 쌓은 층들은 "정적 읽기"를 귀찮게 할 뿐, 막지는 못했다. 진짜 막으려면
  컨트롤 플로우 난독화(OLLVM), 패킹, 화이트박스 크립토, 런타임 무결성까지 필요하다.
- 근본은 이 랩 내내 나온 결론과 같다. 클라이언트에 넣은 비밀은 결국 털린다.

## 다음 (더 세게)

- OLLVM으로 컨트롤 플로우 난독화 붙여서 Ghidra 디컴파일이 얼마나 망가지는지.
- 실제 iOS 앱(.app/IPA)로 만들어 시뮬레이터 + Frida 동적 크랙까지.
