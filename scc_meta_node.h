#ifndef __SCC_META_NODE_H__
#define __SCC_META_NODE_H__

#include "context.h"

struct SCCMetaNode {
  std::unordered_set<SCCMetaNode*> children;
  std::unordered_set<SCCMetaNode*> parents;
  std::unordered_set<Tag*>         tags;

  bool add_child(SCCMetaNode* c) {
    assert(c);
    assert(c != this);
    auto a = this->children.insert(c).second;
    auto b = c->parents.insert(this).second;
    assert(a == b);
    return a;
  }

  bool remove_child(SCCMetaNode* c) {
    assert(c);
    assert(c != this);
    auto a = this->children.erase(c) == 1;
    auto b = c->parents.erase(this) == 1;
    assert(a == b);
    return a;
  }

  void remove_from_graph() {
    for(auto scc : children) {
      assert(scc != this);
      scc->parents.erase(this);
    }
    while(children.size()) {
      assert(children.erase(*(children.begin())));
    }

    for(auto scc : parents) {
      assert(scc != this);
      scc->children.erase(this);
    }
    while(parents.size()) {
      parents.erase(*(parents.begin()));
    }

    assert(children.size() == 0);
    assert(parents.size() == 0);
  }

  std::ostream& print_tag_set(std::ostream& os) {
    os << "{";
    bool first = true;
    for(auto tag : tags) {
      if(!first) os << ", ";
      first = false;
      os << tag->id;
    }
    os << "}";
    return os;
  }

  int entity_count() const;
};

#endif /* __SCC_META_NODE_H__ */
