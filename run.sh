#!/bin/bash
# 사용법: bash run.sh <인터페이스>  예) bash run.sh mon0

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/build/Desktop-Debug/mac-collector"

if [ ! -f "$BIN" ]; then
    echo "오류: 바이너리 없음. 먼저 빌드하세요."
    exit 1
fi

# ibus 데몬 실행
ibus-daemon -drx 2>/dev/null || true

# setcap 적용 여부 확인
if getcap "$BIN" 2>/dev/null | grep -q cap_net_raw; then
    # setcap 적용됨 → 일반 사용자 실행 (오디오·한글 정상)
    echo "→ 일반 사용자 모드로 실행"
    export QT_IM_MODULE=ibus
    export XMODIFIERS="@im=ibus"
    export GTK_IM_MODULE=ibus
    "$BIN" "$@"
else
    # setcap 미적용 → sudo + 환경변수 전달
    echo "→ sudo 모드로 실행 (setcap 미적용)"
    sudo -E \
        DBUS_SESSION_BUS_ADDRESS="$DBUS_SESSION_BUS_ADDRESS" \
        XDG_RUNTIME_DIR="$XDG_RUNTIME_DIR" \
        PULSE_SERVER="${PULSE_SERVER:-unix:/run/user/$(id -u)/pulse/native}" \
        QT_IM_MODULE=ibus \
        XMODIFIERS="@im=ibus" \
        GTK_IM_MODULE=ibus \
        "$BIN" "$@"
fi
