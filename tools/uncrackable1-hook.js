// UnCrackable L1 동적 분석용 Frida 스크립트
//
// 하는 일 두 가지:
//  1) 루트/디버그 탐지 함수의 반환값을 false로 바꿔치기해서 앱이 안 죽게 한다.
//  2) AES 복호화 함수를 후킹해서, 앱이 실행 중에 푼 정답을 메모리에서 가로챈다.
//
// 실행: frida -D emulator-5556 -f owasp.mstg.uncrackable1 -l tools/uncrackable1-hook.js

Java.perform(function () {
    // 1) 루트 탐지 (sg.vantagepoint.a.c 의 a/b/c) 전부 false로
    var c = Java.use('sg.vantagepoint.a.c');
    ['a', 'b', 'c'].forEach(function (m) {
        c[m].implementation = function () {
            console.log('[*] 루트 탐지 c.' + m + '() 호출됨 -> false로 우회');
            return false;
        };
    });

    // 2) 디버그 탐지 (sg.vantagepoint.a.b.a) false로
    var b = Java.use('sg.vantagepoint.a.b');
    b.a.implementation = function (ctx) {
        console.log('[*] 디버그 탐지 b.a() 호출됨 -> false로 우회');
        return false;
    };

    // 3) System.exit 무력화 (탐지에 걸려도 안 죽게 이중 안전장치)
    var System = Java.use('java.lang.System');
    System.exit.implementation = function (code) {
        console.log('[*] System.exit(' + code + ') 차단');
    };

    // 4) AES 복호화 결과를 런타임에 가로채기
    var aes = Java.use('sg.vantagepoint.a.a');
    aes.a.overload('[B', '[B').implementation = function (key, ct) {
        var ret = this.a(key, ct);
        var plain = Java.use('java.lang.String').$new(ret);
        console.log('[+] 런타임에 복호화된 정답: ' + plain);
        return ret;
    };

    // 5) 검증 함수를 직접 호출해서 복호화를 유발 (버튼 안 눌러도 정답이 나온다)
    var verifier = Java.use('sg.vantagepoint.uncrackable1.a');
    console.log('[*] verifier.a("test") 직접 호출 -> ' + verifier.a('test'));
});
