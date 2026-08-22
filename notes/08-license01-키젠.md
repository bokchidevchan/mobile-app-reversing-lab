# License_01 분석 기록 (keygen)

앞선 crackme들은 "숨긴 문자열 찾기"였는데, 이건 결이 다르다.
시리얼 검증 알고리즘을 리버싱해서 통과하는 시리얼을 직접 만들어야 하는 keygen-me.

대상 바이너리(validate)는 커밋 안 함.

## 대상

- `validate`: 32비트 ARM ELF (Android용 커맨드라인 바이너리), stripped.
- 사용법: `./validate <serial>`
- strings에 base32 알파벳 `ABCDEFGHIJKLMNOPQRSTUVWXYZ234567`이 보임 -> 시리얼이 base32.

## 흐름 (Ghidra 디컴파일)

main:

- argc==2, 시리얼 길이 == 0x10 (16자) 요구
- "Entering check_license" 출력
- base32 디코드(FUN_00011340): 16자 base32 -> 10바이트
- 그 10바이트를 검증 함수(FUN_00011760)에 넘김

검증 함수 FUN_00011760:

```c
for (i=0; i<5; i++)
    local_20[i] = b[2*i] ^ b[2*i+1];      // 10바이트를 2개씩 XOR -> 5바이트
if (local_20[0]==0x4c && local_20[1]==0x4f && local_20[2]==0x4c &&
    local_20[3]==0x5a && local_20[4]==0x21)
    puts("Product activation passed. Congratulations!");
```

목표 5바이트 0x4c 0x4f 0x4c 0x5a 0x21 = "LOLZ!" (작은 함수 4개가 리턴하는 상수였음).

## keygen

조건: 10바이트 b에서 b[0]^b[1]=0x4c, b[2]^b[3]=0x4f, b[4]^b[5]=0x4c, b[6]^b[7]=0x5a, b[8]^b[9]=0x21.
해가 무수히 많다. 제일 단순하게 짝수바이트=목표, 홀수바이트=0 (x^0=x).

```
10바이트: 4c 00 4f 00 4c 00 5a 00 21 00
base32 인코딩(10바이트 -> 16자) -> 유효 시리얼
```

생성된 유효 시리얼: `JQAE6ACMABNAAIIA`

검산: 이 시리얼을 base32 디코드 -> 10바이트 -> 2개씩 XOR = 0x4c 0x4f 0x4c 0x5a 0x21 = "LOLZ!" 일치.
(macOS엔 user-mode qemu-arm이 없어서 실행 검증은 못 함. 알고리즘+base32 왕복으로 구성상 유효.
안드로이드 에뮬에 validate 올려 ./validate 로 실행 확인 가능.)

## 앞 crackme들과 뭐가 다른가

- L1~L3: 앱에 있는 정답을 "꺼내기" (복호화/조립).
- License_01: 정답이 애초에 없고, 검증식을 만족하는 입력을 "만들기" (keygen).
  검증 로직을 이해해야 역으로 통과값을 생성할 수 있다.

이게 라이선스/시리얼 크랙의 기본 형태다. 검증 알고리즘만 이해하면 무한히 유효키를 찍어낼 수 있다는 게
"클라이언트 검증은 뚫린다"의 또 다른 사례.

## 다음

- 에뮬에서 실제로 ./validate JQAE6ACMABNAAIIA 실행해 "Congratulations" 확인.
