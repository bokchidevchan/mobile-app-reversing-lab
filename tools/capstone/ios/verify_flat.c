// 캡스톤 강화판: 컨트롤 플로우 평탄화(control-flow flattening) + 불투명 술어를 손으로 구현.
// 원래 순차 로직(복호 -> 길이검사 -> 비교)을 상태변수 + dispatch 루프로 흩어놓는다.
// Ghidra로 보면 명확한 흐름 대신 거대한 switch 디스패치가 되어 읽기 어려워진다.
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include "enc.h"

#ifndef PT_DENY_ATTACH
#define PT_DENY_ATTACH 31
#endif

// 항상 참/거짓인 불투명 술어 (리버서 눈속임)
static int opaque_true(long x)  { return ((x * x + x) & 1) == 0; }   // n^2+n 은 항상 짝수 -> 항상 참

static int check(const char *in) {
    ptrace(PT_DENY_ATTACH, 0, 0, 0);

    unsigned char dec[SECRET_LEN];
    volatile unsigned char k = SECRET_KEY;
    int i = 0, diff = 0, ok = 0;
    long guard = (long)strlen(in);

    // 평탄화: 모든 기본블록을 state로 번호 매겨 dispatch 루프에서 돈다.
    int state = 1;
    for (;;) {
        // 불투명 술어로 dispatch에 잡음 추가
        if (!opaque_true(state + i)) { state = 99; }
        switch (state) {
            case 1:  // 진입/길이 검사
                if (guard != SECRET_LEN) { state = 90; } else { i = 0; state = 2; }
                break;
            case 2:  // 복호 루프 조건
                state = (i < SECRET_LEN) ? 3 : 4;
                break;
            case 3:  // 복호 한 바이트
                dec[i] = SECRET_ENC[i] ^ k;
                i = i + 1;
                state = 2;
                break;
            case 4:  // 비교 루프 초기화
                i = 0; diff = 0; state = 5;
                break;
            case 5:  // 비교 루프 조건
                state = (i < SECRET_LEN) ? 6 : 7;
                break;
            case 6:  // 비교 한 바이트
                diff |= (unsigned char)in[i] ^ dec[i];
                i = i + 1;
                state = 5;
                break;
            case 7:  // 결과 판정
                ok = (diff == 0);
                state = 91;
                break;
            case 90: ok = 0; state = 91; break;   // 길이 불일치
            case 91: goto done;                    // 종료
            case 99: state = 90; break;            // 불투명 분기(도달 안 함)
            default: state = 90; break;
        }
    }
done:
    memset(dec, 0, SECRET_LEN);
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <secret>\n", argv[0]); return 1; }
    printf(check(argv[1]) ? "correct\n" : "nope\n");
    return 0;
}
