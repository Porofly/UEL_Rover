#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────────────
#  UEL Rover — 로버(Jetson) 최초 1회 환경 설정
#
#  하는 일 (이미 끝난 단계는 건너뛰므로 재실행해도 안전하다)
#    1. ROS 2 Jazzy 설치 (/opt/ros/jazzy 가 없을 때만, 공식 apt 절차)
#    2. 빌드 도구 설치 + rosdep 초기화
#    3. ros2_ws 의존성 설치 (rosdep) — foxglove_bridge 포함
#    4. [--with-examples] examples/ 용 패키지 (RealSense, Nav2, slam_toolbox ...)
#    5. micro-ROS agent 소스 빌드 → microros_ws/ (git 추적 안 함)
#    6. ros2_ws colcon 빌드
#    7. dialout 그룹 추가 (제어보드 USB 시리얼 접근)
#
#  사용법
#    ./scripts/setup.sh [--with-examples] [--skip-agent] [--skip-build] [--bashrc]
#      --with-examples  examples/ 에 필요한 ROS 패키지도 설치
#      --skip-agent     micro-ROS agent 빌드 생략
#      --skip-build     ros2_ws 빌드 생략
#      --bashrc         ~/.bashrc 에 환경 source 블록 추가 (한 번만 추가됨)
#
#  전제: Ubuntu 24.04, sudo 사용 가능, 인터넷 연결.
#  완료 후 재로그인(또는 `newgrp dialout`) 해야 /dev/ttyACM0 에 접근할 수 있다.
# ─────────────────────────────────────────────────────────────────
set -eo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROS_DISTRO=jazzy
ROS2_WS="$REPO_DIR/ros2_ws"
MICROROS_WS="${MICROROS_WS:-$REPO_DIR/microros_ws}"
MICROROS_SETUP_URL=https://github.com/micro-ROS/micro_ros_setup.git
CURRENT_USER="${USER:-$(id -un)}"

WITH_EXAMPLES=0
SKIP_AGENT=0
SKIP_BUILD=0
ADD_BASHRC=0

usage() {
    echo "사용법: $0 [--with-examples] [--skip-agent] [--skip-build] [--bashrc]"
    echo "  --with-examples  examples/ 용 ROS 패키지도 설치"
    echo "  --skip-agent     micro-ROS agent 빌드 생략"
    echo "  --skip-build     ros2_ws 빌드 생략"
    echo "  --bashrc         ~/.bashrc 에 환경 source 블록 추가"
}
for arg in "$@"; do
    case "$arg" in
        --with-examples) WITH_EXAMPLES=1 ;;
        --skip-agent)    SKIP_AGENT=1 ;;
        --skip-build)    SKIP_BUILD=1 ;;
        --bashrc)        ADD_BASHRC=1 ;;
        -h|--help)       usage; exit 0 ;;
        *) echo "알 수 없는 옵션: $arg"; usage; exit 1 ;;
    esac
done

# apt 는 항상 비대화식으로 (tzdata 등 프롬프트 방지)
APT="sudo env DEBIAN_FRONTEND=noninteractive apt-get -y -q"
step() { echo; echo "──── $* ────"; }

# ── 0. 사전 확인 ─────────────────────────────────────────────────
step "[0/7] 사전 확인"
. /etc/os-release
echo "OS: ${PRETTY_NAME:-?} ($(uname -m)), 사용자: $CURRENT_USER, 저장소: $REPO_DIR"
if [ "${VERSION_CODENAME:-}" != "noble" ]; then
    echo "경고: ROS 2 Jazzy 는 Ubuntu 24.04(noble) 기준이다. 다른 버전에서는 apt 설치가 실패할 수 있다."
fi
# sudo 가능 여부 확인. NOPASSWD 면 그냥 통과, 아니면 여기서 비밀번호를 한 번 묻는다.
sudo -n true 2>/dev/null || sudo -v

# ── 1. ROS 2 Jazzy ───────────────────────────────────────────────
step "[1/7] ROS 2 $ROS_DISTRO"
if [ -f "/opt/ros/$ROS_DISTRO/setup.bash" ]; then
    echo "이미 설치됨: /opt/ros/$ROS_DISTRO"
else
    $APT update
    $APT install locales software-properties-common curl ca-certificates
    # UTF-8 로케일 (ROS 2 요구사항)
    if ! locale 2>/dev/null | grep -q 'LANG=.*UTF-8'; then
        sudo locale-gen en_US en_US.UTF-8
        sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
    fi
    export LANG=en_US.UTF-8 LC_ALL=en_US.UTF-8
    sudo add-apt-repository -y universe
    # 공식 절차: ros2-apt-source 패키지로 ROS apt 저장소 + 키 등록
    ROS_APT_SOURCE_VERSION="$(curl -fsSL https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest \
        | grep -F '"tag_name"' | awk -F'"' '{print $4}')"
    curl -fsSL -o /tmp/ros2-apt-source.deb \
        "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.${VERSION_CODENAME}_all.deb"
    $APT install /tmp/ros2-apt-source.deb
    $APT update
    $APT install "ros-$ROS_DISTRO-ros-base" ros-dev-tools
fi

