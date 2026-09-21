# uel_rover

로버 제어에 필요한 최소 구성만 담은 ROS 2 (Jazzy) 패키지 템플릿.
제어보드(OpenRB-150)와 통신하는 두 노드 **commander**, **monitor** 와 이를 띄우는
launch 하나로 이루어진다. 오도메트리·URDF·센서·Nav2 같은 부가 요소는 포함하지 않고
[examples/](examples/README.md) 에 참고 구현으로 두었다.

## 구성

```
uel_rover/
├── CMakeLists.txt
├── package.xml
├── config/
│   ├── rover.yaml               # commander / monitor / gcs_teleop 파라미터
│   └── foxglove_bridge.yaml     # GCS 연결(foxglove_bridge) 파라미터
├── launch/
│   ├── bringup.launch.py        # micro_ros_agent + commander + monitor
│   └── gcs.launch.py            # foxglove_bridge + gcs_teleop (GCS 연결, 선택)
├── src/
│   ├── commander.cpp            # 이동 명령 → 제어보드
│   ├── monitor.cpp              # 제어보드 상태 감시 → /diagnostics
│   ├── gcs_teleop.cpp           # GCS Teleop 방향 × 고른 속도 → /cmd_vel
│   └── teleop_speed.hpp         # 속도 단계 계산 (순수 함수)
├── test/
│   └── test_teleop_speed.cpp    # colcon test
└── examples/                    # 빌드·설치되지 않는 참고 구현
```

## 데이터 흐름

```
[상위 제어기: Nav2 / teleop / 사용자 노드]
        │ /cmd_vel (geometry_msgs/Twist)
        ▼
  ┌───────────┐ /cmd_vel_out ┌─────────────────┐  USB 시리얼  ┌────────────┐
  │ commander │ ───────────▶ │ micro_ros_agent │ ◀──────────▶ │ OpenRB-150 │
  └───────────┘              └─────────────────┘  (micro-ROS) └────────────┘
  ┌───────────┐ /wheel_velocity, /drive_mode                        │
  │  monitor  │ ◀──────────────────────────────────────────────────┘
  └───────────┘ ──▶ /diagnostics
```

드라이브 모드(RC / AUTO / STOP) 판정과 게이팅은 제어보드가 단독으로 한다.
`/cmd_vel_out` 은 AUTO 모드일 때만 모터에 적용되고, RC / STOP 에서는 무시된다.
따라서 Jetson 쪽 소프트웨어가 어떤 상태이든 조종기 스위치로 즉시 회수할 수 있다.

## 노드

### commander

| 항목 | 내용 |
|---|---|
| 구독 | `cmd_vel` (`geometry_msgs/Twist`) |
| 발행 | `cmd_vel_out` (`geometry_msgs/Twist`) |
| 동작 | `linear.x`, `angular.z` 만 남기고 상한으로 클램프해 전달. NaN/Inf 는 정지 명령으로 대체. |
| 타임아웃 | `command_timeout` 동안 새 명령이 없으면 정지 명령을 한 번 보내고 로그를 남긴다. |

| 파라미터 | 기본값 | 설명 |
|---|---|---|
| `max_linear_velocity` | 0.26 m/s | 0 이하면 클램프 비활성. `rover.yaml` 은 0.306 (RC 풀 입력 = 바퀴 45 rpm) |
| `max_angular_velocity` | 1.0 rad/s | 0 이하면 클램프 비활성. `rover.yaml` 은 2.04 (제자리 회전 시 바퀴 ±45 rpm) |
| `command_timeout` | 0.5 s | 0 이하면 비활성 |

commander 는 명령을 반복 발행하지 않는다. 제어보드 워치독(500 ms) 안에 다음
`cmd_vel` 이 도착해야 계속 움직이므로, 상위 제어기는 주기적으로 발행해야 한다.

### monitor

| 항목 | 내용 |
|---|---|
| 구독 | `wheel_velocity` (`std_msgs/Float32MultiArray` `[vL_rpm, vR_rpm]`), `drive_mode` (`std_msgs/String`) |
| 발행 | `/diagnostics` (`diagnostic_msgs/DiagnosticArray`) |
| 로그 | 링크 연결/단절, 드라이브 모드 변화, 비물리적 바퀴 속도 |

| 파라미터 | 기본값 | 설명 |
|---|---|---|
| `link_timeout` | 0.5 s | 이 시간 동안 수신이 없으면 링크 단절 |
| `max_wheel_rpm` | 100 rpm | 넘는 값은 통신 오류로 간주 |
| `diagnostics_period` | 1.0 s | `/diagnostics` 발행 주기 |

`/diagnostics` 상태 `uel_rover/control_board` 의 레벨:

| 레벨 | 조건 |
|---|---|
| STALE | 제어보드에서 아직 아무것도 받지 못함 |
| ERROR | 받다가 `link_timeout` 이상 끊김 |
| WARN | 직전 주기 동안 비물리적 바퀴 속도가 있었음 |
| OK | 정상. message 에 현재 드라이브 모드 표시 |

