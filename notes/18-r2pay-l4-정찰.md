# UnCrackable L4 (r2pay) 1차 정찰

L1~L3와 급이 다른 다단계 CTF급 크랙미(r2con 유래). 한 번에 완전 공략은 무리라
이번엔 "무엇이 걸려 있고 어디를 파야 하나" 지도만 그렸다. 대상은 커밋 안 함.

## 정체

- 패키지 `re.pwnme`, targetSdk 29, debuggable=true.
- "r2coin" 가짜 결제 앱. PIN 4자리 + 금액을 넣고 pay를 누르면 `r2c-...` 토큰을 만든다.
- 프레임워크: 네이티브 Java/Kotlin (classes.dex 1개). 단 dex는 껍데기고 로직은 네이티브.

## 구조 (감사 도구 + 수동)

```
classes.dex            (얇음, UI/글루)
lib/*/libnative-lib.so   (arm64 1.7MB — 실제 로직/토큰화)
lib/*/libtool-checker.so (작음 — 도구 탐지 전용)
```

JNI 경계는 난독화된 이름 하나:

```java
public native byte[] gXftm3iswpkVgBNDUp(byte[] bArr, byte b);
System.loadLibrary("native-lib");
```

MainActivity는 PIN/금액을 받아 이 네이티브 함수에 넘기고, 반환 바이트로 `r2c-` 토큰을 표시한다.
즉 진짜 검증·토큰화 로직은 전부 libnative-lib.so 안에 있다.

## 걸려 있는 보호 (dex 문자열 스캔)

- 루트/su 탐지: 있음(12) — /system/xbin, superuser, magisk 등
- 루트 관리앱 패키지 탐지: chainfire, koushikdutta, topjohnwu(Magisk)
- Xposed 탐지: 있음
- 안티디버깅: 있음(ptrace/TracerPid류)
- Frida 문자열은 dex엔 없음 → libtool-checker.so(네이티브)에서 탐지할 가능성 큼
- 에뮬레이터 탐지: dex엔 신호 없음

libtool-checker.so라는 별도 네이티브 라이브러리가 있는 것 자체가 "탐지를 네이티브로 내렸다"는 신호.

## 전체 공략이라면 이렇게 (계획)

1. libtool-checker.so를 Ghidra로 열어 어떤 탐지를 하는지, 어떻게 결과를 앱에 알리는지 파악.
2. 에뮬레이터 + Frida로 탐지 함수들을 무력화(반환값 바꾸기)해서 앱이 죽지 않게.
3. libnative-lib.so(1.7MB, 난독화 가능성)에서 gXftm3iswpkVgBNDUp의 토큰화/검증 로직 분석.
   함수가 커서 정적만으론 힘들면 Frida로 입출력을 관찰하며 좁혀간다.
4. 숨겨진 플래그(다단계) 하나씩 추출.

## 이번 정찰의 결론

- L1~L3에서 익힌 걸 다 동원해야 하는 종합판이다: 네이티브 분석(Ghidra) + 안티탐지 우회(Frida) + 동적 관찰.
- 로직과 탐지를 전부 네이티브로 내리고 JNI 이름까지 난독화한 게 실제 상용 보호에 가장 가깝다.
- 전체 플래그 공략은 별도의 깊은 세션으로 남긴다. 이 노트는 그 출발 지도.

## 진전: libtool-checker.so 정체 규명 (Ghidra)

작은 쪽(libtool-checker.so, 10KB)을 실제로 뜯었더니 정체가 나왔다: **RootBeer**(com.scottyab.rootbeer),
유명 오픈소스 루트 탐지 라이브러리다. 심볼:

```
Java_com_scottyab_rootbeer_RootBeerNative_checkForRoot
Java_com_scottyab_rootbeer_RootBeerNative_setLogDebugMessages
_Z6existsPKc   // exists(const char*)
```

checkForRoot 디컴파일:

```c
bool checkForRoot(env, this, jarray paths){
  int cnt=0, n=GetArrayLength(paths);
  for(i=0;i<n;i++){
    const char* p = GetStringUTFChars(paths[i]);
    cnt += exists(p);          // su 경로가 존재하면 +1
  }
  return cnt > 0;              // 하나라도 있으면 루팅으로 판정
}
```

즉 탐지 경로 목록은 Java(RootBeer)에서 넘어오고, 네이티브는 그 경로들의 존재만 센다.
**우회 지점 확정**: `Java_com_scottyab_rootbeer_RootBeerNative_checkForRoot`를 Frida로 항상 false 반환,
또는 Java RootBeer의 isRooted 계열 메서드 후킹. (에뮬레이터엔 su가 앱 PATH에 없어 실제론 안 걸릴 수도.)

## 이번 도달점 / 남은 것

- 도달: 구조·보호 맵 + 탐지층(RootBeer) 규명 + 우회 지점 확정.
- 남음(별도 세션): libnative-lib.so(1.7MB)의 gXftm3iswpkVgBNDUp 토큰화/검증 로직 분석 →
  다단계 플래그 추출. 함수가 커서 Ghidra 정적 + Frida 입출력 관찰 병행 필요.
