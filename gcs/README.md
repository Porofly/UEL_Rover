# GCS (Foxglove)

로버 밖의 GCS PC 에서 Foxglove 앱으로 로버 상태를 보고 조종하는 구성.
기본 기능은 두 가지다.

1. **모니터링**: 로버 `monitor` 노드가 내보내는 `/diagnostics` 와 제어보드 토픽을 표시
2. **조종**: Teleop 패널이 방향을, "+" / "−" 버튼이 속도 단계를 보냄 → 로버 `gcs_teleop` 이
   둘을 곱해 `/cmd_vel` 발행 → `commander` → 제어보드

```
[GCS PC]                              [로버 (Jetson)]
Foxglove 앱  ── ws://<로버IP>:8765 ──▶ foxglove_bridge ──┬─ /diagnostics, /drive_mode,
  · 상태 표시   ◀──────────────────────                  │  /wheel_velocity, /gcs/speed, /rosout ...
  · Teleop 패널 ── /gcs/teleop ───────▶                  └─ gcs_teleop → /cmd_vel → commander
  · + / − 버튼  ── /gcs/speed_step ───▶                       → /cmd_vel_out → OpenRB-150
```

GCS PC 에는 ROS 2 가 필요 없다. 브릿지는 로버에서 돈다.

## 구성

```
gcs/
├── README.md
└── foxglove/
    └── uel_rover_gcs.json   # Foxglove 레이아웃
```

## 준비

### 로버

```bash
sudo apt install ros-jazzy-foxglove-bridge          # 최초 1회
ros2 launch uel_rover bringup.launch.py             # 제어 스택
ros2 launch uel_rover gcs.launch.py                 # 브릿지 (기본 포트 8765) + gcs_teleop
```

브릿지 파라미터는 `ros2_ws/src/uel_rover/config/foxglove_bridge.yaml`, 속도 단계 파라미터는
같은 디렉터리 `rover.yaml` 의 `gcs_teleop` 절.
`client_topic_whitelist` 로 GCS 발행을 `/gcs/teleop`, `/gcs/speed_step` 만 허용하도록 설정해
두었지만, apt 로 설치되는 foxglove_bridge 3.5.0 은 이 값을 읽기만 하고 적용하지 않는다(실측
확인). 즉 현재는 GCS 가 어떤 토픽이든 발행할 수 있으므로, 레이아웃에 이 둘 외의 발행 패널을
두지 않는다.

### GCS PC

1. Foxglove 데스크톱 앱 설치: https://foxglove.dev/download
   무료 계정으로 로그인한다(학술 용도 무료). 웹 버전은 https 페이지에서 `ws://` 로
   접속할 수 없으므로 데스크톱 앱을 쓴다.
2. 로버와 같은 네트워크에 연결하고 로버 IP 를 확인한다.

## 접속

1. Foxglove 에서 **Open connection → Foxglove WebSocket** 선택
2. URL 에 `ws://<로버 IP>:8765` 입력 후 Open
3. **Layouts → Import from file** 로 `gcs/foxglove/uel_rover_gcs.json` 불러오기

접속되면 왼쪽 Diagnostics 패널에 `uel_rover/control_board` 가 나타난다.

## 패널

| 패널 | 표시/기능 | 출처 |
|---|---|---|
| Diagnostics – Summary | 제어보드 상태 레벨 (OK / WARN / ERROR / STALE) | `/diagnostics` (monitor) |
| Diagnostics – Detail | 드라이브 모드, 바퀴 rpm, 수신 주기, 링크 경과 시간 등 상세 값 | `/diagnostics` (monitor) |
| Indicator | 드라이브 모드 색 표시: AUTO 초록 / RC 노랑 / STOP 빨강 / 수신 없음 회색 | `/drive_mode` (제어보드) |
| Plot | 좌/우 바퀴 rpm 시계열 | `/wheel_velocity` (제어보드) |
| Publish "−" / "+" | 속도 한 단계 내림 / 올림 | → `/gcs/speed_step` (gcs_teleop) |
| Raw Messages (속도) | 현재 고른 속도. 예: `4/10  0.12 m/s  0.82 rad/s` | `/gcs/speed` (gcs_teleop) |
| Teleop | 방향 버튼. 값은 방향(±1)일 뿐 속도가 아니다 | → `/gcs/teleop` (gcs_teleop) |
| Raw Messages | 제어보드로 실제 나가는 명령 확인 | `/cmd_vel_out` (commander) |
| Log | 로버 노드 로그 (INFO 이상) | `/rosout` |

## 조종 (Teleop)

- 조종기 스위치를 **AUTO** 로 둔 상태에서만 로버가 움직인다. RC / STOP 에서는 제어보드가
  `/cmd_vel_out` 을 무시한다.
