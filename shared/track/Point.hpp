#ifndef POINT_HPP
#define POINT_HPP

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "../utils/Math.hpp"

class Point {
 public:
  // Default constructor
  Point() : x(0.0f), y(0.0f), heading(0.0f) {}

  // Constructor from C string
  Point(const char* pointString) : Point() { fromString(pointString); }

  // Constructor from Arduino String
  Point(const String& pointString) : Point() { fromString(pointString); }

  // Parameterized constructor
  Point(float x, float y, float heading) : x(x), y(y), heading(heading) {}

  float getX() const { return x; }
  void setX(float newX) { x = newX; }

  float getY() const { return y; }
  void setY(float newY) { y = newY; }

  float getHeading() const { return heading; }
  void setHeading(float newHeading) { heading = newHeading; }

  bool operator==(const Point& other) const {
    return x == other.x && y == other.y && heading == other.heading;
  }

  bool operator!=(const Point& other) const { return !(*this == other); }

  // Convert Point to string: x:y:heading
  String toString() const {
    String result = String(x, 6);
    result += ":";
    result += String(y, 6);
    result += ":";
    result += String(heading, 6);
    return result;
  }

  // Convert string to Point: x:y:heading
  bool fromString(const char* pointString) {
    if (pointString == nullptr) return false;

    float newX, newY, newHeading;
    char extra;

    // Ensure exactly three values are present.
    if (sscanf(pointString, "%f:%f:%f %c", &newX, &newY, &newHeading, &extra) !=
        3) {
      return false;
    }

    x = newX;
    y = newY;
    heading = newHeading;

    return true;
  }

  // Overload for Arduino String
  bool fromString(const String& pointString) {
    return fromString(pointString.c_str());
  }

  void normalizeHeading() {
    if (std::isinf(heading)) {
      Serial.println("heading is infinite");
      heading = 0;
      return;
    }
    while (heading >= 360.0f) {
      heading -= 360.0f;
    }

    while (heading < 0.0f) {
      heading += 360.0f;
    }
  }

  void movePoint(float heading_, float distance_) {
    // TODO get working with tag.getPoint
    float radians = degreesToRadians(heading_);
    // todo have to check heading
    x = x + cos(radians) * distance_;
    y = y + sin(radians) * distance_;
    heading = heading_;
    normalizeHeading();
  }

  void rotatePoint(float heading_, const Point& pivot) {
    float radians = degreesToRadians(heading_);
    float c = cosf(radians);
    float s = sinf(radians);

    // Position relative to pivot.
    float dx = x - pivot.getX();
    float dy = y - pivot.getY();

    // Rotate around pivot.
    float rotatedX = dx * c - dy * s;
    float rotatedY = dx * s + dy * c;

    // Translate back.
    x = pivot.getX() + rotatedX;
    y = pivot.getY() + rotatedY;

    // Rotate heading.
    heading += heading_;
    normalizeHeading();
  }
  void transformPoint(const Point& origin, const Point& destination,
                      float rotation) {
    float radians = degreesToRadians(rotation);

    float c = cosf(radians);
    float s = sinf(radians);

    // Relative position to origin.
    float dx = x - origin.x;
    float dy = y - origin.y;

    // Rotate and translate.
    x = destination.x + (dx * c - dy * s);
    y = destination.y + (dx * s + dy * c);

    // Rotate heading.
    heading += rotation;
    normalizeHeading();
  }

 private:
  float x;
  float y;
  float heading;
};

#endif  // POINT_HPP