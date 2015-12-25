#ifndef __TAGS_H__
#define __TAGS_H__

#include <unordered_set>
#include <cassert>

#include "id.h"

// needs forward declaration because C++ uses goddamn textual inclusion
struct Context;
struct SCCMetaNode;

struct Tag {
  id_type id;

  // TODO: implement tag implications
  std::unordered_set<Tag*> implies;
  std::unordered_set<Tag*> implied_by;

  Context *context;

  // DAG SCC meta node that the tag belongs to
  SCCMetaNode *meta_node;

  // how many entities have this particular tag
  int _entity_count;

public:
  Tag(Context *context_, id_type _id) :
    id(_id),
    context(context_),
    meta_node(nullptr),
    _entity_count(0) {}

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
};

#endif
