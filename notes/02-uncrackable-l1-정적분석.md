# UnCrackable L1 정적 분석 기록

OWASP MASTG의 리버싱 연습용 앱 UnCrackable Level 1을 정적 분석만으로 풀어봤다.
목표는 앱을 한 번도 실행하지 않고 정답 문자열을 뽑아내는 것.

분석 대상은 저장소에 커밋하지 않는다. `sample-apps/`에 받아서 gitignore로 막아뒀고,
디컴파일 결과물(`out/`, `decompiled/`)도 전부 무시 대상이다. 남의 바이너리는 재배포 안 함.

## 준비하면서 겪은 것

처음엔 도구가 거의 다 안 깔려 있었다. jadx, apktool, frida 전부 없어서 brew로 설치.

```
brew install jadx apktool bundletool
```

분석 대상은 인터넷에서 아무 APK나 받으면 위험해서, 출처가 확실한 OWASP crackme를 썼다.
GitHub raw에서 받고 sha256이랑 gitignore 여부부터 확인.

```
sha256: 1da8bf57d266109f9a07c01bf7111a1975ce01f190b9d914bcd3ae3dbef96f21
```

중간에 결과 폴더 지우려고 `rm -rf`를 습관적으로 쳤는데, apktool `-f`랑 jadx가 어차피
덮어쓰니까 애초에 필요 없는 명령이었다. 위험한 명령은 습관으로도 안 치는 게 낫다.

## 1. 구조 확인 (unzip)

APK는 ZIP이라 그냥 풀린다. 01번에서 정리한 구조가 실제로 그대로 나왔다.

```
AndroidManifest.xml
classes.dex
resources.arsc
res/...
META-INF/  (CERT.RSA, CERT.SF, MANIFEST.MF)
```

`lib/`가 없다. 네이티브 코드 없는 순수 Java/Kotlin 앱이라는 뜻.
classes.dex 앞 8바이트를 보니 `64 65 78 0a 30 33 35 00`, 즉 `dex\n035\0`. DEX 버전 035 맞다.
META-INF만 있고 서명은 v1(JAR). keytool로 보니 SHA256withRSA로 서명돼 있었다.

## 2. 매니페스트 디코드 (apktool)

```
apktool d -f -o decompiled UnCrackable-Level1.apk
```

바이너리 XML이 읽을 수 있는 XML로 풀렸다. 그냥 unzip해서 열면 깨져 보이는데
apktool을 거치니 제대로 나온다. 여기가 01번에서 말한 "매니페스트는 바이너리 XML" 포인트.

- package: `owasp.mstg.uncrackable1`
- 메인 액티비티: `sg.vantagepoint.uncrackable1.MainActivity`
- `allowBackup="true"` 걸려 있음 (백업으로 앱 데이터 뽑을 수 있는 흔한 취약 설정)

## 3. 역컴파일 (jadx)

```
jadx -d out UnCrackable-Level1.apk
```

Java로 6개 클래스가 나왔다. 흐름을 따라가 보면,

`MainActivity.onCreate`에서 먼저 루트 탐지(`c.a/b/c`)랑 디버그 탐지(`b.a`)를 하고,
걸리면 다이얼로그 띄우고 `System.exit(0)`. 입력값 검증은 `verify()` → `a.a(문자열)`.

핵심은 `a.java`였다.

```java
bArrA = sg.vantagepoint.a.a.a(
    b("8d127684cbc37c17616d806cf50473cc"),
    Base64.decode("5UJiFctbmgbDoLXmpL12mkno8HT4Lv8dlat8FxR2GOc=", 0));
return str.equals(new String(bArrA));
```

정답을 코드에 그냥 박아놓은 게 아니라 AES로 암호화해서 숨겨놨다.
`b()`는 hex 문자열을 바이트로 바꾸는 함수라 위 hex가 곧 AES 키.
복호화 클래스(`sg/vantagepoint/a/a.java`)를 보니 `AES/ECB/PKCS7Padding`, 키 16바이트니까 AES-128.

## 4. 정적 복호화로 정답 뽑기

키와 암호문이 코드에 다 있으니 앱을 실행할 필요가 없다. openssl로 바로 복호화.

```
printf '5UJiFctbmgbDoLXmpL12mkno8HT4Lv8dlat8FxR2GOc=' | base64 -d > cipher.bin
openssl enc -d -aes-128-ecb -K 8d127684cbc37c17616d806cf50473cc -in cipher.bin
```

결과: `I want to believe`

루트 탐지도, 디버그 탐지도 안 건드리고 정답이 나왔다.
그 방어들은 앱을 실행할 때만 작동하는데, 나는 실행을 안 했으니 애초에 무의미했던 것.
정적 분석의 힘이 이 지점이다. 값이 코드 안에 있으면 아무리 암호화해도 키까지 같이 있어서 풀린다.

