// Mach-O 정적/동적 분석 연습용으로 직접 만든 작은 크랙미.
// 안드로이드 UnCrackable처럼 정답을 평문 대신 XOR로 숨겨놨다.
// 빌드:   clang -arch arm64 -o macho-crackme tools/macho-crackme.c
// 정적:   otool -h / nm / strings 로 뜯고, ENC 바이트를 KEY로 XOR하면 정답
// 동적:   frida로 strcmp 후킹해서 런타임에 정답 노출
// (여기 ENC/KEY는 내가 넣은 데모용 값이라 개인정보 아님)
#include <stdio.h>
#include <string.h>
static const unsigned char ENC[] = {0x42,0x44,0x58,0x74,0x43,0x42,0x4f,0x4f,0x4e,0x45,0x74,0x4d,0x47,0x4a,0x4c};
static const unsigned char KEY = 0x2b;
static int check(const char *in){
    char dec[64]; size_t n=sizeof(ENC);
    for(size_t i=0;i<n;i++) dec[i]=ENC[i]^KEY;
    dec[n]=0;
    return strcmp(in,dec)==0;
}
int main(int argc,char**argv){
    if(argc<2){printf("usage: %s <secret>\n",argv[0]);return 1;}
    printf(check(argv[1])?"correct\n":"nope\n");
    return 0;
}
