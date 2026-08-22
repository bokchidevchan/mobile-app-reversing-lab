# 캡스톤: 난독화 Android 앱 직접 빌드하고 다시 크랙

지금까지 남의 crackme를 풀었으니, 이번엔 내가 최대한 난독화한 앱을 만들고 그걸 다시 뚫었다.
방어를 직접 쌓아보면 각 층이 왜 뚫리는지 몸으로 안다.

소스는 tools/capstone/android/. 빌드 산출물(.so/.apk)은 커밋 안 함(gitignore).

## 쌓은 방어 층

1. 검증을 네이티브(libverify.so)로 내림 (jadx로는 로직 안 보임)
2. 정답을 XOR 암호화해서 저장 (strings에 안 뜨게)
3. ptrace(PTRACE_TRACEME) 안티디버깅 게이트 (디버거/Frida 붙으면 통과 못 함)
4. strcmp 대신 수동 상수시간 비교 (strcmp 후킹 회피)
5. 심볼 스트립 (JNI export만 남김)
6. 불투명 술어로 흐름에 잡음

## 빌드 (Gradle 없이 수동)

NDK clang으로 .so 빌드 -> javac/d8로 dex -> aapt2 link -> dex/so 삽입 -> zipalign -> apksigner.
스크립트는 build.sh. APK는 v2/v3 서명까지 붙여서 제대로 만들었다.

## 빌드하다 걸린 함정 (중요)

**-O2가 XOR을 상수 폴딩해서 평문을 박아버렸다.**
처음엔 `dec[i] = SECRET_ENC[i] ^ SECRET_KEY`로 짰는데, 둘 다 컴파일 상수라 컴파일러가
XOR을 컴파일 타임에 계산해서 복호화된 평문("capstone_break_m...")을 바이너리에 그대로 넣었다.
strings로 정답이 바로 보였다. 순진한 컴파일타임 XOR은 옵티마이저한테 무력하다.

고친 방법: 키를 `volatile`로 읽게 해서 컴파일러가 XOR을 못 접게 함. 그러니 평문이 사라지고
암호문만 .rodata에 남았다.

## 크랙

1. jadx: MainActivity가 native check()를 부름 -> 로직이 .so에 있어 여기서 막힘.
2. libverify.so strings: 정답 안 보임 (XOR 숨김 성공).
3. Ghidra로 check 함수 디컴파일:
   - 게이트 `DAT == 0x1337` (안티디버깅), 길이 0x16(22) 확인.
   - 비교가 `input[i] ^ 0x5c == 암호문[i]` 형태 -> 정답 = 암호문 XOR 0x5c.
4. .rodata의 암호문(offset 0x530) 16바이트를 XOR 0x5c -> "capstone_break_m".

여기서 또 하나 걸렸다. 컴파일러가 앞 16바이트는 NEON SIMD로 암호화된 채 비교했는데,
**뒤 6바이트는 평문 즉시값으로 폴딩**해서 코드에 그대로 노출했다.

```c
if (input[20] == '2') {
    bVar1 = *(int*)(input+16) == 0x30325f65 ...   // 0x30325f65 = "e_20"
}
if (input[21] == '6') { ... }
```

`0x30325f65`="e_20", `'2'`, `'6'` -> 뒤 6바이트 "e_2026"이 평문으로 누수됐다.

합치면 정답: `capstone_break_me_2026`

## 배운 것

- 내가 건 방어(안티디버깅)는 정적 분석엔 무의미했다. 실행을 안 하니 ptrace 게이트를 마주칠 일이 없다.
  Ghidra로 읽기만 하면 게이트 상수(0x1337)도 그냥 보인다.
- 문자열 XOR도 컴파일러가 폴딩하면 평문이 새거나(전체), 벡터화 경계에서 일부가 샌다(뒤 6바이트).
  즉 "소스에 XOR 썼다"고 안전한 게 아니라, 컴파일 결과를 직접 확인해야 한다.
- 진짜로 막으려면 컨트롤 플로우 난독화(OLLVM), 런타임 문자열 복호를 못 접게 하는 설계,
  패킹까지 필요하다. 지금 층들은 "정적 읽기"를 조금 귀찮게 할 뿐이었다.

## 다음

- OLLVM(컨트롤 플로우 난독화) 붙여서 Ghidra 디컴파일이 얼마나 망가지는지 보기.
- 동적 크랙: 에뮬에서 anti-debug 우회하고 Frida로 check 후킹해 게이트/정답 확인.
- iOS 캡스톤(Mach-O + PT_DENY_ATTACH + 문자열 암호화) 빌드하고 크랙.