## 그 키는 APK 어디에 있었나

궁금해서 직접 찾아봤다. APK 안 파일을 하나씩 grep하니 `classes.dex`에서만 나왔다.
리소스나 별도 파일이 아니라 코드 안 상수. dex 오프셋 0xd52 근처 바이트를 보면:

```
20  38 64 31 32 ... 63 63  00
^   └──── "8d1276...cc" ───┘ ^
길이(0x20=32글자)            끝(null)
```

DEX는 문자열 상수를 전부 한 테이블(string pool)에 "길이 + 글자들 + 00" 형태로 모아둔다.
소스에 `"8d127684..."`라고 적은 게 컴파일되면 이 테이블의 한 항목이 된다.

찾는 방법은 두 가지다.

- 로직 따라가기: jadx에서 verify() → a.a()로 역방향으로 따라가 인자로 박힌 값을 봤다.
  의미를 알고 찾으니 이게 진짜 중요한 값인지 바로 판단된다.
- 문자열 훑기: 코드 안 읽고 dex의 모든 문자열을 뽑아 수상한 패턴(hex, base64, http, key)을
  눈으로 고른다. 1차 스캔으로 빠르다.

```bash
unzip -p UnCrackable-Level1.apk classes.dex | strings | grep -E '^[0-9a-f]{32}$'
```

보통 문자열 훑기로 후보를 빨리 잡고, 로직 따라가기로 그 값의 용도를 확인한다.

## 왜 뚫렸나 (교훈)

정답 문자열 자체("I want to believe")는 소스에 평문으로 박혀 있지 않았다.
개발자가 그걸 숨기려고 AES로 암호화해뒀다. 여기까진 나쁘지 않은 생각이다.

문제는 그걸 푸는 재료 두 개, 즉 키(`8d127684...`)랑 암호문(`5UJiFctb...`)이
소스에 문자열 상수로 그대로 하드코딩돼 있었다는 것.
앱이 실행 중에 스스로 복호화해서 비교하려면, 푸는 방법도 앱 안에 있어야 하니까
어쩔 수 없이 키가 같이 들어간다. 자물쇠를 채우고 그 옆에 열쇠를 붙여둔 꼴.

그래서 암호화가 방어를 하나도 못 했다. 상자(암호문)랑 열쇠(키)를 둘 다 코드에서
꺼내오니 밖에서 그냥 열린다. 난독화를 했어도 키 문자열은 결국 메모리 어딘가에 떠야 해서
정적으로 못 찾으면 동적으로 잡힌다.

핵심 결론: **클라이언트 앱에 넣은 비밀은 암호화를 하든 안 하든 결국 털린다.**
진짜 지켜야 하는 값(정답 검증, API 키)은 앱이 아니라 서버에 둬야 한다.
이 crackme가 가르치려는 게 이거다.

> 참고: 여기 나온 키/암호문/정답은 전부 OWASP가 공개 배포한 UnCrackable L1
> 샘플 앱에 원래 들어 있는 상수다. 내 개인 값이 아니고 OWASP 교육 자료에도 공개돼 있다.

## 5. 컨테이너로 옮기기

호스트에서 돌려도 이 앱은 안전하지만(실행 안 함, 출처 확실), 정체불명 샘플까지 생각하면
정적 도구를 컨테이너에 가두는 습관이 낫겠다 싶어서 Colima를 깔았다.

```
brew install colima docker
colima start --cpu 2 --memory 4 --disk 20
docker build -f tools/Dockerfile.static -t apk-static .
```

그리고 격리해서 다시 디컴파일. 비루트 사용자로 돌고, 네트워크는 끊고, APK는 읽기 전용.

```
docker run --rm --network none \
  -v "$PWD/sample-apps:/in:ro" -v "$PWD/out-container:/out" \
  apk-static jadx -d /out /in/UnCrackable-Level1.apk
```

호스트에서 한 것과 같은 6개 클래스가 나왔다. 결과물만 호스트로 받고 실행은 컨테이너 안에서 끝.

## 다음

- 루트 탐지(`c.a/b/c`)랑 디버그 탐지(`b.a`)는 정적으론 우회할 필요가 없었지만,
  실제로 앱을 돌리는 상황이라면 이걸 런타임에 넘겨야 한다. 다음에 에뮬레이터 세우고
  Frida로 저 함수들 후킹해서 반환값 바꾸는 걸 해본다.
- smali(`decompiled/smali/...`)랑 jadx의 Java를 대조해서, 같은 로직이 두 표현에서
  어떻게 보이는지 눈에 익히기.
