#!/bin/bash
# ============================================================
#  MAC-Collector 의존성 설치 및 환경 설정 스크립트
#  실행: chmod +x setup.sh && sudo bash setup.sh
# ============================================================
set -e

if [ "$(id -u)" -ne 0 ]; then
    echo "Usage: sudo $0"
    exit 1
fi

REAL_USER="${SUDO_USER:-$USER}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
QT_DIR="/home/$REAL_USER/Qt/6.11.0/gcc_64"
PLUGIN_DIR="$QT_DIR/plugins/platforminputcontexts"

echo "========================================"
echo " MAC-Collector 환경 설정 시작"
echo "========================================"

# ── 1. 시스템 패키지 설치 ──
echo "[1/6] 시스템 패키지 설치 중..."
sudo apt-get update -qq

# 개별 설치 (없는 패키지는 건너뜀)
PACKAGES=(
    cmake
    libpcap-dev
    libsqlite3-dev
    fonts-nanum
    ibus
    ibus-hangul
    libibus-1.0-dev
    pipewire
    pipewire-pulse
    wireplumber
    pulseaudio
    libpulse-dev
    ffmpeg
    qt6-base-dev
    qt6-multimedia-dev
    qt6-qpa-plugins
    gstreamer1.0-plugins-base
    gstreamer1.0-plugins-good
    gstreamer1.0-plugins-bad
    gstreamer1.0-libav
    gstreamer1.0-pulseaudio
)

for pkg in "${PACKAGES[@]}"; do
    if sudo apt-get install -y "$pkg" 1> /dev/null; then
        echo "  ✓ $pkg"
    else
        echo "  ⚠ $pkg (건너뜀 - 없는 패키지)"
    fi
done

echo "[1/6] 완료"

# ── 2. ibus Qt6 플러그인 복사 ──
echo "[2/6] ibus Qt6 한글 입력 플러그인 설정 중..."

SYSTEM_IBUS_PLUGIN=""
for candidate in \
    "/usr/lib/x86_64-linux-gnu/qt6/plugins/platforminputcontexts/libibusplatforminputcontextplugin.so" \
    "/usr/lib/x86_64-linux-gnu/qt6/plugins/platforminputcontexts/libqibusplatforminputcontextplugin.so"; do
    if [ -f "$candidate" ]; then
        SYSTEM_IBUS_PLUGIN="$candidate"
        break
    fi
done

if [ -n "$SYSTEM_IBUS_PLUGIN" ]; then
    mkdir -p "$PLUGIN_DIR"
    cp "$SYSTEM_IBUS_PLUGIN" "$PLUGIN_DIR/"
    echo "  → 플러그인 복사 완료: $PLUGIN_DIR/$(basename $SYSTEM_IBUS_PLUGIN)"
else
    echo "  ⚠ ibus Qt6 플러그인을 찾지 못했습니다."
    echo "    설치 후 위치 확인:"
    echo "    find /usr -name '*ibus*inputcontext*.so' 2>/dev/null"
fi
echo "[2/6] 완료"

# ── 3. 빌드 ──
echo "[3/6] 프로젝트 빌드 중..."
BIN_PATH="$SCRIPT_DIR/bin/mac-collector"
if cmake -S "$SCRIPT_DIR" -B "$SCRIPT_DIR/build" && cmake --build "$SCRIPT_DIR/build" -j$(nproc); then
    echo "[3/6] 빌드 완료"
else
    echo "  ⚠ 빌드 실패. 수동으로 빌드하세요:"
    echo "    cd $SCRIPT_DIR && cmake -S . -B build && cmake --build build -j\$(nproc)"
    exit 1
fi

# ── 4. setcap 설정 ──
echo "[4/6] pcap 권한 설정 (setcap)..."
if [ -f "$BIN_PATH" ]; then
    setcap cap_net_raw,cap_net_admin=eip "$BIN_PATH"
    if getcap "$BIN_PATH" 2>/dev/null | grep -q cap_net_raw; then
        echo "  → setcap 검증 완료: $BIN_PATH"
    else
        echo "  ⚠ setcap 적용 확인 실패"
    fi
else
    echo "  ⚠ 바이너리 없음. 빌드 후 수동 실행:"
    echo "    sudo setcap cap_net_raw,cap_net_admin=eip $BIN_PATH"
fi
echo "[4/6] 완료"

# ── 5. ibus 자동시작 등록 ──
echo "[5/6] ibus 자동시작 설정..."
AUTOSTART_DIR="/home/$REAL_USER/.config/autostart"
mkdir -p "$AUTOSTART_DIR"
cat > "$AUTOSTART_DIR/ibus.desktop" << 'DESKTOP'
[Desktop Entry]
Type=Application
Name=IBus
Exec=ibus-daemon -drx
Hidden=false
NoDisplay=false
X-GNOME-Autostart-enabled=true
DESKTOP
chown "$REAL_USER:$REAL_USER" "$AUTOSTART_DIR/ibus.desktop"
echo "[5/6] 완료"

# ── 6. 실행 스크립트 생성 ──
echo "[6/6] run.sh 생성 중..."
RUN_SCRIPT="$SCRIPT_DIR/run.sh"
cat > "$RUN_SCRIPT" << 'RUNSCRIPT'
#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/bin/mac-collector"

if [ ! -f "$BIN" ]; then
    echo "오류: 바이너리 없음. sudo ./setup.sh 를 먼저 실행하세요."
    exit 1
fi

if ! getcap "$BIN" 2>/dev/null | grep -q cap_net_raw; then
    echo "오류: setcap 미적용. sudo ./setup.sh 를 먼저 실행하세요."
    exit 1
fi

ibus-daemon -drx 2>/dev/null || true

export QT_IM_MODULE=ibus
export XMODIFIERS="@im=ibus"
export GTK_IM_MODULE=ibus

"$BIN" "$@"
RUNSCRIPT

chmod +x "$RUN_SCRIPT"
chown "$REAL_USER:$REAL_USER" "$RUN_SCRIPT"
echo "[6/6] 완료: $RUN_SCRIPT"

echo ""
echo "========================================"
echo " 설정 완료!"
echo "========================================"
echo "  실행: $SCRIPT_DIR/run.sh"
echo "========================================"