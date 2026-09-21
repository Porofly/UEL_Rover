// GCS Teleop 속도 단계 계산. ROS 에 의존하지 않는 순수 함수라 test/test_teleop_speed.cpp 가 직접 검증한다.
#pragma once

#include <algorithm>
#include <cmath>

namespace uel_rover
{

// direction 은 부호만 본다: + 면 한 단계 위, - 면 한 단계 아래. 결과는 항상 1..levels.
// 단계를 정수로 세는 이유는 0.1 씩 더하는 실수 누적 오차를 피하기 위해서다.
inline int steppedLevel(int level, int direction, int levels)
{
  const int step = (direction > 0) - (direction < 0);
  return std::clamp(level + step, 1, std::max(levels, 1));
}

// 단계에 비례하는 속도. level == levels 일 때 max_speed.
inline double levelSpeed(int level, int levels, double max_speed)
{
  return max_speed * level / std::max(levels, 1);
}

// Teleop 패널의 방향 입력(-1..1)에 고른 속도를 곱한다. 레이아웃에 더 큰 값이 들어 있어도
// 고른 속도를 넘지 않고, NaN/Inf 는 정지(0)로 바꾼다.
inline double scaledCommand(double direction, double speed)
{
  return std::isfinite(direction) ? std::clamp(direction, -1.0, 1.0) * speed : 0.0;
}

}  // namespace uel_rover
