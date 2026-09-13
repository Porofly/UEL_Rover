// ─────────────────────────────────────────────────────────────────
//  Monitor — 제어보드(OpenRB-150)가 보내는 상태를 감시한다.
//
//  subscribe : wheel_velocity (std_msgs/Float32MultiArray [vL_rpm, vR_rpm], 50 Hz)
//              drive_mode     (std_msgs/String "RC" | "AUTO" | "STOP",       50 Hz)
//  publish   : /diagnostics   (diagnostic_msgs/DiagnosticArray)
//
//  하는 일
//    1. 두 토픽의 마지막 수신 시각으로 제어보드 링크 상태를 판정한다.
//       link_timeout 동안 아무것도 오지 않으면 단절로 본다.
//    2. 링크 연결/단절과 드라이브 모드 변화를 로그로 남긴다.
//    3. 물리적으로 불가능한 바퀴 속도(NaN, |rpm| > max_wheel_rpm)를 세고 경고한다.
//    4. 위 내용을 /diagnostics 로 주기 발행한다.
//       rqt_robot_monitor, diagnostic_aggregator 같은 표준 도구로 볼 수 있다.
//
//  하지 않는 일
//    - 오도메트리 계산. 필요하면 examples/odometry 를 참고해 추가한다.
// ─────────────────────────────────────────────────────────────────
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

#include <diagnostic_msgs/msg/diagnostic_array.hpp>
#include <diagnostic_msgs/msg/diagnostic_status.hpp>
#include <diagnostic_msgs/msg/key_value.hpp>
#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>
#include <std_msgs/msg/string.hpp>

using namespace std::chrono_literals;
using diagnostic_msgs::msg::DiagnosticStatus;

