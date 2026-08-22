#!/usr/bin/env bash
# 캡스톤 iOS/macOS 네이티브 크랙미 빌드. 산출물 바이너리는 커밋 안 함.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
clang -O2 -arch arm64 -o "$HERE/ios-capstone" "$HERE/verify.c"
strip -x "$HERE/ios-capstone"   # 로컬 심볼 제거
echo "빌드 완료: $HERE/ios-capstone"
