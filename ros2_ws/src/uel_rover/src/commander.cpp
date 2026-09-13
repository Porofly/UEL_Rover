// ─────────────────────────────────────────────────────────────────
//  Commander — 상위 제어기의 이동 명령을 제어보드(OpenRB-150)에 전달한다.
//
//  subscribe : cmd_vel     (geometry_msgs/Twist)  ← Nav2 / teleop / 사용자 노드
//  publish   : cmd_vel_out (geometry_msgs/Twist)  → OpenRB-150 (micro-ROS agent 경유)
//
//  하는 일
//    1. 차동구동에서 의미 없는 성분(linear.y/z, angular.x/y)을 버린다.
//    2. linear.x / angular.z 를 파라미터 상한으로 클램프한다.
//    3. command_timeout 동안 새 명령이 없으면 정지 명령을 한 번 보낸다.
//       제어보드에도 500 ms 워치독이 있으므로 이는 이중 안전장치이며,
//       정지 시점이 로그에 남는 것이 주된 효용이다.
//
//  하지 않는 일
//    - 명령 반복 발행. 상위 제어기가 주기적으로 cmd_vel 을 보내야 한다.
//      제어보드 워치독(500 ms) 안에 다음 명령이 와야 계속 움직인다.
//    - 드라이브 모드 게이팅. RC / AUTO / STOP 판정은 제어보드가 단독으로 한다.
//      AUTO 가 아니면 cmd_vel_out 은 제어보드에서 무시된다.
// ─────────────────────────────────────────────────────────────────
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>

using namespace std::chrono_literals;

class Commander : public rclcpp::Node
{
public:
  Commander()
  : Node("commander")
  {
    // 상한값은 이전 Nav2 튜닝값(max_vel_x 0.26, max_vel_theta 1.0)과 같다.
    // 0 이하로 주면 해당 축의 클램프를 끈다.
    max_linear_  = declare_parameter("max_linear_velocity", 0.26);   // [m/s]
    max_angular_ = declare_parameter("max_angular_velocity", 1.0);   // [rad/s]
    // 이 시간 동안 cmd_vel 이 없으면 정지 명령을 보낸다. 0 이하면 비활성.
    timeout_     = declare_parameter("command_timeout", 0.5);        // [s]

    pub_ = create_publisher<geometry_msgs::msg::Twist>("cmd_vel_out", 10);
    sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "cmd_vel", 10, std::bind(&Commander::onCmdVel, this, std::placeholders::_1));

    last_cmd_time_ = now();
    if (timeout_ > 0.0) {
      timer_ = create_wall_timer(50ms, std::bind(&Commander::checkTimeout, this));
    }

    RCLCPP_INFO(get_logger(),
      "commander 시작: cmd_vel → cmd_vel_out, 상한 %.2f m/s / %.2f rad/s, 타임아웃 %.2f s",
      max_linear_, max_angular_, timeout_);
  }

private:
  void onCmdVel(const geometry_msgs::msg::Twist::ConstSharedPtr in)
  {
    geometry_msgs::msg::Twist out;   // 명시하지 않은 성분은 0
    if (!std::isfinite(in->linear.x) || !std::isfinite(in->angular.z)) {
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
        "cmd_vel 에 NaN/Inf 가 있어 정지 명령으로 대체");
    } else {
      out.linear.x  = limit(in->linear.x,  max_linear_);
      out.angular.z = limit(in->angular.z, max_angular_);
    }
    pub_->publish(out);
    last_cmd_time_ = now();
    moving_ = (out.linear.x != 0.0 || out.angular.z != 0.0);
  }

  void checkTimeout()
  {
    if (!moving_) {
      return;   // 이미 정지 상태면 보낼 것이 없다
    }
    if ((now() - last_cmd_time_).seconds() < timeout_) {
      return;
    }
    pub_->publish(geometry_msgs::msg::Twist{});
    moving_ = false;
    RCLCPP_WARN(get_logger(), "%.0f ms 동안 cmd_vel 없음 → 정지 명령 발행",
      timeout_ * 1000.0);
  }

  static double limit(double v, double max)
  {
    return max > 0.0 ? std::clamp(v, -max, max) : v;
  }

  double max_linear_{0.0};
  double max_angular_{0.0};
  double timeout_{0.0};
  bool moving_{false};
  rclcpp::Time last_cmd_time_;

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Commander>());
  rclcpp::shutdown();
  return 0;
}
