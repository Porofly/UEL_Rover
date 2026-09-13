import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


# Nav2 핵심 서버 + lifecycle_manager.
# SLAM 또는 AMCL(map→odom TF)은 별도 launch(mapping / localization)로 띄운다.
# 사용: nav2_params.yaml 을 config/ 로, 이 파일을 launch/ 로 복사.
#       apt: ros-jazzy-navigation2 ros-jazzy-nav2-bringup
#
# 기존 구성과의 차이: controller_server / behavior_server 의 cmd_vel 을
# /cmd_vel_out 으로 remap 하지 않는다. Nav2 는 /cmd_vel 로 발행하고
# commander 가 이를 /cmd_vel_out 으로 전달한다.
def generate_launch_description():
    pkg = get_package_share_directory('uel_rover')
    nav2_params = os.path.join(pkg, 'config', 'nav2_params.yaml')
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')

    lifecycle_nodes = [
        'controller_server',
        'planner_server',
        'behavior_server',
        'bt_navigator',
        'waypoint_follower',
    ]

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),

        Node(package='nav2_controller', executable='controller_server',
             name='controller_server', output='screen', parameters=[nav2_params]),
        Node(package='nav2_planner', executable='planner_server',
             name='planner_server', output='screen', parameters=[nav2_params]),
        Node(package='nav2_behaviors', executable='behavior_server',
             name='behavior_server', output='screen', parameters=[nav2_params]),
        Node(package='nav2_bt_navigator', executable='bt_navigator',
             name='bt_navigator', output='screen', parameters=[nav2_params]),
        Node(package='nav2_waypoint_follower', executable='waypoint_follower',
             name='waypoint_follower', output='screen', parameters=[nav2_params]),

        # bond_timeout: 재부팅 직후 부하로 activate 가 느리면 기본 4 s 안에 bond 를
        #   못 맺어 bringup 이 abort 된다. 여유를 크게 준다.
        Node(package='nav2_lifecycle_manager', executable='lifecycle_manager',
             name='lifecycle_manager_navigation', output='screen',
             parameters=[{'autostart': True, 'node_names': lifecycle_nodes,
                          'use_sim_time': use_sim_time,
                          'bond_timeout': 30.0,
                          'attempt_respawn_reconnection': True,
                          'bond_respawn_max_duration': 60.0}]),
    ])
