// iOS 방식 AES 키 탈취용 Frida 스크립트.
// CommonCrypto의 CCCrypt는 복호화할 때 키를 인자로 받는다. 그 순간 인자를 읽는다.
//
// 실행(로컬 macOS 데모): frida 로 cc-demo를 spawn하며 주입
//   (CommonCrypto는 macOS에도 있어서 iOS 없이 연습 가능. 실제 iOS는 탈옥/gadget)

var CCCrypt = Module.getExportByName(null, 'CCCrypt');
Interceptor.attach(CCCrypt, {
    onEnter: function (args) {
        // CCCrypt(op, alg, options, key, keyLength, iv, dataIn, dataInLen, dataOut, ...)
        var op = args[0].toInt32();            // 0=encrypt, 1=decrypt
        var keyLen = args[4].toInt32();
        var u8 = new Uint8Array(args[3].readByteArray(keyLen));
        var key = '';
        for (var i = 0; i < u8.length; i++) key += String.fromCharCode(u8[i]);
        console.log('[+] CCCrypt(' + (op === 1 ? '복호화' : '암호화') +
                    ') -> 키(' + keyLen + 'B): "' + key + '"');
    }
});
