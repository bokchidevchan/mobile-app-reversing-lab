# 다음 학습: 네트워크 트래픽 분석 (mitmproxy + SSL Pinning)

아직 안 함. 다음 세션 시작점으로 적어둔 계획 노트다.

## 왜 이 순서

지금까지 앱 안의 값(정답, 키)을 정적/동적으로 뽑았다. 다음은 앱이 밖으로 주고받는 값이다.
API 요청, 토큰, 응답을 가로채는 것. Frida로 함수를 후킹한 것과 같은 발상인데,
이번엔 함수가 아니라 네트워크 경계에서 가로챈다.

## 목표

1. 에뮬레이터의 HTTPS 트래픽을 mitmproxy로 평문으로 본다.
2. 앱이 인증서 검사(SSL Pinning)로 프록시를 거부하면, 그걸 우회한다.

## 개념 정리할 것

- HTTPS 중간자(MITM)가 왜 되는가: 프록시가 자기 인증서로 TLS를 대신 맺고, 기기가 그 인증서를
  신뢰하게 만들면 평문이 보인다.
- 그냥 시스템 CA에 프록시 인증서 넣는 것과, 앱이 자체적으로 특정 인증서만 믿게 박아둔
  SSL Pinning의 차이. 후자는 시스템 CA를 신뢰해도 안 뚫린다.
- Android 7+부터 앱이 사용자 CA를 기본으로 안 믿는다는 것(network_security_config).

## 실습 계획 (에뮬레이터, 안전하게)

- mitmproxy 설치, 프록시 띄우기.
- 에뮬레이터 프록시 설정 + mitmproxy CA 인증서를 시스템 CA로 설치(rooted 에뮬이라 가능).
- 평범한 HTTPS 앱으로 요청/응답 평문 확인.
- Pinning 있는 앱에서 막히는 것 재현 -> objection `android sslpinning disable` 또는
  Frida 스크립트로 우회 -> 다시 평문 확인.
- 대상은 이번에도 임의 앱 말고 직접 만든 테스트 앱이나 공개 취약 앱(OWASP 계열)만.

## 준비물

- mitmproxy (brew install mitmproxy)
- 이미 있는 것: 에뮬레이터(revlab), frida, objection
- 실기기는 안 건드림. 대상은 -s로 에뮬레이터만 지정.

## 확인하고 싶은 것

- Pinning 우회 전후로 mitmproxy 로그가 어떻게 달라지는지.
- 우회가 시스템 CA 설치로 되는 앱과, Frida 후킹까지 필요한 앱의 차이.
