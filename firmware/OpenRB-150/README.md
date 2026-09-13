# OpenRB-150 펌웨어

UEL Rover 제어보드(ROBOTIS OpenRB-150) 펌웨어. RC 수신기 입력과 Jetson의 자율주행
명령을 받아 다이나믹셀 바퀴 모터 2개를 구동하고, 바퀴 속도와 드라이브 모드를
micro-ROS로 Jetson에 발행한다.

## 구성

```
OpenRB-150/
├── README.md
└── OpenRB-150.ino   # 스케치
```

## 하드웨어

| 항목 | 내용 |
|---|---|
| 보드 | ROBOTIS OpenRB-150 |
| 모터 | 다이나믹셀 2개, TTL 포트 직결, 1 Mbps, Protocol 2.0 (ID 1 = 좌, 2 = 우) |
| RC | FlySky 송신기 + FS-iA10B 수신기, 채널별 개별 PWM 출력 |
| 상위 제어기 | Jetson Orin, USB 시리얼 (micro-ROS XRCE-DDS, 115200 bps) |

### RC 배선

| 수신기 채널 | 역할 | OpenRB-150 핀 |
|---|---|---|
| CH1 | 조향 | D6 |
| CH2 | 스로틀 | D7 |
| CH5 | 모드 스위치 | D8 |
| GND | 공통 접지 | GND |

수신기 VCC는 별도 전원 또는 OpenRB-150 5V 핀. PWM 펄스폭은 1000 / 1500 / 2000 µs
(최소 / 중립 / 최대).

## 빌드 · 업로드

1. Arduino IDE Boards Manager에서 **OpenRB-150** 보드 패키지 설치.
2. 라이브러리 설치
   - **DynamixelShield** (ROBOTIS): Library Manager에서 설치.
   - **micro_ros_arduino**: ROS 2 **Jazzy** 릴리스 zip을 내려받아
     `Sketch → Include Library → Add .ZIP Library`로 수동 설치.
     https://github.com/micro-ROS/micro_ros_arduino/releases
3. `OpenRB-150.ino`를 열고 보드를 OpenRB-150으로 선택한 뒤 업로드.
4. 업로드 후 **시리얼 모니터를 열지 않는다.** USB 시리얼이 micro-ROS 트랜스포트라
   Serial 출력이 통신을 오염시킨다. 같은 이유로 펌웨어 안에서도 `Serial.print`를
   쓰지 않는다.

## ROS 2 인터페이스

노드 이름 `openrb150`. Jetson에서 micro-ROS Agent가 떠 있어야 통신한다.

```
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyACM0 -b 115200
```

| 토픽 | 타입 | 방향 | 비고 |
|---|---|---|---|
| `/cmd_vel_out` | `geometry_msgs/Twist` | 구독 | Nav2 controller_server 출력 (AUTO 모드에서만 적용) |
| `/wheel_velocity` | `std_msgs/Float32MultiArray` `[vL_rpm, vR_rpm]` | 발행 50 Hz | Jetson `odom_publisher` 입력 |
| `/drive_mode` | `std_msgs/String` `"RC"` / `"AUTO"` / `"STOP"` | 발행 50 Hz | 관측용 |

Agent가 재시작되어도 보드 리셋 없이 자동으로 다시 붙는다
(WAITING → AVAILABLE → CONNECTED → DISCONNECTED 상태 머신, ping 기반 생존 확인).

## 드라이브 모드

CH5 펄스폭으로 판정한다. 이 판정이 로버의 유일한 안전 게이트이며 Jetson 쪽에는
중계 노드가 없다.

| CH5 | 모드 | 동작 |
|---|---|---|
| ≥ 1700 µs | RC | 조종기 입력으로 직접 구동. `/cmd_vel_out` 무시. |
| ≤ 1300 µs | AUTO | `/cmd_vel_out` 적용. Agent 연결됨 · 재연결 후 새 명령 수신 · 500 ms 워치독 이내, 세 조건을 모두 만족할 때만 구동하고 아니면 정지. |
| 그 사이 | STOP | 즉시 정지. |

RC / STOP 제어는 micro-ROS 연결 상태와 무관하게 매 루프 먼저 실행된다(데드맨 스위치).

## 배포 전 확인할 상수

모두 `OpenRB-150.ino` 상단 `#define`에 있다.

| 상수 | 현재 값 | 확인 사항 |
|---|---|---|
| `DXL_LEFT_ID` / `DXL_RIGHT_ID` | 1 / 2 | 실제 장착된 모터 ID |
| `RIGHT_MOTOR_INVERTED` | `true` | 우측 모터 장착 방향 |
| `MAX_RPM` | 50 | 모터 모델 한계. Jetson odom 가드 `max_wheel_rpm`(100)은 이 값의 2배로 유지 |
| `WHEEL_BASE` | 0.30 m | ROS 측 `rover_params.yaml`은 실측 보정값 0.915 m 사용 |
| `WHEEL_RADIUS` | 0.065 m | ROS 측 `rover_params.yaml`은 실측값 0.0877 m 사용 |

`WHEEL_BASE` / `WHEEL_RADIUS`는 AUTO 모드에서 `/cmd_vel_out`(Twist)을 바퀴 RPM으로
바꿀 때만 쓰인다. ROS 측 오도메트리가 쓰는 값과 다르면 Nav2가 명령한 속도와 실제
속도가 어긋나므로, 두 곳은 반드시 같은 값으로 맞춘다.
