#!/usr/bin/env python3
# 로컬 라이선스 활성화 서버 (stdlib만, 127.0.0.1 전용).
# 핵심: 검증 판정과 비밀(HMAC 키)을 서버가 쥔다. 클라는 요청만 한다.
#  - /activate : 라이선스를 기기ID에 묶어 등록. seat 한도로 재사용(복사) 차단. 토큰 발급.
#  - /verify   : 발급한 토큰을 검증(서버가 상태로 판정).
import http.server, json, hmac, hashlib, os, time, threading

SECRET = os.urandom(32)          # 서버만 아는 비밀 (실행 중에만 존재, 커밋 안 함)
SEAT_LIMIT = 2                   # 라이선스당 허용 기기 수
VALID_LICENSES = {"LIC-AAAA-BBBB", "LIC-CCCC-DDDD"}   # 벤더가 발급한 유효 키(데모)
activations = {}                 # {license: set(device_id)}  <- 서버가 쥔 상태
lock = threading.Lock()

def token(license, device):
    exp = int(time.time()) + 3600
    msg = f"{license}|{device}|{exp}".encode()
    sig = hmac.new(SECRET, msg, hashlib.sha256).hexdigest()
    return f"{exp}.{sig}"

def token_ok(license, device, tok):
    try:
        exp, sig = tok.split(".", 1)
        if int(exp) < time.time(): return False
        msg = f"{license}|{device}|{exp}".encode()
        good = hmac.new(SECRET, msg, hashlib.sha256).hexdigest()
        return hmac.compare_digest(sig, good)
    except Exception:
        return False

class H(http.server.BaseHTTPRequestHandler):
    def _send(self, code, obj):
        b = json.dumps(obj).encode()
        self.send_response(code); self.send_header("Content-Type","application/json")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def log_message(self, *a): pass
    def do_POST(self):
        n = int(self.headers.get("Content-Length", 0))
        try: req = json.loads(self.rfile.read(n) or b"{}")
        except Exception: return self._send(400, {"error":"bad json"})
        lic = req.get("license"); dev = req.get("device_id")
        if self.path == "/activate":
            if lic not in VALID_LICENSES:
                return self._send(403, {"ok":False, "reason":"invalid license"})
            with lock:
                seats = activations.setdefault(lic, set())
                if dev not in seats and len(seats) >= SEAT_LIMIT:
                    return self._send(409, {"ok":False, "reason":f"seat limit {SEAT_LIMIT} exceeded"})
                seats.add(dev)
                return self._send(200, {"ok":True, "token":token(lic,dev), "seats_used":len(seats)})
        if self.path == "/verify":
            ok = token_ok(lic, dev, req.get("token",""))
            return self._send(200, {"ok":ok})
        self._send(404, {"error":"no route"})

if __name__ == "__main__":
    srv = http.server.HTTPServer(("127.0.0.1", 8799), H)   # localhost 전용
    print("activation server on 127.0.0.1:8799"); srv.serve_forever()
