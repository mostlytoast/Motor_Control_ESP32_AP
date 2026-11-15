#ifndef TAGS_HPP
#define TAGS_HPP
#include "Tag.hpp"
// Tags tags;

class Tags {
 public:
  Tag* findTagByUid(Uid uid) {
    if (!uid.isValid()) {
      return nullptr;
    }

    for (Tag& tag : tags) {
      if (tag.getUid().isValid() && tag.getUid().equals(uid)) {
        return &tag;
      }
    }

    return nullptr;
  }

  void addTag(Tag tag) { tags.emplace_back(tag); }

  bool removeTag(const Tag* tag) {
    auto it = std::find_if(tags.begin(), tags.end(),
                           [tag](Tag& p) { return p == *tag; });

    if (it != tags.end()) {
      tags.erase(it);
      return true;
    }
    return false;
  }
  int size() const { return tags.size(); }
  bool empty() const { return tags.size() == 0; }
  //   bool operator==(const Tag& other) const {}
  Tag& operator[](int idx) { return tags[idx]; }
  Tag operator[](int idx) const { return tags[idx]; }
  auto begin() { return tags.begin(); }
  auto end() { return tags.end(); }

  auto begin() const { return tags.begin(); }
  auto end() const { return tags.end(); }

 private:
  std::vector<Tag> tags;
};
#endif  // TAGS_HPP
