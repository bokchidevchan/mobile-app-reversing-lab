// 캡스톤 난독화 crackme (Android 네이티브)
// 쌓은 층:
//  1) 검증을 네이티브(.so)로: jadx로는 로직이 안 보임
//  2) 문자열 XOR 암호화: strings에 정답이 안 뜸
//  3) ptrace 안티디버깅: 디버거/Frida 붙으면 게이트가 안 열림
//  4) 수동 바이트 비교: strcmp 후킹으로 못 잡게
//  5) 불투명 술어(opaque predicate): 흐름에 잡음
//  (심볼 스트립은 빌드 옵션에서)
#include <jni.h>
#include <string.h>
#include <sys/ptrace.h>
#include <unistd.h>
#include "enc.h"

static volatile int g_gate = 0;

// 라이브러리 로드 시 ptrace(TRACEME). 이미 디버거가 붙어 있으면 -1 -> 게이트 안 열림
__attribute__((constructor))
static void anti_debug(void) {
    long r = ptrace(PTRACE_TRACEME, 0, 0, 0);
    // 불투명 술어: (x*x - x) 는 항상 짝수 -> & 1 == 0. 리버서 눈속임용 잡음
    volatile long x = (long)getpid();
    long opaque = (x * x - x) & 1;   // 항상 0
    if (r != -1 && opaque == 0) {
        g_gate = 0x1337;
    }
}

// 수동 상수시간 비교 (strcmp 안 씀)
static int eq(const unsigned char *a, const unsigned char *b, int n) {
    int diff = 0;
    for (int i = 0; i < n; i++) diff |= (a[i] ^ b[i]);
    return diff == 0;
}

JNIEXPORT jboolean JNICALL
Java_com_example_capstone_MainActivity_check(JNIEnv *env, jobject thiz, jstring input) {
    if (g_gate != 0x1337) return JNI_FALSE;   // 안티디버깅 게이트

    const char *in = (*env)->GetStringUTFChars(env, input, 0);
    jsize len = (*env)->GetStringUTFLength(env, input);

    // 키를 volatile로 읽어야 컴파일러가 XOR을 상수폴딩(=평문 박제)하지 못한다.
    volatile unsigned char k = SECRET_KEY;
    unsigned char dec[SECRET_LEN];
    for (int i = 0; i < SECRET_LEN; i++) dec[i] = SECRET_ENC[i] ^ k;  // 런타임 복호

    jboolean ok = JNI_FALSE;
    if (len == SECRET_LEN && eq((const unsigned char *)in, dec, SECRET_LEN)) {
        ok = JNI_TRUE;
    }
    memset(dec, 0, SECRET_LEN);
    (*env)->ReleaseStringUTFChars(env, input, in);
    return ok;
}