class Monitor : public rclcpp::Node
{
public:
  Monitor()
  : Node("monitor")
  {
    // 제어보드는 50 Hz(20 ms)로 발행한다. 0.5 s 는 25 주기 연속 누락에 해당한다.
    link_timeout_  = declare_parameter("link_timeout", 0.5);        // [s]
    // 펌웨어 MAX_RPM(50)의 2배. 넘으면 측정값이 아니라 통신 오류로 본다.
    max_wheel_rpm_ = declare_parameter("max_wheel_rpm", 100.0);     // [rpm]
    diag_period_   = declare_parameter("diagnostics_period", 1.0);  // [s]

    wheel_sub_ = create_subscription<std_msgs::msg::Float32MultiArray>(
      "wheel_velocity", 10,
      std::bind(&Monitor::onWheelVelocity, this, std::placeholders::_1));
    mode_sub_ = create_subscription<std_msgs::msg::String>(
      "drive_mode", 10,
      std::bind(&Monitor::onDriveMode, this, std::placeholders::_1));
    diag_pub_ = create_publisher<diagnostic_msgs::msg::DiagnosticArray>("/diagnostics", 10);

    last_wheel_time_ = now();
    last_mode_time_  = now();
    last_diag_time_  = now();

    link_timer_ = create_wall_timer(100ms, std::bind(&Monitor::checkLink, this));
    diag_timer_ = create_wall_timer(
      std::chrono::milliseconds(static_cast<int64_t>(std::max(diag_period_, 0.1) * 1000.0)),
      std::bind(&Monitor::publishDiagnostics, this));

    RCLCPP_INFO(get_logger(),
      "monitor 시작: 제어보드 응답 대기 중 (link_timeout %.2f s)", link_timeout_);
  }

private:
  // ── 수신 콜백 ──────────────────────────────────────────────
  void onWheelVelocity(const std_msgs::msg::Float32MultiArray::ConstSharedPtr msg)
  {
    last_wheel_time_ = now();
    has_wheel_ = true;
    ++wheel_count_;

    if (msg->data.size() < 2) {
      ++bad_samples_;
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
        "wheel_velocity 길이 %zu (2 기대)", msg->data.size());
      return;
    }
    const float l = msg->data[0];
    const float r = msg->data[1];
    if (!std::isfinite(l) || !std::isfinite(r) ||
        std::fabs(l) > max_wheel_rpm_ || std::fabs(r) > max_wheel_rpm_) {
      ++bad_samples_;
      RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
        "비물리적 바퀴 속도: [%.1f, %.1f] rpm (허용 ±%.0f)", l, r, max_wheel_rpm_);
      return;
    }
    left_rpm_  = l;
    right_rpm_ = r;
  }

  void onDriveMode(const std_msgs::msg::String::ConstSharedPtr msg)
  {
    last_mode_time_ = now();
    has_mode_ = true;
    if (msg->data != drive_mode_) {
      RCLCPP_INFO(get_logger(), "drive_mode: %s → %s",
        drive_mode_.empty() ? "-" : drive_mode_.c_str(), msg->data.c_str());
      drive_mode_ = msg->data;
    }
  }

  // ── 링크 판정 ──────────────────────────────────────────────
  // 마지막 수신 후 경과 시간 [s]. 한 번도 못 받았으면 음수.
  double age(const rclcpp::Time & t, bool received) const
  {
    return received ? (now() - t).seconds() : -1.0;
  }

  // 두 토픽 중 더 최근 수신을 기준으로 한 경과 시간. 수신 이력이 없으면 음수.
  double linkAge() const
  {
    const double w = age(last_wheel_time_, has_wheel_);
    const double m = age(last_mode_time_, has_mode_);
    if (w < 0.0) {return m;}
    if (m < 0.0) {return w;}
    return std::min(w, m);
  }

  bool linkUp() const
  {
    const double a = linkAge();
    return a >= 0.0 && a < link_timeout_;
  }

  void checkLink()
  {
    const bool up = linkUp();
    if (up && !link_up_) {
      RCLCPP_INFO(get_logger(), "제어보드 링크 연결됨");
    } else if (!up && link_up_) {
      RCLCPP_WARN(get_logger(), "제어보드 링크 끊김 (%.2f s 동안 수신 없음)", linkAge());
    }
    link_up_ = up;
  }

  // ── /diagnostics ──────────────────────────────────────────
  static void add(DiagnosticStatus & s, const std::string & key, const std::string & value)
  {
    diagnostic_msgs::msg::KeyValue kv;
    kv.key = key;
    kv.value = value;
    s.values.push_back(kv);
  }

  static std::string fmt(double v, const char * f = "%.2f")
  {
    char buf[32];
    std::snprintf(buf, sizeof(buf), f, v);
    return buf;
  }

  void publishDiagnostics()
  {
    const auto t = now();
    const double dt = (t - last_diag_time_).seconds();
    const double rate = dt > 0.0 ? static_cast<double>(wheel_count_) / dt : 0.0;
    wheel_count_ = 0;
    last_diag_time_ = t;

    DiagnosticStatus s;
    s.name = "uel_rover/control_board";
    s.hardware_id = "OpenRB-150";

    const double a = linkAge();
    if (a < 0.0) {
      s.level = DiagnosticStatus::STALE;
      s.message = "제어보드 응답 없음 (수신 이력 없음)";
    } else if (!linkUp()) {
      s.level = DiagnosticStatus::ERROR;
      s.message = "제어보드 링크 끊김";
    } else if (bad_samples_ != last_bad_samples_) {
      s.level = DiagnosticStatus::WARN;
      s.message = "비물리적 바퀴 속도 수신";
    } else {
      s.level = DiagnosticStatus::OK;
      s.message = "정상 / 모드 " + (drive_mode_.empty() ? std::string("-") : drive_mode_);
    }
    last_bad_samples_ = bad_samples_;

    add(s, "drive_mode", drive_mode_.empty() ? "-" : drive_mode_);
    add(s, "wheel_left_rpm", fmt(left_rpm_, "%.1f"));
    add(s, "wheel_right_rpm", fmt(right_rpm_, "%.1f"));
    add(s, "wheel_velocity_rate_hz", fmt(rate, "%.1f"));
    add(s, "wheel_velocity_age_s", fmt(age(last_wheel_time_, has_wheel_)));
    add(s, "drive_mode_age_s", fmt(age(last_mode_time_, has_mode_)));
    add(s, "bad_wheel_samples", std::to_string(bad_samples_));

    diagnostic_msgs::msg::DiagnosticArray arr;
    arr.header.stamp = t;
    arr.status.push_back(s);
    diag_pub_->publish(arr);
  }

  // ── 파라미터 ───────────────────────────────────────────────
  double link_timeout_{0.0};
  double max_wheel_rpm_{0.0};
  double diag_period_{0.0};

  // ── 상태 ───────────────────────────────────────────────────
  bool has_wheel_{false};
  bool has_mode_{false};
  bool link_up_{false};
  rclcpp::Time last_wheel_time_;
  rclcpp::Time last_mode_time_;
  rclcpp::Time last_diag_time_;
  float left_rpm_{0.0f};
  float right_rpm_{0.0f};
  std::string drive_mode_;
  uint64_t wheel_count_{0};
  uint64_t bad_samples_{0};
  uint64_t last_bad_samples_{0};

  rclcpp::Subscription<std_msgs::msg::Float32MultiArray>::SharedPtr wheel_sub_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr mode_sub_;
  rclcpp::Publisher<diagnostic_msgs::msg::DiagnosticArray>::SharedPtr diag_pub_;
  rclcpp::TimerBase::SharedPtr link_timer_;
  rclcpp::TimerBase::SharedPtr diag_timer_;
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<Monitor>());
  rclcpp::shutdown();
  return 0;
}
