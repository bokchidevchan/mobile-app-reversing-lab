// 캡스톤 난독화 crackme (iOS/macOS 네이티브 Mach-O)
// 층: 문자열 XOR 암호화(volatile 키로 상수폴딩 방지) + PT_DENY_ATTACH 안티디버깅
//     + 수동 비교 + 심볼 스트립(빌드 옵션)
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include "enc.h"

#ifndef PT_DENY_ATTACH
#define PT_DENY_ATTACH 31
#endif

static int check(const char *in) {
    // 안티디버깅: 디버거가 붙는 것을 커널 차원에서 거부
    ptrace(PT_DENY_ATTACH, 0, 0, 0);

    volatile unsigned char k = SECRET_KEY;   // volatile: XOR 상수폴딩 방지
    unsigned char dec[SECRET_LEN];
    for (int i = 0; i < SECRET_LEN; i++) dec[i] = SECRET_ENC[i] ^ k;

    int diff = 0;
    if ((int)strlen(in) != SECRET_LEN) return 0;
    for (int i = 0; i < SECRET_LEN; i++) diff |= (unsigned char)in[i] ^ dec[i];  // 수동 비교
    memset(dec, 0, SECRET_LEN);
    return diff == 0;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <secret>\n", argv[0]); return 1; }
    printf(check(argv[1]) ? "correct\n" : "nope\n");
    return 0;
}
