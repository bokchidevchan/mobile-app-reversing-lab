// iOS 방식 AES: CommonCrypto의 CCCrypt로 복호화하는 데모.
// 앱이 실행 중 CCCrypt를 부를 때 키가 인자로 들어간다 -> Frida로 그 인자를 훔친다.
#include <stdio.h>
#include <string.h>
#include <CommonCrypto/CommonCrypto.h>

int main(void) {
    unsigned char key[16] = "s3cr3t_ios_key!!";      // AES-128 키 (16바이트)
    unsigned char plain[16] = "hello_ios_world!";     // 16바이트 평문
    unsigned char cipher[32] = {0};
    unsigned char decrypted[32] = {0};
    size_t outLen = 0;

    // 1) 암호화 (ECB)
    CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode,
            key, sizeof(key), NULL,
            plain, sizeof(plain), cipher, sizeof(cipher), &outLen);

    // 2) 복호화 (여기서 CCCrypt가 키를 다시 인자로 받음)
    CCCrypt(kCCDecrypt, kCCAlgorithmAES, kCCOptionECBMode,
            key, sizeof(key), NULL,
            cipher, outLen, decrypted, sizeof(decrypted), &outLen);

    printf("복호화 결과: %s\n", decrypted);
    return 0;
}
