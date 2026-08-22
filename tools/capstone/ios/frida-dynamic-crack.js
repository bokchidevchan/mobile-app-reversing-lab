// 동적 크랙: ptrace(PT_DENY_ATTACH) 무력화 + memcmp 후킹으로 런타임 복호 정답 가로채기.
// 사용: frida로 ios-capstone-hard를 25자 더미 인자로 spawn하며 주입.
var ptrace = Module.getExportByName(null, 'ptrace');
Interceptor.replace(ptrace, new NativeCallback(function (a, b, c, d) { return 0; },
    'int', ['int', 'int', 'pointer', 'int']));   // 안티디버깅 우회

var memcmp = Module.getExportByName(null, 'memcmp');
Interceptor.attach(memcmp, {
    onEnter: function (args) {
        if (args[2].toInt32() === 25) {           // 비교 길이 = 정답 길이
            var u8 = new Uint8Array(args[1].readByteArray(25)), s = '';
            for (var i = 0; i < u8.length; i++) s += String.fromCharCode(u8[i]);
            console.log('[+] 런타임 복호된 정답: ' + s);
        }
    }
});
