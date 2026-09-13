# examples

템플릿에서 뺀 요소들의 참고 구현. **빌드·설치되지 않는다.** 필요한 것만 골라
아래 절차대로 패키지에 넣는다. 파라미터 값은 실기에서 튜닝된 값을 그대로 옮겼다.

| 디렉터리 | 추가되는 것 | 필요 패키지 (apt, ros-jazzy-*) |
|---|---|---|
| [odometry/](odometry/) | `/wheel_velocity` → `/odom` + `odom→base_link` TF | tf2, tf2-ros, nav-msgs |
| [description/](description/) | URDF → `base_link→camera_link`, 바퀴 정적 TF | robot-state-publisher |
| [sensors/](sensors/) | RealSense D455 → `/scan` (pointcloud_to_laserscan) | realsense2-camera, pointcloud-to-laserscan |
| [navigation/](navigation/) | Nav2 서버, slam_toolbox 매핑, map_server + AMCL 주행 | navigation2, nav2-bringup, slam-toolbox, nav2-amcl, nav2-map-server |

## 넣는 방법

**launch / yaml**: `*.launch.py` 는 `launch/` 로, `*.yaml` 은 `config/` 로 복사한 뒤
`colcon build`. 두 디렉터리는 통째로 설치되므로 CMake 수정이 없다.

**URDF**: `description/uel_rover.urdf` 를 `urdf/` 에 두고 `CMakeLists.txt` 의
`install(DIRECTORY launch config ...)` 에 `urdf` 를 추가한다.

**odom_publisher 노드**: `odometry/odom_publisher.cpp` 를 `src/` 로 복사하고
`CMakeLists.txt` 에 다음을 추가한다.

```cmake
find_package(nav_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)

add_executable(odom_publisher src/odom_publisher.cpp)
target_link_libraries(odom_publisher rclcpp::rclcpp
  ${std_msgs_TARGETS} ${nav_msgs_TARGETS} ${geometry_msgs_TARGETS}
  tf2::tf2 tf2_ros::tf2_ros)
# install(TARGETS ...) 목록에 odom_publisher 추가
```

`package.xml` 에도 `nav_msgs`, `tf2`, `tf2_ros` 를 `<depend>` 로 넣고,
`odometry/odometry.yaml` 을 `config/` 로 복사해 노드에 `parameters` 로 넘긴다
(bringup.launch.py 에 Node 를 추가하거나 별도 launch 를 만든다).

## 기존 시스템 전체를 재현하는 순서

```
bringup.launch.py          commander + monitor + micro_ros_agent   (템플릿)
+ odometry                 odom_publisher                          odom→base_link
+ description.launch.py    robot_state_publisher                   base_link→camera_link
+ sensors.launch.py        realsense + pointcloud_to_laserscan     /scan
+ nav2.launch.py           Nav2 서버 + lifecycle_manager           /cmd_vel
+ mapping.launch.py  또는  localization.launch.py map:=...          map→odom
```

TF 트리: `map → odom → base_link → camera_link, 바퀴`.

## 원본과 달라진 점

- Nav2 `controller_server` / `behavior_server` 의 `cmd_vel` 을 `/cmd_vel_out` 으로
  remap 하지 않는다. Nav2 는 `/cmd_vel` 로 발행하고 commander 가 전달한다.
  `nav2_params.yaml` 의 `enable_stamped_cmd_vel: false` 는 그대로 필요하다
  (commander 는 `Twist` 를 받는다).
- `bringup_common.launch.py` 하나에 있던 노드들을 관심사별 launch 로 나눴다.
- `localization.yaml` 의 `global_costmap` static 오버레이는 원본과 마찬가지로 어떤
  노드에도 로드되지 않는다(비활성). 저장맵 기반 전역 계획을 쓰려면 planner_server 에
  이 오버레이를 넘겨야 한다.
- 오도메트리 파라미터(`wheel_base 0.915`, `wheel_radius 0.0877`)는 펌웨어의
  `WHEEL_BASE 0.30`, `WHEEL_RADIUS 0.065` 와 다르다. 배포 전에 통일할 것.
