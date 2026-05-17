#!/bin/bash
# ============================================================
#  MAC-Collector 의존성 설치 및 환경 설정 스크립트
#  실행: chmod +x setup.sh && sudo bash setup.sh
# ============================================================
set -e

REAL_USER="${SUDO_USER:-$USER}"
QT_DIR="/home/$REAL_USER/Qt/6.11.0/gcc_64"
PLUGIN_DIR="$QT_DIR/plugins/platforminputcontexts"

echo "========================================"
echo " MAC-Collector 환경 설정 시작"
echo "========================================"

# ── 1. 시스템 패키지 설치 ──
echo "[1/5] 시스템 패키지 설치 중..."
apt-get update -qq

# 개별 설치 (없는 패키지는 건너뜀)
PACKAGES=(
    libpcap-dev
    libsqlite3-dev
    ibus
    ibus-hangul
    libibus-1.0-dev
    pipewire
    pipewire-pulse
    wireplumber
    pulseaudio
    libpulse-dev
    ffmpeg
    qt6-qpa-plugins
    libqt6core6t64
    libqt6widgets6t64
    libqt6network6t64
    libqt6multimedia6t64
    gstreamer1.0-plugins-base
    gstreamer1.0-plugins-good
    gstreamer1.0-plugins-bad
    gstreamer1.0-libav
    gstreamer1.0-pulseaudio
)

for pkg in "${PACKAGES[@]}"; do
    if apt-get install -y "$pkg" 2>/dev/null; then
        echo "  ✓ $pkg"
    else
        echo "  ⚠ $pkg (건너뜀 - 없는 패키지)"
    fi
done

echo "[1/5] 완료"

# ── 2. ibus Qt6 플러그인 복사 ──
echo "[2/5] ibus Qt6 한글 입력 플러그인 설정 중..."

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
echo "[2/5] 완료"

# ── 3. setcap 설정 ──
echo "[3/5] pcap 권한 설정 (setcap)..."
BIN_PATH="/home/$REAL_USER/Desktop/MAC-Collector/build/Desktop-Debug/mac-collector"
if [ -f "$BIN_PATH" ]; then
    setcap cap_net_raw,cap_net_admin=eip "$BIN_PATH"
    echo "  → setcap 완료: $BIN_PATH"
else
    echo "  ⚠ 바이너리 없음. 빌드 후 수동 실행:"
    echo "    sudo setcap cap_net_raw,cap_net_admin=eip $BIN_PATH"
fi
echo "[3/5] 완료"

# ── 4. ibus 자동시작 등록 ──
echo "[4/5] ibus 자동시작 설정..."
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
echo "[4/5] 완료"

# ── 5. 실행 스크립트 생성 ──
echo "[5/5] run.sh 생성 중..."
RUN_SCRIPT="/home/$REAL_USER/Desktop/MAC-Collector/run.sh"
cat > "$RUN_SCRIPT" << 'RUNSCRIPT'
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
RUNSCRIPT

chmod +x "$RUN_SCRIPT"
chown "$REAL_USER:$REAL_USER" "$RUN_SCRIPT"
echo "[5/5] 완료: $RUN_SCRIPT"

echo ""
echo "========================================"
echo " 설정 완료!"
echo "========================================"
echo "  ① 빌드:   cd ~/Desktop/MAC-Collector/build/Desktop-Debug && make -j\$(nproc)"
echo "  ② setcap: sudo setcap cap_net_raw,cap_net_admin=eip ~/Desktop/MAC-Collector/build/Desktop-Debug/mac-collector"
echo "  ③ 실행:   bash ~/Desktop/MAC-Collector/run.sh mon0"
echo "========================================"