#ifndef __ENTITY_H__
#define __ENTITY_H__

#include <unordered_set>
#include "all_the_tags/id.h"
#include "all_the_tags/tag.h"

// An edge between an Entity and a Tag,
// with an additional bit of information: a 'rel'ationship
// that can be queried against
struct TagWithRel {
  Tag *tag;
  rel_type rel;

  TagWithRel(Tag *t_, rel_type rel_ = 1) :
    tag(t_), rel(rel_) {}

  bool operator==(const TagWithRel& other) const {
    return this->tag == other.tag;
  }
};

// dedup based on the tag ptr, not the
struct TagWithRelHashFunc {
  size_t operator()(const TagWithRel& twr) const {
    return (size_t) twr.tag;
  }
};

// represents an entity that can be tagged
// is represented by a unique ID
struct Entity {
  typedef std::unordered_set<TagWithRel, TagWithRelHashFunc> tags_set;

  tags_set tags;
  id_type id;

  Entity(id_type _id) : id(_id) {}

  // add tag to the entity (with default relationship 1)
  // returns:
  //  - true: tag was added
  //  - false: tag arleady on this entity
  bool add_tag(Tag* t, rel_type rel = 1) {
    auto twr = TagWithRel(t, rel);
    auto success = tags.insert(twr).second;
    if(success) t->inc_entity_count();
    return success;
  }

  bool remove_tag(Tag* t) {
    auto success = tags.erase(t) == 1;
    if(success) t->dec_entity_count();
    return success;
  }
};

#endif
