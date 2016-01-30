#ifndef __ENTITY_H__
#define __ENTITY_H__

#include <unordered_set>
#include <unordered_map>
#include "all_the_tags/id.h"
#include "all_the_tags/tag.h"

// An edge between an Entity and a Tag,
// with an additional bit of information: a 'rel'ationship
// that can be queried against
// struct TagWithRel {
//   Tag *tag;
//   rel_type rel;

//   TagWithRel(Tag *t_, rel_type rel_ = 1) :
//     tag(t_), rel(rel_) {}

//   bool operator==(const TagWithRel& other) const {
//     return this->tag == other.tag;
//   }
// };

// // dedup based on the tag ptr, not the
// struct TagWithRelHashFunc {
//   size_t operator()(const TagWithRel& twr) const {
//     return (size_t) twr.tag;
//   }
// };

// represents an entity that can be tagged
// is represented by a unique ID
struct Entity {
  typedef std::unordered_map<Tag*, rel_type> tags_set;

  tags_set tags;
  id_type id;

  Entity(id_type _id) : id(_id) {}

  // add tag to the entity (with default relationship 1)
  // returns:
  //  - true: tag was added
  //  - false: tag arleady on this entity
  bool add_tag(Tag* t, rel_type rel = 1) {
    bool success = false;

    if(rel == 0) {
      return false;
    }

    auto have_already = tags.find(t);
    if(have_already != tags.end()) {
      rel_type tagging_rel = (*have_already).second;
      if((tagging_rel & rel) != rel) {
        // already have tag, and it doesn't have this relationship type on it
        (*have_already).second = tagging_rel | rel;
        success = true;
      }
    }
    else {
      success = tags.insert(std::make_pair(t, rel)).second;
      if(success) t->inc_entity_count();
    }

    return success;
  }

  bool remove_tag(Tag* t, rel_type rel = ALL_REL_MASK) {
    auto twr = tags.find(t);

    if(twr == tags.end()) {
      return false;
    }

    rel_type tagging_rel = (*twr).second;

    if((tagging_rel & ~rel) == 0) {
      // would clear all relationship tags on it
      assert(tags.erase(t) == 1);
      t->dec_entity_count();
      return true;
    }
    else {
      rel_type rels_removed = tagging_rel & ~rel;
      if(rels_removed == tagging_rel) {
        // not removing any tagging relationships
        return false;
      }
      else {
        (*twr).second = rels_removed;
        return true;
      }
    }
  }
};

#endif
