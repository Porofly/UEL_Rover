import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


# GCS(Foxglove) 연결용 foxglove_bridge.
# GCS PC 의 Foxglove 앱이 ws://<로버 IP>:<port> 로 접속해 토픽을 보고 /cmd_vel 을 발행한다.
# bringup.launch.py 와는 별도로 띄운다:
#   ros2 launch uel_rover gcs.launch.py
# apt: ros-jazzy-foxglove-bridge
# GCS 쪽 설정(앱 설치, 접속, 레이아웃)은 저장소 루트의 gcs/README.md 참고.
def generate_launch_description():
    pkg = get_package_share_directory('uel_rover')
    default_params = os.path.join(pkg, 'config', 'foxglove_bridge.yaml')

    port        = LaunchConfiguration('port')
    address     = LaunchConfiguration('address')
    params_file = LaunchConfiguration('params_file')

    return LaunchDescription([
        DeclareLaunchArgument('port', default_value='8765',
                              description='WebSocket 포트'),
        DeclareLaunchArgument('address', default_value='0.0.0.0',
                              description='바인드 주소. 127.0.0.1 이면 로버 안에서만 접속 가능'),
        DeclareLaunchArgument('params_file', default_value=default_params,
                              description='foxglove_bridge 파라미터 yaml'),

        # launch 인자는 문자열이라 port 는 int 로 명시해야 파라미터 타입 오류가 없다.
        Node(package='foxglove_bridge', executable='foxglove_bridge',
             name='foxglove_bridge', output='screen',
             parameters=[params_file,
                         {'port': ParameterValue(port, value_type=int),
                          'address': address}]),
    ])
