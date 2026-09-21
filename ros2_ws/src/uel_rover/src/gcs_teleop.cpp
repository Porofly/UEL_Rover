// ─────────────────────────────────────────────────────────────────
//  GcsTeleop — GCS(Foxglove) Teleop 의 방향 입력에 +/- 로 고른 속도를 곱해 cmd_vel 로 낸다.
//
//  subscribe : gcs/teleop      (geometry_msgs/Twist)  ← Teleop 패널. 방향만 -1..1
//              gcs/speed_step  (std_msgs/Int8)        ← "+" / "−" Publish 패널. 부호만 본다
//  publish   : cmd_vel         (geometry_msgs/Twist)  → commander
//              gcs/speed       (std_msgs/String)      → GCS 표시용 현재 속도 (변경 시 + 1 Hz)
//
//  속도는 1..speed_levels 단계이고, 단계 / speed_levels 비율로 선속도·각속도가 함께 변한다.
//
//  하지 않는 일
//    - 명령 반복 발행. 입력이 올 때만 cmd_vel 을 낸다. 버튼을 떼면 발행이 멈추고
//      commander 타임아웃과 제어보드 워치독이 그대로 로버를 세운다.
//    - 상한 보장. 최종 클램프는 여전히 commander 가 한다. max_*_speed 를 commander 상한보다
//      크게 주면 표시되는 속도보다 실제 명령이 작아진다.
// ─────────────────────────────────────────────────────────────────
#include <chrono>
#include <cstdio>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>

#include <geometry_msgs/msg/twist.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int8.hpp>
#include <std_msgs/msg/string.hpp>

#include "teleop_speed.hpp"

using namespace std::chrono_literals;

class GcsTeleop : public rclcpp::Node
{
public:
  GcsTeleop()
  : Node("gcs_teleop")
  {
    max_linear_  = declare_parameter("max_linear_speed", 0.26);    // [m/s]   최고 단계의 선속도
    max_angular_ = declare_parameter("max_angular_speed", 1.0);    // [rad/s] 최고 단계의 각속도
    levels_      = declare_parameter("speed_levels", 10);
    const int initial_level = declare_parameter("initial_level", 6);

    // commander 와 달리 값을 고쳐 쓰지 않고 바로 종료한다(의도된 fail-fast). 조종 입력을 만드는
    // 노드라, 잘못된 설정으로 추측한 속도를 내느니 뜨지 않는 편이 안전하다.
    if (levels_ < 1 || max_linear_ <= 0.0 || max_angular_ <= 0.0) {
      throw std::invalid_argument(
              "gcs_teleop: speed_levels 는 1 이상, max_linear_speed / max_angular_speed 는 0 보다 커야 한다");
    }
    level_ = uel_rover::steppedLevel(initial_level, 0, levels_);

    cmd_pub_   = create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);
    speed_pub_ = create_publisher<std_msgs::msg::String>("gcs/speed", 10);
    teleop_sub_ = create_subscription<geometry_msgs::msg::Twist>(
      "gcs/teleop", 10, std::bind(&GcsTeleop::onTeleop, this, std::placeholders::_1));
    step_sub_ = create_subscription<std_msgs::msg::Int8>(
      "gcs/speed_step", 10, std::bind(&GcsTeleop::onStep, this, std::placeholders::_1));
    // GCS 가 나중에 접속해도 현재 값이 보이도록 주기적으로도 낸다.
    timer_ = create_wall_timer(1s, std::bind(&GcsTeleop::publishSpeed, this));

    RCLCPP_INFO(get_logger(), "gcs_teleop 시작: %s", describeSpeed().c_str());
  }

private:
  void onTeleop(const geometry_msgs::msg::Twist::ConstSharedPtr in)
  {
    geometry_msgs::msg::Twist out;   // 명시하지 않은 성분은 0
    out.linear.x  = uel_rover::scaledCommand(in->linear.x, linearSpeed());
    out.angular.z = uel_rover::scaledCommand(in->angular.z, angularSpeed());
    cmd_pub_->publish(out);
  }

  void onStep(const std_msgs::msg::Int8::ConstSharedPtr step)
  {
    const int next = uel_rover::steppedLevel(level_, step->data, levels_);
    if (next != level_) {
      level_ = next;
      RCLCPP_INFO(get_logger(), "속도 변경: %s", describeSpeed().c_str());
    }
    publishSpeed();   // 끝 단계에서 눌러도 GCS 에 즉시 응답이 보이게 한다
  }

  void publishSpeed()
  {
    std_msgs::msg::String msg;
    msg.data = describeSpeed();
    speed_pub_->publish(msg);
  }

  double linearSpeed() const {return uel_rover::levelSpeed(level_, levels_, max_linear_);}
  double angularSpeed() const {return uel_rover::levelSpeed(level_, levels_, max_angular_);}

  std::string describeSpeed() const
  {
    char text[64];
    std::snprintf(text, sizeof(text), "%d/%d  %.2f m/s  %.2f rad/s",
      level_, levels_, linearSpeed(), angularSpeed());
    return text;
  }

  double max_linear_{0.0};
  double max_angular_{0.0};
  int levels_{1};
  int level_{1};

  rclcpp::Publisher<geometry_msgs::msg::Twist>::SharedPtr cmd_pub_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr speed_pub_;
  rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr teleop_sub_;
  rclcpp::Subscription<std_msgs::msg::Int8>::SharedPtr step_sub_;
  rclcpp::TimerBase::SharedPtr timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<GcsTeleop>());
  rclcpp::shutdown();
  return 0;
}
