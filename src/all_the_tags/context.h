#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <utility>

#include "all_the_tags/query.h"
#include "all_the_tags/scc_meta_node.h"

struct Tag;

// error codes for `Context::query`
static const int ERR_CONTEXT_DIRTY = -1;

struct Context {
private:
  id_type last_tag_id;

  std::unordered_map<id_type, Tag*> id_to_tag;

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
    recalc_metagraph(false)
    {}
  ~Context();

  // returns a new tag (or null)
  Tag *new_tag();
  Tag *new_tag(id_type id);

  // inverse of new_tag
  void destroy_tag(Tag *t);

  // look up tag by id
  Tag* tag_by_id(id_type tid) const;

  // INTERNAL
  // notify the context that 'dirtying_tag' gained/lost 'other' as an implied
  // tag. 'gained_imply' true if it now implies other, false if implication removed
  // should only be called by Tag* internally
  void dirty_tag_imply_dag(Tag* dirtying_tag, bool gained_imply, Tag* other);

public:
  // calls 'match' callback with all entities that match the QueryClause
  // returns true if query was succesfull, false otherwise (e.g. context was dirty)
  template<class UnaryFunction>
  long query(const QueryClause *q, UnaryFunction match) const {
    if(is_dirty()) {
      return ERR_CONTEXT_DIRTY;
    }

    long i = 0;
    for(auto&& iter : id_to_tag) {
      i++;
      auto e = iter.second;
      if(q->matches_set(e->tags)) {
        match(e);
      }
    }
    return i;
  }

  // context statistics
  size_t num_tags() const {
    return id_to_tag.size();
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
