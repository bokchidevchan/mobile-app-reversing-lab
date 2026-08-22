#!/usr/bin/env bash
# 디지털 서명 기반 라이선스 검증 원리 시연 (Ed25519, openssl).
# 개인키=벤더(서버)만, 공개키=앱에 배포. 앱은 검증만, 발급은 못 한다.
set -e
D="$(mktemp -d)"; cd "$D"
openssl genpkey -algorithm ed25519 -out vendor_priv.key                 # 벤더 개인키(비공개)
openssl pkey -in vendor_priv.key -pubout -out vendor_pub.key            # 앱에 넣을 공개키
printf 'user=chan;plan=pro;expires=2027-01-01' > license.txt            # 라이선스 내용
openssl pkeyutl -sign -inkey vendor_priv.key -rawin -in license.txt -out license.sig
echo "[앱] 정상 검증:"; openssl pkeyutl -verify -pubin -inkey vendor_pub.key -rawin -in license.txt -sigfile license.sig
printf 'user=chan;plan=pro;expires=2099-01-01' > tampered.txt           # 변조 시도
echo "[공격] 변조 검증:"; openssl pkeyutl -verify -pubin -inkey vendor_pub.key -rawin -in tampered.txt -sigfile license.sig || true
openssl genpkey -algorithm ed25519 -out attacker.key                    # 위조 시도(자기 키)
openssl pkeyutl -sign -inkey attacker.key -rawin -in tampered.txt -out forged.sig
echo "[공격] 위조 검증:"; openssl pkeyutl -verify -pubin -inkey vendor_pub.key -rawin -in tampered.txt -sigfile forged.sig || true
echo "-> 변조/위조 둘 다 실패. 개인키 없이는 유효 라이선스를 못 만든다."
