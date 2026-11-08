#ifndef MATH_HPP
#define MATH_HPP
#include <math.h>
static float degreesToRadians(float degrees) { return degrees * M_PI / 180.0f; }

static float radiansToDegrees(float radians) { return radians * 180.0f / M_PI; }

static int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return value;
}

#endif  // MATH_HPP