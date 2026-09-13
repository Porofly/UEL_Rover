import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


# URDF → 정적 TF (base_link → camera_link, left/right_wheel_link)
# 사용: urdf/uel_rover.urdf 를 패키지에 넣고 CMakeLists.txt 의
#       install(DIRECTORY ...) 목록에 urdf 를 추가한 뒤 이 파일을 launch/ 로 복사.
def generate_launch_description():
    pkg = get_package_share_directory('uel_rover')
    with open(os.path.join(pkg, 'urdf', 'uel_rover.urdf'), 'r') as f:
        robot_desc = f.read()

    return LaunchDescription([
        Node(package='robot_state_publisher', executable='robot_state_publisher',
             name='robot_state_publisher', output='screen',
             parameters=[{'robot_description': robot_desc}]),
    ])
