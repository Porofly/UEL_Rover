import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


# 매핑 모드: slam_toolbox (온라인 async) → /map + map→odom TF
# 입력: /scan (sensors 예제), odom→base_link TF (odometry 예제)
# 사용: slam_toolbox.yaml 을 config/ 로, 이 파일을 launch/ 로 복사.
#       apt: ros-jazzy-slam-toolbox
def generate_launch_description():
    pkg = get_package_share_directory('uel_rover')
    slam_params = os.path.join(pkg, 'config', 'slam_toolbox.yaml')
    use_sim_time = LaunchConfiguration('use_sim_time', default='false')

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),

        Node(package='slam_toolbox', executable='async_slam_toolbox_node',
             name='slam_toolbox', output='screen',
             parameters=[slam_params, {'use_sim_time': use_sim_time}]),

        # slam_toolbox 는 lifecycle 노드라 configure/activate 를 걸어줄
        # lifecycle_manager 가 없으면 unconfigured 상태로 멈춰 있다.
        # bond_timeout=0.0: Jetson CPU 과부하 시 heartbeat 가 밀려 멀쩡한 slam 을
        #   DOWN 판정 → respawn 을 반복(flapping)하는 것을 막는다.
        Node(package='nav2_lifecycle_manager', executable='lifecycle_manager',
             name='lifecycle_manager_slam', output='screen',
             parameters=[{'autostart': True, 'node_names': ['slam_toolbox'],
                          'use_sim_time': use_sim_time,
                          'bond_timeout': 0.0}]),
    ])
