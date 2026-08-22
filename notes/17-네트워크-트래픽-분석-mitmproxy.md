# 네트워크 트래픽 분석 (mitmproxy + SSL Pinning)

앱 안의 값을 봤으니, 이제 앱이 밖으로 주고받는 값을 본다. 데이터 흐름/비인가 전송 확인의 기본.
대상은 임의 앱 말고 내 요청/에뮬레이터 자체 트래픽만 썼다.

## mitmproxy 원리

앱과 서버 사이에 프록시(mitmproxy)가 끼어 TLS를 대신 맺는다.

- 클라이언트 <-> mitmproxy: mitmproxy가 그 자리에서 만든 인증서로 TLS
- mitmproxy <-> 서버: 진짜 TLS
- 그래서 mitmproxy는 가운데서 평문을 본다.

이게 되려면 클라이언트가 mitmproxy의 CA를 신뢰해야 한다. 안 그러면 위조 인증서라 거부당한다.

## 시연: CA 신뢰 여부가 전부

호스트 curl로 mitmproxy(127.0.0.1:8080)를 통해 https 요청:

```
(1) CA 신뢰 안 함: curl: (60) SSL certificate problem  -> 막힘
(2) CA 신뢰함(--cacert): 통과. mitm이 복호화해서 기록:
    GET https://example.com/secretpath?token=ABC123
```

URL 안의 token까지 평문으로 드러난다. 즉 CA만 신뢰시키면 HTTPS 내용이 다 보인다.

에뮬레이터 자체 트래픽도 프록시로 잡혔다(연결성 체크가 평문 HTTP라 바로 보임):

```
GET http://connectivitycheck.gstatic.com/generate_204
GET http://play.googleapis.com/generate_204
```

## 에뮬레이터 세팅

```
emulator -avd revlab -http-proxy 127.0.0.1:8080   # 부팅 시 프록시 지정
mitmdump -w flows                                  # 호스트에서 프록시 실행
```

프록시는 잘 붙는다(위 HTTP 트래픽이 증거). 문제는 HTTPS 신뢰다.

## Android CA의 함정 (실제로 부딪힘)

- **user CA vs system CA**: 사용자가 설정에서 넣는 user CA는 Android 7+부터 앱이 기본으로
  안 믿는다(network_security_config 때문). 그래서 대부분 앱엔 system CA로 넣어야 mitm이 통한다.
- **Android 14는 system CA 심기가 막힌다**: revlab(android-34)에서 시도하니
  `/system`이 read-only(verity)라 remount 실패, `cp ... Read-only file system`.
  게다가 트러스트 앵커가 conscrypt APEX(/apex/com.android.conscrypt/cacerts)로 옮겨가서
  단순 복사로는 안 된다.
- 우회 방향: cacerts 디렉토리에 tmpfs를 bind-mount하고 기존 인증서 + mitm CA를 채워 넣는 것.
  단 APEX는 마운트 네임스페이스가 분리돼 있어, 앱이 보는 네임스페이스에 반영하려면
  zygote 네임스페이스로 들어가 마운트해야 한다(nsenter). 여기는 별도 심화로 남김.

즉 프록시 붙이기는 쉽고, "Android 14에서 HTTPS를 신뢰시키기"가 실전 관문이다.

## SSL Pinning

system CA로 신뢰시켜도 막히는 경우가 pinning이다. 앱이 "이 서버는 이 특정 인증서(또는 공개키)만
믿는다"를 코드에 박아둔 것. 그러면 mitm의 인증서가 시스템에서 신뢰돼도 앱이 거부한다.

우회는 런타임 후킹이다(정적으론 인증서 검사가 코드에 있어도 값 비교라 못 우회, 실행 중 바꿔야 함).

- objection: `android sslpinning disable` 한 줄
- Frida: TrustManager/OkHttp CertificatePinner/SSLContext를 후킹해서 검사를 무력화

이 랩에서 Frida는 이미 동작 확인했으니(캡스톤), 실제 pinned 앱을 구해 붙이는 건 다음 단계.

## 검수 관점 (이 분석이 잡아내는 것)

- 앱이 어떤 엔드포인트로 무슨 데이터를 보내는지 (비인가 전송, 몰래 하는 추적)
- 민감정보가 URL/헤더/바디에 평문으로 나가는지
- 토큰/키가 트래픽에 노출되는지
- pinning이 걸려 있으면 그 자체가 "숨기려는 통신"의 신호일 수 있음

## 다음

- Android 14 system CA를 tmpfs+nsenter로 실제 심어서 앱 HTTPS까지 잡기.
- pinned 앱(직접 빌드 or 공개 샘플)에 objection/Frida로 pinning 우회 시연.
