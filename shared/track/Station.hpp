
#ifndef STATION_HPP
#define STATION_HPP
#include "Point.hpp"
#include "Tag.hpp"

class Station {
 public:
 public:  // Constructor
  Station(Tag* tag) { this->tag = tag; }
  // Getter for tag
  Tag* getTag() const { return tag; }

  // Setter for tag
  void setTag(Tag* newTag) { tag = newTag; }

  // Getter for positioned
  bool isPositioned() const { return positioned; }

  // Setter for positioned
  void setPositioned(bool newStatus) { positioned = newStatus; }

  // Getter for point
  Point getPoint() const { return point; }

  // Setter for point
  void setPoint(Point newPoint) { point = newPoint; }

 private:
 
  Tag* tag;                 // pointer to a Tag object
  bool positioned = false;  // boolean to indicate if the station is positioned
  Point point;              // the coordinate of the station
};

#endif  // STATION_HPP