- 속도는 Teleop 위의 **"−" / "+" 버튼**으로 바꾼다. 1 ~ 10 단계이고 시작은 4 단계
  (0.12 m/s, 0.82 rad/s), 최고 단계가 RC 풀 입력과 같은 0.306 m/s, 2.04 rad/s(바퀴 45 rpm, 서보 Velocity Limit 바로 아래)다. 선속도와 각속도가 같은 비율로
  변하며, 두 버튼 사이에 현재 값이 표시된다. 속도는 로버 쪽 `gcs_teleop` 이 기억하므로 GCS 를
  다시 접속해도 유지되고, `gcs.launch.py` 를 다시 띄우면 시작 단계로 돌아간다.
- 단계 수·시작 단계·최고 속도는 `rover.yaml` 의 `gcs_teleop` 절에서 바꾼다. 최고 속도를
  올리려면 `commander` 의 `max_linear_velocity` / `max_angular_velocity` 도 같이 올려야 한다
  (commander 가 최종 클램프를 한다).
- 방향 버튼을 누르는 동안 10 Hz 로 발행한다. Teleop 패널의 버튼 값(±1)은 방향이므로 바꾸지
  않는다. 1 보다 큰 값을 넣어도 고른 속도를 넘지 않는다.
- 버튼을 떼면 Teleop 이 정지(0)를 한 번 보내 즉시 멈춘다(`autoSendStopOnRelease`). 그 메시지가
  유실돼도 commander 가 0.5 s 후 정지 명령을 보내고, 제어보드에도 0.5 s 워치독이 있다.
- GCS 연결이 끊겨도 같은 경로로 로버는 정지한다. 비상시에는 조종기 스위치를 RC 나 STOP 으로
  넘기는 것이 가장 빠르다.
- GCS 에서 `/cmd_vel_out` 으로 직접 발행하면 commander 의 속도 제한을 건너뛴다. 제어보드가
  `MAX_RPM` 으로 다시 제한하고 AUTO 모드에서만 적용하긴 하지만, 그런 발행 패널은 만들지 않는다.

## 문제 해결

| 증상 | 확인할 것 |
|---|---|
| 접속이 안 됨 | 로버에서 `gcs.launch.py` 실행 중인지, 로버 IP, 방화벽에서 8765/tcp 허용 여부 |
| 토픽이 하나도 안 보임 | `bringup.launch.py` 실행 여부, `ros2 topic list` 로 로버에서 토픽 확인 |
| Diagnostics 가 STALE / ERROR | 제어보드 USB 연결과 micro_ros_agent 상태 (`ros2 topic hz /wheel_velocity`) |
| Teleop 을 눌러도 안 움직임 | 조종기 스위치가 AUTO 인지, Raw Messages 에 `/cmd_vel_out` 이 찍히는지 |
| Teleop 이 "publish 불가" 표시 | 브릿지 `capabilities` 에 `clientPublish` 가 있는지(기본 포함). 화이트리스트를 적용하는 브릿지 버전이면 `client_topic_whitelist` 에 `/gcs/teleop`, `/gcs/speed_step` 이 있는지 |
| 속도 표시가 비어 있음 / "+" "−" 가 반응 없음 | 로버에서 `gcs_teleop` 이 떠 있는지(`ros2 node list`). 구버전 `gcs.launch.py` 는 이 노드를 띄우지 않는다. 레이아웃도 새 파일로 다시 임포트 |
| Teleop 을 눌러도 `/cmd_vel_out` 이 안 찍힘 | 위와 같음. 예전 레이아웃은 `/cmd_vel` 로, 새 레이아웃은 `/gcs/teleop` 으로 발행한다 |

## 확장

- **게임패드**: Foxglove 확장 "Joystick panel"(`sensor_msgs/Joy` 발행) + 로버에
  `teleop_twist_joy`(`ros-jazzy-teleop-twist-joy`, `/joy` → `/cmd_vel`, 활성 버튼 지원)를
  추가하면 아날로그 조종이 된다. 화이트리스트를 적용하는 브릿지 버전이면
  `client_topic_whitelist` 에 `^/joy$` 를 추가한다.
- **카메라**: sensors 예제를 켠 뒤 Image 패널에 `/camera/camera/color/image_raw` 를 추가한다.
  무선 대역폭을 아끼려면 `compressed` 토픽을 쓴다.
- **목표점 지정**: navigation 예제를 켠 뒤 3D 패널의 "Publish pose" 로 `/goal_pose` 를
  발행한다. 화이트리스트를 적용하는 브릿지 버전이면 `client_topic_whitelist` 에
  `^/goal_pose$`, `^/initialpose$` 를 추가한다.
