# 캡스톤 강화: 컨트롤 플로우 평탄화 붙이고 다시 크랙

캡스톤 한 단계 더. 원래 순차 로직(복호 -> 길이검사 -> 비교)을 상태머신 dispatch로 흩어놓는
컨트롤 플로우 평탄화(control-flow flattening)를 손으로 구현했다. OLLVM/Tigress를 못 구해서(설치 차단/라이선스)
개념을 직접 짜서 확인. 소스: tools/capstone/ios/verify_flat.c.

## 뭘 했나

- 모든 기본블록을 state 번호로 매기고 `for(;;) switch(state)` dispatch 루프로 변환.
- 불투명 술어(n^2+n은 항상 짝수) 호출을 dispatch에 끼워 잡음 추가.
- 문자열 XOR 암호화, ptrace(PT_DENY_ATTACH), 수동 비교는 유지.

## 빌드 함정

-O2로 빌드했더니 옵티마이저가 평탄화를 상당 부분 되돌렸다(흐름 복원).
평탄화를 유지하려면 -O0로 빌드해야 했다. 이게 손으로 짠 평탄화의 한계다.
진짜 OLLVM/Tigress는 옵티마이저가 못 풀게 dispatcher를 표시하고 더 정교하게 흩는다.

## Ghidra로 본 차이

평탄화 전(원래 iOS 캡스톤): 복호 for 루프 + 비교가 깔끔하게 순차로 보였다.

평탄화 후: 이렇게 됐다.

```c
local_5c = 1;
do {
    iVar3 = FUN_100003efc(local_5c + local_48);   // 불투명 술어
    if (iVar3 == 0) local_5c = 99;
    switch(local_5c) {
    case 1: if (sVar4 == 0x19) { local_48=0; local_5c=2; } else local_5c=0x5a; break;
    case 2: local_5c = (0x18 < local_48) ? 4 : 3; break;
    case 3: local_38[local_48] = (&DAT_100003f7f)[local_48] ^ 0x3d; local_48++; local_5c=2; break;
    case 4: ... case 5: ... case 6: 비교 ... case 7: 판정 ...
    }
} while(...);
```

로직이 상태머신으로 흩어져서 "무슨 일을 하는지" 한눈에 안 들어온다. 읽는 비용이 확 올라갔다.

## 그런데 크랙은 그대로 됐다 (핵심)

case 3에 `(&DAT_100003f7f)[i] ^ 0x3d`가 그대로 보인다. 즉 흐름은 꼬였어도:

- 암호문이 여전히 .rodata(0x3f7f)에 있고
- XOR 키(0x3d)도 코드에 그대로 있다

그래서 흐름을 하나도 안 따라가고, 암호문 25바이트를 꺼내 XOR 0x3d 하니 정답이 나왔다.

복원: `ios_capstone_cracked_2026` (평탄화 전과 동일)

## 배운 것

- **컨트롤 플로우 난독화는 "로직 읽기"를 막지, "데이터"를 안 숨긴다.**
  암호문+키가 바이너리에 있으면 흐름이 아무리 꼬여도 데이터만 뽑아 역산하면 끝.
- 데이터까지 지키려면 다른 축이 필요하다: 화이트박스 크립토(떼어낼 키 없애기),
  런타임에 키를 유도(정적 추출 불가), 패킹(암호문 자체를 숨김).
- 손으로 짠 평탄화는 -O2에 풀린다. 실전 강도는 OLLVM/Tigress 같은 도구가 옵티마이저 내성까지 갖춘 것.

## 다음 (진짜 세게)

- 데이터 은닉 축 추가: 런타임 키 유도 + 암호문 조각 분산 -> "데이터 뽑기"도 어렵게.
- OLLVM/Tigress를 정식으로 구해서 도구 수준 난독화 vs 크랙 비교.
