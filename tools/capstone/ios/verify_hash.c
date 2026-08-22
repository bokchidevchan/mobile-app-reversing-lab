// 캡스톤 은닉의 상한: 정답을 복호화하지 않는다.
// SHA-256(입력) == 저장된 해시  로만 검사.
// -> 정답 평문이 코드에도, 실행 중 메모리에도 절대 안 뜬다.
//    Frida로 비교를 후킹해도 "해시"만 보이지 정답은 못 얻는다.
//    남는 공격은 무차별 대입(brute-force)뿐 -> 정답이 강하면 사실상 못 뚫음.
#include <stdio.h>
#include <string.h>
#include <CommonCrypto/CommonDigest.h>
#include "target_hash.h"

static int check(const char *in) {
    unsigned char h[32];
    CC_SHA256(in, (CC_LONG)strlen(in), h);   // 입력을 해시
    return memcmp(h, TARGET, 32) == 0;        // 해시끼리 비교 (정답 평문 없음)
}

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage: %s <secret>\n", argv[0]); return 1; }
    printf(check(argv[1]) ? "correct\n" : "nope\n");
    return 0;
}
