cmake_minimum_required(VERSION 3.8)
project(uel_rover)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(diagnostic_msgs REQUIRED)

# ── 노드 ────────────────────────────────────────────────────────
add_executable(commander src/commander.cpp)
target_link_libraries(commander rclcpp::rclcpp ${geometry_msgs_TARGETS})

add_executable(monitor src/monitor.cpp)
target_link_libraries(monitor rclcpp::rclcpp ${std_msgs_TARGETS} ${diagnostic_msgs_TARGETS})

# GCS Teleop 속도 조절 (gcs.launch.py 에서만 띄운다)
add_executable(gcs_teleop src/gcs_teleop.cpp)
target_link_libraries(gcs_teleop rclcpp::rclcpp ${geometry_msgs_TARGETS} ${std_msgs_TARGETS})

# 노드를 추가할 때는 위 패턴대로 add_executable / target_link_libraries 를
# 쓰고 아래 install(TARGETS ...) 목록에 넣는다.

# ── 설치 ────────────────────────────────────────────────────────
install(TARGETS commander monitor gcs_teleop
  DESTINATION lib/${PROJECT_NAME})

# launch/ 와 config/ 는 디렉터리째 설치되므로 파일을 추가해도 여기는 손댈 필요 없다.
# urdf/ 등 새 디렉터리를 만들면 이 목록에 추가한다. examples/ 는 설치하지 않는다.
install(DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME})

# ── 테스트 (colcon test) ────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_teleop_speed test/test_teleop_speed.cpp)
  target_include_directories(test_teleop_speed PRIVATE src)
endif()

ament_package()
