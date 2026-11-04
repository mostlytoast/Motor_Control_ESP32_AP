
#ifndef POINT_HPP
#define POINT_HPP

class Point {
 public:
  // Default constructor
  Point() : x(0.0), y(0.0), heading(0.0) {}

  // Parameterized constructor
  Point(float x, float y, float heading) : x(x), y(y), heading(heading) {}

  // Getter for x
  float getX() const { return x; }

  // Setter for x
  void setX(float newX) { x = newX; }

  // Getter for y
  float getY() const { return y; }

  // Setter for y
  void setY(float newY) { y = newY; }

  // Getter for heading
  float getHeading() const { return heading; }

  // Setter for heading
  void setHeading(float newHeading) { heading = newHeading; }
  bool operator==(const Point& other) const {
    if (x != other.getX()) return false;
    if (y != other.getY()) return false;
    if (heading != other.getHeading()) return false;
    return true;
  }

  bool operator!=(const Point& other) const { return !(*this == other); }

 private:
  float x;        // x-coordinate
  float y;        // y-coordinate
  float heading;  // Angle in radians
};

#endif  // POINT_HPP