# ── 2. 빌드 도구 + rosdep ────────────────────────────────────────
step "[2/7] 빌드 도구 + rosdep"
$APT update
$APT install build-essential cmake git python3-pip \
    python3-colcon-common-extensions python3-rosdep python3-vcstool
# shellcheck disable=SC1090
source "/opt/ros/$ROS_DISTRO/setup.bash"
if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then
    sudo rosdep init
fi
rosdep update

# ── 3. ros2_ws 의존성 ────────────────────────────────────────────
step "[3/7] ros2_ws 의존성 (rosdep)"
rosdep install --from-paths "$ROS2_WS/src" --ignore-src -r -y
# foxglove_bridge 는 uel_rover 의 exec_depend 라 위에서 함께 설치된다.
# 이 아키텍처용 apt 패키지가 없으면 rosdep 이 경고만 내고 지나가므로 여기서 확인한다.
if ! dpkg -s "ros-$ROS_DISTRO-foxglove-bridge" >/dev/null 2>&1; then
    echo "경고: ros-$ROS_DISTRO-foxglove-bridge 가 설치되지 않았다. GCS 연결(gcs.launch.py)에 필요."
    echo "      소스 빌드: https://github.com/foxglove/foxglove-sdk/tree/main/ros"
fi

# ── 4. examples 의존성 (선택) ────────────────────────────────────
step "[4/7] examples 의존성"
if [ "$WITH_EXAMPLES" = 1 ]; then
    $APT install \
        "ros-$ROS_DISTRO-robot-state-publisher" \
        "ros-$ROS_DISTRO-tf2" "ros-$ROS_DISTRO-tf2-ros" "ros-$ROS_DISTRO-nav-msgs" \
        "ros-$ROS_DISTRO-realsense2-camera" \
        "ros-$ROS_DISTRO-pointcloud-to-laserscan" \
        "ros-$ROS_DISTRO-navigation2" "ros-$ROS_DISTRO-nav2-bringup" \
        "ros-$ROS_DISTRO-slam-toolbox" \
        "ros-$ROS_DISTRO-nav2-amcl" "ros-$ROS_DISTRO-nav2-map-server"
else
    echo "건너뜀 (--with-examples 로 설치)"
fi

# ── 5. micro-ROS agent ───────────────────────────────────────────
step "[5/7] micro-ROS agent ($MICROROS_WS)"
if [ "$SKIP_AGENT" = 1 ]; then
    echo "건너뜀 (--skip-agent)"
elif [ -d "$MICROROS_WS/install/micro_ros_agent" ]; then
    echo "이미 빌드됨"
else
    mkdir -p "$MICROROS_WS/src"
    if [ ! -d "$MICROROS_WS/src/micro_ros_setup" ]; then
        git clone --depth 1 -b "$ROS_DISTRO" "$MICROROS_SETUP_URL" "$MICROROS_WS/src/micro_ros_setup"
    fi
    (
        cd "$MICROROS_WS"
        rosdep install --from-paths src --ignore-src -r -y
        colcon build
        # shellcheck disable=SC1091
        source install/local_setup.bash
        [ -d src/uros ] || ros2 run micro_ros_setup create_agent_ws.sh
        ros2 run micro_ros_setup build_agent.sh
    )
fi

# ── 6. ros2_ws 빌드 ──────────────────────────────────────────────
step "[6/7] ros2_ws 빌드"
if [ "$SKIP_BUILD" = 1 ]; then
    echo "건너뜀 (--skip-build)"
else
    (
        cd "$ROS2_WS"
        colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
    )
fi

# ── 7. dialout ───────────────────────────────────────────────────
step "[7/7] dialout 그룹"
if id -nG "$CURRENT_USER" | grep -qw dialout; then
    echo "이미 dialout 그룹에 속해 있음"
else
    sudo usermod -aG dialout "$CURRENT_USER"
    echo "추가됨. 반영하려면 재로그인 또는 'newgrp dialout'"
fi

# ── ~/.bashrc (선택) ─────────────────────────────────────────────
ENV_LINES="source /opt/ros/$ROS_DISTRO/setup.bash
[ -f \"$MICROROS_WS/install/local_setup.bash\" ] && source \"$MICROROS_WS/install/local_setup.bash\"
[ -f \"$ROS2_WS/install/setup.bash\" ] && source \"$ROS2_WS/install/setup.bash\""
if [ "$ADD_BASHRC" = 1 ]; then
    if grep -q '# >>> UEL_Rover >>>' ~/.bashrc 2>/dev/null; then
        echo "~/.bashrc 에 이미 UEL_Rover 블록이 있음"
    else
        printf '\n# >>> UEL_Rover >>>\n%s\n# <<< UEL_Rover <<<\n' "$ENV_LINES" >> ~/.bashrc
        echo "~/.bashrc 에 환경 source 블록 추가됨"
    fi
fi

echo
echo "══════════════════════════════════════════════════════════════"
echo " 완료. 새 셸에서 다음을 source 한 뒤 실행한다."
echo "   $ENV_LINES" | sed 's/^/   /'
echo
echo "   ros2 launch uel_rover bringup.launch.py     # 제어 스택 (agent + commander + monitor)"
echo "   ros2 launch uel_rover gcs.launch.py         # GCS(Foxglove) 브릿지"
echo "══════════════════════════════════════════════════════════════"
