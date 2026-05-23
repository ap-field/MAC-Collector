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
    fcitx5                  # Qt6 네이티브 한글 입력, 비GNOME 환경 권장
    fcitx5-hangul
    fcitx5-frontend-qt6     # Qt6 입력 컨텍스트 플러그인 제공
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

# ── 2. 한글 입력기 Qt6 플러그인 설정 ──
# 우선순위: fcitx5 (비GNOME 환경에서 ibus보다 안정적) > ibus
echo "[2/6] 한글 입력기 Qt6 플러그인 설정 중..."
mkdir -p "$PLUGIN_DIR"

# fcitx5 Qt6 플러그인 복사 (fcitx5-frontend-qt6 패키지 제공)
FCITX5_PLUGIN=""
for candidate in \
    "/usr/lib/x86_64-linux-gnu/qt6/plugins/platforminputcontexts/libfcitx5platforminputcontextplugin.so" \
    $(find /usr/lib -name 'libfcitx5*inputcontext*.so' -path '*/qt6/*' 2>/dev/null | head -1); do
    if [ -f "$candidate" ]; then
        FCITX5_PLUGIN="$candidate"
        break
    fi
done

if [ -n "$FCITX5_PLUGIN" ]; then
    cp "$FCITX5_PLUGIN" "$PLUGIN_DIR/"
    echo "  ✓ fcitx5 Qt6 플러그인 복사: $PLUGIN_DIR/$(basename $FCITX5_PLUGIN)"
else
    echo "  ⚠ fcitx5 Qt6 플러그인 없음 (fcitx5-frontend-qt6 설치 확인)"
fi

# fcitx5 한글 프로필 생성 (없을 때만)
FCITX5_CONF="/home/$REAL_USER/.config/fcitx5"
if [ ! -f "$FCITX5_CONF/profile" ]; then
    mkdir -p "$FCITX5_CONF"
    cat > "$FCITX5_CONF/profile" << 'FCITX_PROFILE'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=hangul

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=hangul
Layout=

[GroupOrder]
0=Default
FCITX_PROFILE
    chown -R "$REAL_USER:$REAL_USER" "$FCITX5_CONF"
    echo "  ✓ fcitx5 한글 프로필 생성 (한/영 전환: 한글키 또는 Shift+Space)"
else
    echo "  → fcitx5 프로필 이미 있음. 덮어쓰지 않음."
fi

# ibus Qt6 플러그인 확인 (Qt 인스톨러에 내장된 경우 복사 불필요)
if [ -f "$PLUGIN_DIR/libibusplatforminputcontextplugin.so" ]; then
    echo "  ✓ ibus Qt6 플러그인 내장 확인"
else
    for candidate in \
        "/usr/lib/x86_64-linux-gnu/qt6/plugins/platforminputcontexts/libibusplatforminputcontextplugin.so" \
        $(find /usr/lib -name '*ibus*inputcontext*.so' -path '*/qt6/*' 2>/dev/null | head -1); do
        if [ -f "$candidate" ]; then
            cp "$candidate" "$PLUGIN_DIR/"
            echo "  ✓ ibus Qt6 플러그인 복사: $(basename $candidate)"
            break
        fi
    done
fi

echo "  → 플러그인 디렉토리 현황:"
ls "$PLUGIN_DIR" | sed 's/^/    /'
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

# ── 4. setcap 설정 + sudoers 규칙 ──
echo "[4/6] pcap 권한 설정 (setcap + sudoers)..."

# 바이너리에 capability 적용
if [ -f "$BIN_PATH" ]; then
    setcap cap_net_raw,cap_net_admin=eip "$BIN_PATH"
    if getcap "$BIN_PATH" 2>/dev/null | grep -q cap_net_raw; then
        echo "  → setcap 검증 완료: $BIN_PATH"
    else
        echo "  ⚠ setcap 적용 확인 실패"
    fi
else
    echo "  ⚠ 바이너리 없음. 빌드 후 자동 적용됨 (run.sh 실행 시)"
fi

# sudoers NOPASSWD 규칙:
#   - setcap: run.sh가 재빌드 후 capability를 비밀번호 없이 재적용
#   - iwconfig: cap_net_admin은 자식 프로세스에 상속되지 않으므로 sudo -n 으로 실행
SUDOERS_FILE="/etc/sudoers.d/mac-collector"
SETCAP_BIN="$(command -v setcap 2>/dev/null || echo /usr/sbin/setcap)"
IWCONFIG_BIN="$(command -v iwconfig 2>/dev/null || echo /usr/sbin/iwconfig)"
cat > "$SUDOERS_FILE" << EOF
# MAC-Collector: 비밀번호 없이 capability 재적용 및 채널 설정 허용
$REAL_USER ALL=(root) NOPASSWD: $SETCAP_BIN
$REAL_USER ALL=(root) NOPASSWD: $IWCONFIG_BIN
EOF
chmod 440 "$SUDOERS_FILE"
# visudo -c로 문법 검증
if visudo -c -f "$SUDOERS_FILE" &>/dev/null; then
    echo "  → sudoers 규칙 적용: $SUDOERS_FILE"
else
    echo "  ⚠ sudoers 문법 오류. 파일을 삭제합니다."
    rm -f "$SUDOERS_FILE"
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
QT_PLUGIN_DIR="$HOME/Qt/6.11.0/gcc_64/plugins/platforminputcontexts"

if [ ! -f "$BIN" ]; then
    echo "오류: 바이너리 없음. 먼저 빌드하세요."
    exit 1
fi

# 재빌드 시 capability가 사라지므로 매 실행마다 확인 후 자동 재적용
if ! getcap "$BIN" 2>/dev/null | grep -q cap_net_raw; then
    echo "[권한 설정] pcap capability 적용 중 (sudo 비밀번호 필요)..."
    sudo setcap cap_net_raw,cap_net_admin=eip "$BIN"
    if ! getcap "$BIN" 2>/dev/null | grep -q cap_net_raw; then
        echo "오류: setcap 적용 실패. sudo ./setup.sh 를 먼저 실행하세요."
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
    echo "[입력기] fcitx5  (한/영 전환: 한글키 또는 Shift+Space)"
else
    if ! pgrep -x ibus-daemon > /dev/null 2>&1; then
        ibus-daemon -drx 2>/dev/null || true
        sleep 1
    fi
    export IBUS_USE_PORTAL=0
    export QT_IM_MODULE=ibus
    export XMODIFIERS="@im=ibus"
    export GTK_IM_MODULE=ibus
    echo "[입력기] ibus  (한/영 전환: 한글키 또는 Shift+Space)"
fi

exec "$BIN" "$@"
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