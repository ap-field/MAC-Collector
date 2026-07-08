#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/bin/mac-collector"
QT_PLUGIN_DIR="$HOME/Qt/6.11.0/gcc_64/plugins/platforminputcontexts"

if [ ! -f "$BIN" ]; then
    echo "Error: Binary not found. You should build first."
    exit 1
fi

# /etc/environment 는 로그인 시 로드되므로 현재 세션에 없으면 직접 읽는다.
if [ -z "$MACCOLLECTOR_API_KEY" ] || [ -z "$MACCOLLECTOR_API_URL" ]; then
    MACCOLLECTOR_API_KEY="$(grep -E '^MACCOLLECTOR_API_KEY=' /etc/environment | head -n1 | cut -d= -f2-)"
    MACCOLLECTOR_API_URL="$(grep -E '^MACCOLLECTOR_API_URL=' /etc/environment | head -n1 | cut -d= -f2-)"
    export MACCOLLECTOR_API_KEY MACCOLLECTOR_API_URL
fi
if [ -z "$MACCOLLECTOR_API_KEY" ]; then
    echo "Error: API 키가 없습니다. 'sudo bash setup.sh' 를 실행해 키를 등록하세요."
    exit 1
fi
if [ -z "$MACCOLLECTOR_API_URL" ]; then
    echo "Error: API URL이 없습니다. 'sudo bash setup.sh' 를 실행해 URL을 등록하세요."
    exit 1
fi

# 재빌드 시 capability가 사라지므로 매 실행마다 확인 후 자동 재적용
if ! getcap "$BIN" 2>/dev/null | grep -q cap_net_raw; then
    echo "[권한 설정] pcap capability 적용 중 (sudo 비밀번호 필요)..."
    sudo setcap cap_net_raw,cap_net_admin=eip "$BIN"
    if ! getcap "$BIN" 2>/dev/null | grep -q cap_net_raw; then
        echo "Error: setcap 적용 실패. sudo ./setup.sh 를 먼저 실행하세요."
        exit 1
    fi
    echo "  → capability 적용 완료"
fi

# ── 한글 입력기 선택 ──
# fcitx5: Qt6 네이티브 플러그인 제공, 비GNOME 환경에서 ibus보다 안정적
# ibus: Qt 인스톨러에 플러그인 내장돼 있으나 비GNOME에서 hangul 엔진 자동설정 없음
if command -v fcitx5 &>/dev/null && \
   [ -f "$QT_PLUGIN_DIR/libfcitx5platforminputcontextplugin.so" ]; then
    if ! pgrep -x fcitx5 > /dev/null 2>&1; then
        fcitx5 -d 2>/dev/null || true
        for i in $(seq 15); do
            sleep 0.2
            fcitx5-remote > /dev/null 2>&1 && break
        done
    fi
    export QT_IM_MODULE=fcitx
    export XMODIFIERS="@im=fcitx"
    export GTK_IM_MODULE=fcitx
    echo "[입력기] fcitx5  (한/영 전환: Ctrl+Space)"
else
    if ! pgrep -x ibus-daemon > /dev/null 2>&1; then
        ibus-daemon -drx 2>/dev/null || true
        sleep 1
    fi
    export IBUS_USE_PORTAL=0
    export QT_IM_MODULE=ibus
    export XMODIFIERS="@im=ibus"
    export GTK_IM_MODULE=ibus
    echo "[입력기] ibus  (한/영 전환: Ctrl+Space)"
fi

exec "$BIN" "$@"
