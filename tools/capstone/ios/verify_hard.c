// 캡스톤 최종 강화: 정적 키 제거 + 런타임 키스트림 유도.
// 이제 .rodata엔 랜덤해 보이는 암호문만 있고, 키는 실행 중 LCG로 만들어진다.
// 정적으로 뽑으려면 이 유도 알고리즘을 읽어 재현해야 한다(비용 상승).
// 그리고 memcmp로 비교 -> 동적으로는 memcmp 후킹 한 방에 털린다.
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include "enc_hard.h"

#ifndef PT_DENY_ATTACH
#define PT_DENY_ATTACH 31
#endif

static int check(const char *in) {
    ptrace(PT_DENY_ATTACH, 0, 0, 0);          // 안티디버깅
    if ((int)strlen(in) != SECRET_LEN) return 0;

    unsigned char dec[SECRET_LEN];
    volatile unsigned int s = 0x3d;           // 키스트림 시드
    for (int i = 0; i < SECRET_LEN; i++) {
        s = (s * 31 + 17) & 0xff;             // LCG로 키 한 바이트 유도
        dec[i] = SECRET_ENC[i] ^ (unsigned char)s;
    }
    int r = (memcmp(in, dec, SECRET_LEN) == 0);   // 동적 후킹 표적
    memset(dec, 0, SECRET_LEN);
    return r;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <secret>\n", argv[0]); return 1; }
    printf(check(argv[1]) ? "correct\n" : "nope\n");
    return 0;
}
