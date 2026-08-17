# Mobile App Reversing Lab

모바일 앱 바이너리를 직접 뜯어보면서 정적/동적 분석을 익히는 개인 학습 저장소.
Android와 iOS 앱이 실제로 어떻게 빌드되고, 배포된 바이너리에서 무엇을 읽어낼 수 있는지 손으로 확인하는 게 목표다.

## 다루는 주제

- [x] APK 구조와 빌드 산출물 (DEX, AAB, 리소스, 서명 v1~v4)
- [x] Android 리버싱 도구: jadx, apktool, Frida, objection (정적: UnCrackable L1 실습 완료 / 동적: 예정)
- [ ] iOS 바이너리 분석: Mach-O, class-dump, Hopper/Ghidra, ipa 구조
- [ ] 난독화와 보호 기법: R8/ProGuard, DexGuard
- [ ] 네트워크 트래픽 분석: mitmproxy, SSL Pinning
- [ ] 플랫폼 심사 정책: Google Play / App Store
- [ ] Flutter/Hybrid 앱의 바이너리 특성 (libapp.so, 스냅샷)
- [ ] 샘플 앱 정적/동적 분석 연습

## 폴더 구조

- `docs/` — 주제별 개념 정리
- `notes/` — 분석 실습 기록
- `tools/` — 분석에 쓰는 스크립트, 격리용 Dockerfile

## 진행 방식

주제를 하나씩, 개념 정리 다음 직접 실습, 그다음 기록 순서로 남긴다.
분석 대상 바이너리와 도구 출력물은 커밋하지 않는다 (`.gitignore` 참고).
