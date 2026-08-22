# UnCrackable L2 네이티브 분석 기록

L1은 jadx만으로 풀렸다. L2는 정답 검증이 네이티브 라이브러리로 넘어가서 한 단계 더 들어가야 했다.
목표는 같다. 앱 실행 없이(정적으로) 정답 뽑기. 이번엔 .so를 Ghidra로.

분석 대상(apk, libfoo.so)과 디컴파일 산출물은 커밋 안 함. gitignore로 막았다.

## 크기부터 다르다

L2 apk가 901KB. L1이 66KB였다. 차이는 lib/에 있었다.

```
classes.dex
lib/arm64-v8a/libfoo.so
lib/armeabi-v7a/libfoo.so
lib/x86/libfoo.so
lib/x86_64/libfoo.so
```

아키텍처별 네이티브 라이브러리가 4개. 로직 일부가 C로 컴파일돼 여기 들어간 것.

## jadx가 막히는 지점

jadx로 뜯으니 MainActivity랑 CodeCheck가 나왔다. 그런데 검증 부분이 이렇다.

```java
// CodeCheck.java
public boolean a(String str) {
    return bar(str.getBytes());     // bar가 native
}
private native boolean bar(byte[] bArr);
```

`bar`가 native다. 실제 비교 로직이 libfoo.so 안에 있어서 jadx로는 여기서 끝. Java만 봐선 정답이 안 나온다.
MainActivity엔 `System.loadLibrary("foo")`랑 `native void init()`도 있었다.

## .so 훑기

arm64 libfoo.so를 꺼내서 봤다.

- JNI 심볼: `Java_sg_vantagepoint_uncrackable2_CodeCheck_bar`, `..._MainActivity_init`
- 비교 함수: `strncmp`
- 안티디버깅 흔적: `ptrace`, `getppid`, `waitpid`, `pthread_create`
- 수상한 문자열: `"Thanks for all t"` (16바이트에서 잘림), `5063045`

문자열이 "Thanks for all t"에서 끊긴 게 힌트였다. 뒤에 non-printable 바이트가 붙어서 strings가 거기서 멈춘 것.
정답이 통째로 안 박혀 있고 조각나 있다는 뜻.

## Ghidra로 bar 디컴파일

Ghidra 헤드리스로 bar 함수를 의사 C로 뽑았다. (`tools/DecompFunc.java`, JDK 21 필요)

```c
if (DAT_0011300c == '\x01') {                 // init의 안티디버깅이 세팅하는 플래그
    uStack_48 = _UNK_00100ea8;
    local_50   = _DAT_00100ea0;               // "Thanks for all t" (16바이트)
    local_40   = 0x68736966206568;            // 리틀엔디언 -> "he fish"
    __s1 = GetStringUTFChars(input);          // param+0x5c0
    iVar2 = GetStringUTFLength(input);         // param+0x558
    if (iVar2 == 0x17 && strncmp(__s1, &local_50, 0x17) == 0)   // 길이 23, strncmp
        return 1;
}
```

정답을 두 조각으로 나눠 숨겼다.

- 데이터 영역 `_DAT_00100ea0` = `"Thanks for all t"` (16바이트)
- 코드 인라인 리터럴 `0x68736966206568` = `"he fish"` (리틀엔디언 8바이트)

스택에 이어 붙여서 23바이트를 만들고, 입력 길이가 0x17(23)이면서 strncmp가 일치하면 통과.

합치면 정답: `Thanks for all the fish` (23자)

`DAT_0011300c == 1` 게이트도 있다. init()의 ptrace 안티디버깅이 이 플래그를 세팅하는 걸로 보인다.
디버거가 붙으면 이 경로가 막힐 수 있다. 정적 분석엔 상관없지만 동적으로 갈 땐 이 안티디버깅을 넘겨야 한다.

## L1 -> L2 정리

| | L1 | L2 |
|---|---|---|
| 검증 위치 | Java (DEX) | 네이티브 libfoo.so |
| 도구 | jadx | jadx + Ghidra |
| 정답 숨김 | AES 암호화 | 조각 분할 + 네이티브 매설 |
| 방어 | 루트/디버그 탐지(Java) | + ptrace 안티디버깅(네이티브) |

핵심: 민감한 로직을 네이티브로 내리면 jadx가 안 통해서 디스어셈블/디컴파일러가 필요해진다.
실제 상용 앱이 이렇게 한다. 네이티브 분석이 되냐 안 되냐가 실전 리버싱의 갈림길.

## 다음

- 동적으로도 풀어보기: 에뮬에서 anti-debug(ptrace) 우회하고 Frida로 strncmp 후킹해서 정답 확인.
- init()이 DAT_0011300c 플래그를 어떻게 세팅하는지 Ghidra로 따라가기.
