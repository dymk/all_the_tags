#ifndef __TAGS_H__
#define __TAGS_H__

#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include <iostream>

#include "all_the_tags/id.h"

// needs forward declaration because C++ uses goddamn textual inclusion
struct Context;
struct SCCMetaNode;

struct Tag {
  id_type id;

  using tagging_map = std::unordered_map<Tag*, rel_type>;
  tagging_map tags;

  using implied_set = std::unordered_set<
    Tag*,
    std::hash<Tag*>,
    std::equal_to<Tag*>,
    std::allocator<Tag*> >;

  // TODO: implement tag implications
  implied_set implies;
  implied_set implied_by;

  Context *context;

  // DAG SCC meta node that the tag belongs to
private:
  SCCMetaNode *meta_node_;

public:
  SCCMetaNode* meta_node() const {
    return meta_node_;
  }
  SCCMetaNode* set_meta_node(SCCMetaNode* node) {
    assert(node);
    this->meta_node_ = node;
    return node;
  }
  void clear_meta_node() {
    this->meta_node_ = nullptr;
  }

  // how many entities have this particular tag
  int _entity_count;

public:
  Tag(Context *context_, id_type _id) :
    id(_id),
    context(context_),
    meta_node_(nullptr),
    _entity_count(0) {}

  // tag imply/unimply
  // this tag implies -> other tag
  bool imply(Tag *other);
  bool unimply(Tag *other);

  int entity_count() const {
    return _entity_count;
  }

  void inc_entity_count() {
    _entity_count++;
  }
  void dec_entity_count() {
    _entity_count--;
  }

  // tag add/removal
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
      rel_type tagging_rel = have_already->second;
      if((tagging_rel & rel) != rel) {
        // already have tag, and it doesn't have this relationship type on it
        have_already->second = tagging_rel | rel;
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
      const auto _erased = tags.erase(t);
      assert(_erased == 1);
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
