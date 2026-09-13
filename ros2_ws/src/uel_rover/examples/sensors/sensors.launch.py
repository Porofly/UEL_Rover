import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


# RealSense D455 → /camera/camera/depth/color/points → pointcloud_to_laserscan → /scan
# 사용: pointcloud_to_laserscan.yaml 을 config/ 로, 이 파일을 launch/ 로 복사.
#       apt: ros-jazzy-realsense2-camera ros-jazzy-pointcloud-to-laserscan
#       TF 상 camera_link 가 있어야 하므로 description 예제와 함께 쓴다.
def generate_launch_description():
    pkg = get_package_share_directory('uel_rover')
    p2l_params = os.path.join(pkg, 'config', 'pointcloud_to_laserscan.yaml')

    return LaunchDescription([
        # namespace='camera' + name='camera' → 토픽이 /camera/camera/... 로 나온다.
        # 아래 pointcloud_to_laserscan 의 cloud_in remap 과 맞춰야 /scan 이 생성된다.
        Node(package='realsense2_camera', executable='realsense2_camera_node',
             namespace='camera', name='camera', output='screen',
             parameters=[{
                 'initial_reset': True,
                 'enable_depth': True,
                 'enable_color': True,
                 'enable_infra1': False,
                 'enable_infra2': False,
                 # realsense-ros 의 __neon_ 파라미터 버그 우회
                 # (IntelRealSense/realsense-ros #3397). 버전에 따라 불필요할 수 있다.
                 'pointcloud__neon_.enable': True,
                 'enable_sync': True,
                 'depth_module.depth_profile': '640x480x30',
                 'rgb_camera.color_profile': '640x480x30',
                 'enable_gyro': False,
                 'enable_accel': False,
             }]),

        Node(package='pointcloud_to_laserscan',
             executable='pointcloud_to_laserscan_node',
             name='pointcloud_to_laserscan', output='screen',
             parameters=[p2l_params],
             remappings=[('cloud_in', '/camera/camera/depth/color/points'),
                         ('scan', '/scan')]),
    ])
