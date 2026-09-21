#include <limits>

#include <gtest/gtest.h>

#include "teleop_speed.hpp"

using uel_rover::levelSpeed;
using uel_rover::scaledCommand;
using uel_rover::steppedLevel;

TEST(SteppedLevel, moves_one_level_in_the_direction_of_the_sign)
{
  EXPECT_EQ(steppedLevel(6, 1, 10), 7);
  EXPECT_EQ(steppedLevel(6, -1, 10), 5);
  EXPECT_EQ(steppedLevel(6, 100, 10), 7);   // 크기는 무시하고 부호만 본다
  EXPECT_EQ(steppedLevel(6, 0, 10), 6);
}

TEST(SteppedLevel, stays_within_one_and_levels)
{
  EXPECT_EQ(steppedLevel(10, 1, 10), 10);
  EXPECT_EQ(steppedLevel(1, -1, 10), 1);    // 0 단계(정지)로는 내려가지 않는다
  EXPECT_EQ(steppedLevel(99, 0, 10), 10);   // 범위 밖 초기값도 안으로 들어온다
}

TEST(LevelSpeed, is_proportional_to_level)
{
  EXPECT_DOUBLE_EQ(levelSpeed(10, 10, 0.26), 0.26);
  EXPECT_DOUBLE_EQ(levelSpeed(5, 10, 0.26), 0.13);
}

TEST(ScaledCommand, multiplies_direction_by_speed)
{
  EXPECT_DOUBLE_EQ(scaledCommand(1.0, 0.2), 0.2);
  EXPECT_DOUBLE_EQ(scaledCommand(-1.0, 0.2), -0.2);
  EXPECT_DOUBLE_EQ(scaledCommand(0.0, 0.2), 0.0);
}

TEST(ScaledCommand, never_exceeds_the_selected_speed)
{
  EXPECT_DOUBLE_EQ(scaledCommand(5.0, 0.2), 0.2);
  EXPECT_DOUBLE_EQ(scaledCommand(-5.0, 0.2), -0.2);
}

TEST(ScaledCommand, turns_non_finite_input_into_stop)
{
  EXPECT_DOUBLE_EQ(scaledCommand(std::numeric_limits<double>::quiet_NaN(), 0.2), 0.0);
  EXPECT_DOUBLE_EQ(scaledCommand(std::numeric_limits<double>::infinity(), 0.2), 0.0);
}
