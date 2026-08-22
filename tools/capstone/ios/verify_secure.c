// 실무적으로 "못 뚫는" 검증 (확인만 하면 되는 비밀 기준).
// 강한 정답 + salt + 느린 KDF(PBKDF2-HMAC-SHA256, 20만회) + 컨트롤 플로우 평탄화 + 안티디버깅.
//  - 정적: 정답 없음. salt/iters/저장 derived key만 보임.
//  - 동적: 후킹해도 derived key(해시)만 보이지 정답 평문은 안 뜸.
//  - brute-force: 고엔트로피 + KDF 20만회라 한 번 추측에 큰 비용 -> 사실상 불가.
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include "secure.h"

#ifndef PT_DENY_ATTACH
#define PT_DENY_ATTACH 31
#endif

static int opaque_true(long x) { return ((x * x + x) & 1) == 0; }  // 항상 참

static int check(const char *in) {
    ptrace(PT_DENY_ATTACH, 0, 0, 0);            // 안티디버깅

    unsigned char derived[DKLEN];
    int state = 1, r = 0;
    size_t inlen = strlen(in);
    for (;;) {                                    // 컨트롤 플로우 평탄화
        if (!opaque_true(state)) state = 90;
        switch (state) {
            case 1:  // KDF로 입력을 유도 (정답 원본은 등장하지 않음)
                CCKeyDerivationPBKDF(kCCPBKDF2, in, inlen,
                    SALT, sizeof(SALT), kCCPRFHmacAlgSHA256, ITERS,
                    derived, DKLEN);
                state = 2;
                break;
            case 2:  // 저장된 derived key와 비교
                r = (memcmp(derived, STORED, DKLEN) == 0);
                state = 3;
                break;
            case 90: r = 0; state = 3; break;
            case 3: goto done;
        }
    }
done:
    memset(derived, 0, DKLEN);
    return r;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <secret>\n", argv[0]); return 1; }
    printf(check(argv[1]) ? "correct\n" : "nope\n");
    return 0;
}
