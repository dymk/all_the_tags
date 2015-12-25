#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <utility>

#include "entity.h"
#include "query.h"
#include "scc_meta_node.h"

struct Tag;

struct Context {
private:
  id_type last_tag_id;
  id_type last_entity_id;

  std::unordered_map<id_type,     Tag*> id_to_tag;

  std::unordered_map<id_type,  Entity*> id_to_entity;

  // does the metagraph need recalculating? call make_clean
  // to recalculate the metagraph
  bool recalc_metagraph;

  // internals
  Tag *new_tag_common(id_type id);

public:
  // meta nodes representing the DAG of tag implications
  std::unordered_set<SCCMetaNode*> meta_nodes;
  std::unordered_set<SCCMetaNode*> sink_meta_nodes;

  Context() :
    last_tag_id(0),
    last_entity_id(0),
    recalc_metagraph(false)
    {}
  ~Context();

  // returns a new tag (or null)
  Tag *new_tag();
  Tag *new_tag(id_type id);

  // inverse of new_tag
  void destroy_tag(Tag *t);

  // returns a newly created entity
  Entity *new_entity();
  Entity *new_entity(id_type id);

  // inverse of new_entity
  void destroy_entity(Entity *e);

  // look up tag by id
  Tag* tag_by_id(id_type tid) const;

  // notify the context that a node gained or
  // removed a parent, and to recalculate the tag tree's pre/post numbers
  void dirty_tag_parent_tree(Tag* dirtying_tag);

  // notify the context that 'dirtying_tag' gained/lost 'other' as an implied
  // tag. 'gained_imply' true if it now implies other, false if implication removed
  void dirty_tag_imply_dag(Tag* dirtying_tag, bool gained_imply, Tag* other);

  // look up entity by id
  Entity* entity_by_id(id_type eid) const;

  // calls 'match' with all entities that match the QueryClause
  template<class UnaryFunction>
  void query(const QueryClause *q, UnaryFunction match) const {
    if(is_dirty()) {
      assert(false && "can't call query on dirty context");
    }

    for(auto&& iter : id_to_entity) {
      auto e = iter.second;
      if(q->matches_set(e->tags)) {
        match(e);
      }
    }
  }

  // context statistics
  size_t num_tags() const {
    return id_to_tag.size();
  }
  size_t num_entities() const {
    return id_to_entity.size();
  }

  // is a call to make_clean() required?
  bool is_dirty() const {
    return recalc_metagraph;
  }

  // suppress incremental DAG building, let it
  // happen all in one go with a call to make_clean
  void mark_dirty() {
    recalc_metagraph = true;
  }

  // recalculate the metagraph of tag implications from scratch
  void make_clean();
};

#endif
