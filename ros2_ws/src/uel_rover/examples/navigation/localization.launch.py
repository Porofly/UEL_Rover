import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


# 주행 모드: map_server(저장맵) + AMCL → map→odom TF
# 입력: /scan (sensors 예제), odom→base_link TF (odometry 예제)
# 사용: localization.yaml 을 config/ 로, 이 파일을 launch/ 로 복사.
#       apt: ros-jazzy-nav2-map-server ros-jazzy-nav2-amcl
#       ros2 launch uel_rover localization.launch.py map:=/path/to/map.yaml
def generate_launch_description():
    pkg = get_package_share_directory('uel_rover')
    loc_params = os.path.join(pkg, 'config', 'localization.yaml')

    map_yaml     = LaunchConfiguration('map')
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')

    return LaunchDescription([
        DeclareLaunchArgument('map', description='저장맵 .yaml 경로'),
        DeclareLaunchArgument('use_sim_time', default_value='false'),

        Node(package='nav2_map_server', executable='map_server',
             name='map_server', output='screen',
             parameters=[loc_params, {'yaml_filename': map_yaml,
                                      'use_sim_time': use_sim_time}]),

        Node(package='nav2_amcl', executable='amcl',
             name='amcl', output='screen',
             parameters=[loc_params, {'use_sim_time': use_sim_time}]),

        Node(package='nav2_lifecycle_manager', executable='lifecycle_manager',
             name='lifecycle_manager_localization', output='screen',
             parameters=[{'autostart': True, 'node_names': ['map_server', 'amcl'],
                          'use_sim_time': use_sim_time}]),
    ])
