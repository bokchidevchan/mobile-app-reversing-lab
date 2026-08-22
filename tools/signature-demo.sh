#!/usr/bin/env bash
# 공개키 서명 검증 원리 시연 (Ed25519, openssl). 키는 mktemp에만 생성, 커밋 안 함.
set -e
D="$(mktemp -d)"; cd "$D"
openssl genpkey -algorithm ed25519 -out priv.key                  # 개인키(비공개)
openssl pkey -in priv.key -pubout -out pub.key                    # 공개키(앱에 배포)
printf 'user=chan;plan=pro;expires=2027-01-01' > msg.txt
openssl pkeyutl -sign -inkey priv.key -rawin -in msg.txt -out msg.sig
echo "서명 크기: $(stat -f%z msg.sig 2>/dev/null || stat -c%s msg.sig) 바이트"
echo "1) 원본 검증:"; openssl pkeyutl -verify -pubin -inkey pub.key -rawin -in msg.txt -sigfile msg.sig
printf 'user=chan;plan=pro;expires=2028-01-01' > msg2.txt
echo "2) 메시지 변경 검증:"; openssl pkeyutl -verify -pubin -inkey pub.key -rawin -in msg2.txt -sigfile msg.sig || true
python3 -c "d=bytearray(open('msg.sig','rb').read()); d[0]^=1; open('bad.sig','wb').write(d)"
echo "3) 서명 훼손 검증:"; openssl pkeyutl -verify -pubin -inkey pub.key -rawin -in msg.txt -sigfile bad.sig || true
echo "4) 공개키로 서명 시도:"; openssl pkeyutl -sign -inkey pub.key -rawin -in msg.txt -out x.sig || true
