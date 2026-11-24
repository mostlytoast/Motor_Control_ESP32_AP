#ifndef MATH_HPP
#define MATH_HPP
#include <math.h>
static float degreesToRadians(float degrees) { return degrees * M_PI / 180.0f; }

static float radiansToDegrees(float radians) { return radians * 180.0f / M_PI; }
static bool areFloatsEqual(float a, float b, float epsilon = 0.00001f) {
  return std::fabs(a - b) < epsilon;
}

static int clampInt(int value, int minValue, int maxValue) {
  if (value < minValue) {
    return minValue;
  }

  if (value > maxValue) {
    return maxValue;
  }

  return value;
}

static float clampFloat(float value, float minimum, float maximum) {

  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}

static float mapValue(float value, float inMin, float inMax, float outMin,
               float outMax) {

  if (inMax == inMin) {
    return outMin;
  }

  return (value - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}
#endif  // MATH_HPP