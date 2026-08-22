# UnCrackable L3 분석 기록 (네이티브 + 변조 탐지)

L2가 검증을 네이티브로 내렸다면, L3는 거기에 변조 탐지(무결성 검사)와 런타임 초기화를 얹었다.
정답 재료가 정적 상수가 아니라 실행 중 세팅되는 게 새로운 점.

대상 apk, libfoo.so, 디컴파일 산출물은 커밋 안 함(gitignore).

## 새로 생긴 방어

jadx로 보니 클래스가 늘었다: RootDetection, IntegrityCheck.

- **변조 탐지(verifyLibs)**: 각 아키텍처 libfoo.so와 classes.dex의 CRC를 계산해서
  리소스에 저장된 값(그리고 네이티브 baz())과 비교. 하나라도 다르면 `tampered=31337`.
  리패키징(.so나 dex를 수정)을 잡으려는 것.
- onCreate에서 루트/디버그/tampered를 다 검사해서 걸리면 "Rooting or tampering detected." 후 종료.
- xor 키가 Java에 그대로 보인다: `xorkey = "pizzapizzapizzapizzapizz"` (24바이트).
- `init(xorkey.getBytes())`로 키를 네이티브에 넘김. 검증은 여전히 native `bar()`.

## 네이티브 bar 디컴파일 (Ghidra)

```c
if (DAT_00115054 == 2) {                     // init이 세팅하는 플래그
    FUN_001010e0(local_68);                  // local_68 <- xorkey (init이 저장한 것)
    input = GetStringUTFChars(...);
    len   = GetStringUTFLength(...);
    if (len == 0x18) {                        // 입력 24자
        for (i=0; i<0x18; i++)
            if (input[i] != (DAT_00115038[i] ^ local_68[i])) fail;
        return 1;                             // 전부 일치하면 통과
    }
}
```

즉 `정답[i] = 내장암호문[i] XOR xorkey[i]`, 길이 24.

## 정답 뽑기

DAT_00115038(암호문)이 .bss에 있었다. .bss는 런타임에 채워지는 영역이라 정적으론 0.
즉 암호문이 실행 중에 어딘가(.rodata)에서 복사돼 온다는 뜻.

그래서 역방향으로 확인했다. 예상 정답을 xorkey로 XOR한 암호문이 .so에 실제 박혀 있나 찾으니,
파일 오프셋 0x34b0에 있었다. 런타임에 이게 .bss로 복사되는 구조.

정답: `making owasp great again` (24자)

(정적으로 확정. 동적으로 하려면 anti-debug/tamper 우회하고 bar에서 XOR 결과를 덤프하면 된다.)

## L1 -> L2 -> L3 흐름

| | L1 | L2 | L3 |
|---|---|---|---|
| 검증 위치 | Java | 네이티브 | 네이티브 |
| 정답 숨김 | AES 암호화 | 조각 분할 | XOR + 런타임 초기화 |
| 방어 추가 | 루트/디버그 | + ptrace | + CRC 변조탐지, 루트탐지 클래스 |
| 뚫은 방법 | openssl 복호화 | Ghidra 조립 | Ghidra 로직 + 암호문 역산 |

핵심: 방어가 "정적 분석 막기"에서 "변조/리패키징 막기"로 확장됐다.
그래도 정적 분석(안 고치고 읽기만)엔 CRC 무결성 검사가 안 걸린다. 읽기만 하니까.

## 다음

- 동적: 에뮬에서 anti-debug 우회하고 bar 후킹해서 XOR 타깃 덤프로 재확인.
- baz()가 dex CRC를 어떻게 계산해 저장값과 맞추는지 따라가기.
