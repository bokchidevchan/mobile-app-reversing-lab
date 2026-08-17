# 02. Android 리버싱 도구: jadx, apktool, Frida, objection

도구를 정적/동적 두 축으로 나눠서 이해하는 게 먼저다.

- **정적 분석**은 앱을 실행하지 않고 바이너리를 뜯어 읽는다. jadx, apktool이 여기 속한다. 실행하지 않으니 상대적으로 안전하다.
- **동적 분석**은 앱을 실제로 돌리면서 런타임을 관찰하고 조작한다. Frida, objection이 여기 속한다. 실행이 필요하니 격리된 에뮬레이터에서 한다.

## jadx (정적)

DEX를 Java 비슷한 소스로 역컴파일해서 보여준다. 리버싱의 첫 삽은 대부분 여기서 뜬다.

```
jadx-gui app.apk       # GUI로 클래스 트리 탐색
jadx -d out/ app.apk   # 소스를 파일로 추출
```

원본 변수명은 대부분 날아가고 `a`, `b` 같은 이름으로 나온다(난독화 안 한 앱도 컴파일 과정에서 로컬 변수명이 사라진다). 그래도 문자열 상수, 메서드 호출 흐름, 하드코딩된 값은 그대로 드러난다. 분석의 90%는 "어디에 뭐가 있나"를 읽는 일이라 이걸로 충분할 때가 많다.

## apktool (정적)

jadx가 "읽기 좋은 Java"를 뽑는다면 apktool은 "구조를 정확히" 뽑는다.

```
apktool d app.apk -o decompiled/   # 디코드
apktool b decompiled/ -o new.apk   # 다시 빌드
```

두 가지가 핵심이다.

바이너리 XML로 저장된 `AndroidManifest.xml`을 사람이 읽는 XML로 디코드해준다. 권한 목록, 액티비티/서비스, `exported`, `debuggable`, `allowBackup` 같은 설정을 여기서 본다.

그리고 DEX를 **smali**(DEX 바이트코드의 어셈블리 표현)로 풀어준다. jadx의 Java가 실패하거나 부정확한 지점을 smali에서 정확히 확인할 수 있고, smali를 고쳐서 `apktool b`로 다시 빌드하면 앱 동작을 바꿔 재서명해 설치하는 리패키징이 된다.

jadx는 이해용, apktool은 정밀 확인과 수정용으로 나눠 쓰면 된다.

## Frida (동적)

실행 중인 프로세스에 JavaScript를 주입해서 함수를 가로채는 도구(인스트루멘테이션 프레임워크). 앱을 실행해야 하므로 에뮬레이터나 기기에 frida-server를 올리고 붙는다.

정적 분석으로 "이 함수가 루트를 탐지한다"까지 읽었으면, Frida로 그 함수를 런타임에 후킹해서 반환값을 항상 false로 바꿔치기하는 식이다. 암호화된 값도 복호화 직후 메모리에서 가로채면 키를 몰라도 평문을 얻는다.

```
frida-ps -U                          # 붙을 수 있는 프로세스 목록
frida -U -f 패키지명 -l hook.js       # 스크립트 주입하며 실행
```

## objection (동적)

Frida 위에 얹은 도구. 자주 쓰는 후킹을 명령어로 미리 만들어놨다. 스크립트를 직접 안 짜도 루트 우회, SSL 피닝 우회, 클래스 나열, 메서드 트레이싱 같은 걸 바로 실행한다.

```
objection -g 패키지명 explore
# android root disable
# android sslpinning disable
```

Frida는 세밀하게 짜는 손도구, objection은 흔한 작업을 빠르게 끝내는 전동공구쯤으로 보면 된다.

## 실습 환경 격리

정적 도구는 앱을 실행하지 않지만, 파서 자체에 취약점이 있으면 조작된 APK가 그걸 노릴 수 있다. 출처가 확실한 교육용 앱은 호스트에서 돌려도 되지만, 정체불명 샘플을 다룰 거라면 컨테이너 안에서 도는 습관을 들이는 게 안전하다. `tools/Dockerfile.static`에 jadx+apktool 컨테이너를 만들어뒀다. 분석 대상은 read-only로 마운트하고 `--network none`으로 네트워크를 끊는다.

동적 분석은 Docker로 격리가 안 된다. APK는 Android 런타임에서만 돌아서 컨테이너에선 실행 자체가 안 되고, 이때 샌드박스 역할은 에뮬레이터(AVD)가 한다.

## 도구별 역할 한 줄 정리

| 도구 | 축 | 하는 일 |
|------|-----|---------|
| jadx | 정적 | DEX → Java, 빠른 코드 읽기 |
| apktool | 정적 | 매니페스트 디코드, DEX → smali, 리패키징 |
| Frida | 동적 | 런타임 함수 후킹, 값 조작 |
| objection | 동적 | Frida 기반, 흔한 우회를 명령어로 |

## 남은 의문

- jadx의 Java 출력이 틀리는 경우 smali와 어떻게 대조하는지, 실제 사례로 확인해보기.
- Frida 후킹은 다음에 에뮬레이터 세우고 UnCrackable의 루트 탐지 함수부터 실습.
