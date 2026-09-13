import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


# 로버 제어에 필요한 최소 구성:
#   micro_ros_agent  OpenRB-150 시리얼 ↔ ROS 2 (start_agent:=false 로 끌 수 있음)
#   commander        cmd_vel → cmd_vel_out
#   monitor          wheel_velocity / drive_mode 감시 → /diagnostics
#
# 오도메트리, URDF, 센서, Nav2 등은 examples/ 를 참고해 필요할 때 추가한다.
def generate_launch_description():
    pkg = get_package_share_directory('uel_rover')
    default_params = os.path.join(pkg, 'config', 'rover.yaml')

    serial_port = LaunchConfiguration('serial_port')
    baudrate    = LaunchConfiguration('baudrate')
    start_agent = LaunchConfiguration('start_agent')
    params_file = LaunchConfiguration('params_file')

    return LaunchDescription([
        DeclareLaunchArgument('serial_port', default_value='/dev/ttyACM0',
                              description='OpenRB-150 USB 시리얼 장치'),
        DeclareLaunchArgument('baudrate', default_value='115200',
                              description='micro-ROS 시리얼 보레이트'),
        DeclareLaunchArgument('start_agent', default_value='true',
                              description='micro_ros_agent 를 이 launch 에서 띄울지 여부'),
        DeclareLaunchArgument('params_file', default_value=default_params,
                              description='commander / monitor 파라미터 yaml'),

        # ── micro-ROS agent ──────────────────────────────────────
        # name / parameters 를 주면 launch 가 --ros-args 를 붙이는데, agent 는
        # 자체 CLI 파서라 이를 거부한다. 그래서 arguments 만 넘긴다.
        # 환경변수 두 개는 기존 실행 스크립트(run.sh, systemd)에서 그대로 가져왔다.
        Node(package='micro_ros_agent', executable='micro_ros_agent',
             output='screen',
             arguments=['serial', '--dev', serial_port, '-b', baudrate],
             additional_env={'RMW_IMPLEMENTATION': 'rmw_fastrtps_cpp',
                             'MICROROS_DISABLE_SHM': '1'},
             condition=IfCondition(start_agent)),

        # ── Commander: 이동 명령 → 제어보드 ─────────────────────
        Node(package='uel_rover', executable='commander',
             name='commander', output='screen',
             parameters=[params_file]),

        # ── Monitor: 제어보드 상태 감시 ─────────────────────────
        Node(package='uel_rover', executable='monitor',
             name='monitor', output='screen',
             parameters=[params_file]),
    ])
