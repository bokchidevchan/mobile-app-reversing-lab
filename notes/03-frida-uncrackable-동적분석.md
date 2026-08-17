# UnCrackable L1 동적 분석 (Frida) 기록

정적으론 이미 풀었지만, 같은 앱을 실행 중에 Frida로 후킹해서 정답을 메모리에서 가로채봤다.
"값은 아무리 숨겨도 실행하면 언젠가 평문으로 뜬다"를 직접 확인하는 게 목적.

## 환경 세팅

에뮬레이터가 있어야 앱을 돌린다. 호스트와 격리돼서 안전하다.

- Android SDK 커맨드라인 도구로 에뮬레이터/시스템 이미지 설치.
- `google_apis`(플레이스토어 없는) arm64 이미지로 AVD를 만들었다. `adb root`가 돼야 frida-server를 올린다.
  `google_apis_playstore`는 루팅이 안 된다.

여기서 삽질 좀 했다.

처음 만든 AVD가 커널 파일을 못 찾는다고 부팅 실패했다. brew로 깐 SDK랑 이미지 경로가 안 맞아서였다.
ANDROID_SDK_ROOT를 일관되게 맞춰서 다시 만드니 떴다.

그리고 adb에 기기가 여러 개 잡혔다. 내가 띄운 에뮬레이터 말고 **실제 기기 하나**(네트워크 adb)랑
기존에 돌던 다른 에뮬레이터까지. 실기기를 실수로 건드리면 안 되니, 이후 모든 명령에 `-s emulator-5556`으로
대상을 못박았다. 이거 안 하면 엉뚱한 기기에 앱 깔거나 root 명령 날아간다.

## frida-server 올리기

호스트 frida 버전과 기기 frida-server 버전이 맞아야 붙는다.

처음에 17.x로 했다가 Python API로 후킹하니 `Java is not defined` 에러가 났다.
Frida 17부터 Java 브리지가 런타임에서 분리됐는데, frida CLI는 자동으로 얹어주지만 Python `create_script`는
안 얹어준다. 브리지 내장된 16.7.19로 호스트/서버 둘 다 내리니 바로 됐다.

```
adb -s emulator-5556 root
adb -s emulator-5556 push frida-server /data/local/tmp/
adb -s emulator-5556 shell "/data/local/tmp/frida-server &"
```

## 후킹으로 정답 가로채기

스크립트는 `tools/uncrackable1-hook.js`. 하는 일:

- 루트 탐지(c.a/b/c)랑 디버그 탐지(b.a) 반환값을 false로 바꿔치기
- System.exit 무력화(이중 안전장치)
- AES 복호화 함수(sg.vantagepoint.a.a.a)를 후킹해서 복호화 결과를 출력
- 검증 함수를 직접 호출해서 복호화를 유발

결과:

```
[+] 런타임에 복호화된 정답: I want to believe
```

키도 안 계산하고, 돌아가는 앱이 스스로 푼 값을 그대로 가져왔다.
네이티브로 숨겼거나 키가 서버에서 왔어도, 복호화 직후 이 지점이면 똑같이 잡힌다는 게 핵심.

## 정정: 이 에뮬레이터에선 루트 탐지가 안 걸렸다

처음엔 "이 앱은 에뮬에서 root detected 뜨고 종료된다"고 생각했다. `/system/xbin/su`가 있었으니까.
그런데 진단해보니 원본 `c.a()`의 실제 반환값이 false였다.

앱의 c.a()는 `System.getenv("PATH")`를 쪼개서 각 디렉토리에 `su`가 있나 본다.
su가 /system/xbin에 있어도, 그 경로가 앱 PATH에 없으면 못 찾는다. 그래서 false.
c.b()는 Build.TAGS에 test-keys가 있나 보는데 이 에뮬은 dev-keys라 false. c.c()도 false.

즉 revlab에선 루트 탐지가 애초에 안 걸려서 우회가 필수는 아니었다.
우회 훅 자체는 정상 작동한다(원본 c.a 반환값 찍어보고 false로 바꾸는 것까지 확인).
탐지가 걸리는 환경(su가 PATH에 있거나 test-keys 빌드)이면 이 훅이 실제로 필요해진다.

배운 것: "루트 탐지가 있다"와 "이 환경에서 실제로 걸린다"는 다르다. 원본 반환값을 찍어보고 확인해야 한다.

## 다음

- 실제로 탐지가 걸리는 이미지에서 우회 전후 동작 차이 보기.
- SSL Pinning 있는 앱에 objection으로 `android sslpinning disable` 해보기.