values: `drive_mode`, `wheel_left_rpm`, `wheel_right_rpm`, `wheel_velocity_rate_hz`,
`wheel_velocity_age_s`, `drive_mode_age_s`, `bad_wheel_samples`.

### gcs_teleop

`gcs.launch.py` 에서만 뜬다. GCS 의 Teleop 패널은 방향만 보내고, 속도는 "+" / "−" 버튼으로
고른 단계를 이 노드가 곱한다. commander 입장에서는 또 하나의 상위 제어기다.

| 항목 | 내용 |
|---|---|
| 구독 | `gcs/teleop` (`geometry_msgs/Twist`, 방향 −1..1), `gcs/speed_step` (`std_msgs/Int8`, 부호만 사용) |
| 발행 | `cmd_vel` (`geometry_msgs/Twist`), `gcs/speed` (`std_msgs/String`, 예: `4/10  0.12 m/s  0.82 rad/s`) |
| 동작 | 단계 / `speed_levels` 비율로 선속도·각속도를 함께 정한다. 방향 입력은 ±1 로 자르고 NaN/Inf 는 0. |
| 발행 시점 | `cmd_vel` 은 입력이 올 때만(반복 발행 없음). `gcs/speed` 는 변경 시 + 1 Hz. |

| 파라미터 | 기본값 | 설명 |
|---|---|---|
| `max_linear_speed` | 0.26 m/s | 최고 단계의 선속도. commander 상한 이하로 둔다. `rover.yaml` 은 0.306 |
| `max_angular_speed` | 1.0 rad/s | 최고 단계의 각속도. commander 상한 이하로 둔다. `rover.yaml` 은 2.04 |
| `speed_levels` | 10 | 단계 수 |
| `initial_level` | 6 | 시작 단계. `rover.yaml` 은 4 |

## 빌드

```bash
cd ros2_ws
source /opt/ros/jazzy/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
```

`micro_ros_agent` 는 apt 패키지가 아니라 별도 워크스페이스(`microros_ws/`, 저장소 루트)에서
소스 빌드한다. 저장소 루트의 `scripts/setup.sh` 가 위 절차와 agent 빌드를 한 번에 처리한다.
`microros_ws/install/local_setup.bash` 도 함께 source 해야 launch 가 agent 를 찾는다.

## 실행

```bash
ros2 launch uel_rover bringup.launch.py                       # agent 포함
ros2 launch uel_rover bringup.launch.py serial_port:=/dev/ttyACM1
ros2 launch uel_rover bringup.launch.py start_agent:=false     # agent 를 따로 띄울 때
```

| launch 인자 | 기본값 |
|---|---|
| `serial_port` | `/dev/ttyACM0` |
| `baudrate` | `115200` |
| `start_agent` | `true` |
| `params_file` | `config/rover.yaml` |

시리얼 장치 접근에는 `dialout` 그룹이 필요하다.

### GCS 연결 (선택)

```bash
sudo apt install ros-jazzy-foxglove-bridge      # 최초 1회
ros2 launch uel_rover gcs.launch.py             # 기본 ws://0.0.0.0:8765
ros2 launch uel_rover gcs.launch.py port:=9000
```

GCS PC 의 Foxglove 앱이 `ws://<로버 IP>:8765` 로 접속해 `/diagnostics` 등을 보고,
Teleop 패널(`/gcs/teleop`)과 속도 +/− 버튼(`/gcs/speed_step`)으로 조종한다. 같은 launch 가
띄우는 gcs_teleop 이 이를 `/cmd_vel` 로 바꾼다. [config/foxglove_bridge.yaml](config/foxglove_bridge.yaml)
의 `client_topic_whitelist` 는 GCS 발행을 이 두 토픽으로 제한하려는 설정이지만, apt 의
foxglove_bridge 3.5.0 은 이 파라미터를 적용하지 않는다(설정 파일 주석 참고).
앱 설치·접속·레이아웃은 저장소 루트의 [gcs/README.md](../../../gcs/README.md) 참고.

## 동작 확인

```bash
ros2 topic echo /diagnostics                 # 링크 상태, 모드, 바퀴 rpm
ros2 topic echo /drive_mode                  # 조종기 스위치 상태
# 조종기를 AUTO 로 둔 상태에서
ros2 topic pub -r 10 /cmd_vel geometry_msgs/msg/Twist "{linear: {x: 0.1}}"
# 또는
ros2 run teleop_twist_keyboard teleop_twist_keyboard
```

## 확장

노드를 추가하려면 `src/` 에 파일을 만들고 `CMakeLists.txt` 의 `add_executable` /
`target_link_libraries` / `install(TARGETS ...)` 에 넣는다. launch 와 yaml 은
`launch/`, `config/` 에 두면 디렉터리째 설치된다.

기존 시스템(오도메트리, RealSense, slam_toolbox, Nav2, AMCL)을 되살리는 방법은
[examples/README.md](examples/README.md) 를 참고한다.
