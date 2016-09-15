#ifndef __QUERY_H__
#define __QUERY_H__

#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <bitset>

#include "all_the_tags/tag.h"

struct QueryClause;
struct QueryClauseBin;
struct QueryClauseNot;
struct SCCMetaNode;

enum QueryOptFlags {
  QueryOptFlags_Reorder = 0x1,
  QueryOptFlags_JIT     = 0x2
};

QueryClause    *build_lit(Tag *tag);
QueryClause    *build_lit(Tag *tag, rel_type rel_mask);
QueryClauseBin *build_and(QueryClause *r, QueryClause *l);
QueryClauseBin *build_or (QueryClause *r, QueryClause *l);
QueryClauseNot *build_not(QueryClause *c);
QueryClause    *optimize(QueryClause *clause, QueryOptFlags flags = QueryOptFlags_Reorder);

// root clause AST type
struct QueryClause {
  // returns true/false if the clause matches a given unordered set
  virtual bool matches_set(const Tag::tagging_map& tags) const = 0;
  virtual ~QueryClause() {}

  virtual int depth() const = 0;
  virtual int num_children() const = 0;
  virtual int entity_count() const = 0;
  virtual QueryClause *dup() const = 0;

  virtual void debug_print(int indent = 0) const = 0;
protected:
  void print_indent(int indent) const {
    for(int i = 0; i < indent; i++) {
      std::cerr << "  ";
    }
  }
};

// clause negation
struct QueryClauseNot : public QueryClause {
  QueryClause* c;

  QueryClauseNot(QueryClause *c_) : c(c_) {}
  virtual ~QueryClauseNot() { delete c; }

  virtual bool matches_set(const Tag::tagging_map& tags) const {
    return !(c->matches_set(tags));
  }

  virtual int depth()        const { return c->depth() + 1;        }
  virtual int num_children() const { return c->num_children() + 1; }
  virtual int entity_count() const { return c->entity_count();     }
  virtual QueryClauseNot *dup() const {
    return new QueryClauseNot(c->dup());
  }

  virtual void debug_print(int indent = 0) const {
    print_indent(indent);
    std::cerr << "not(" << entity_count() << ") ->" << std::endl;
    c->debug_print(indent + 1);
  }

};

// logical clause operators
enum QueryClauseBinType {
  QueryClauseAnd,
  QueryClauseOr
};
struct QueryClauseBin : public QueryClause {
  QueryClauseBinType type;
  QueryClause *l;
  QueryClause *r;

  QueryClauseBin(QueryClauseBinType type_, QueryClause *l_, QueryClause *r_) :
    type(type_), l(l_), r(r_) {}

  virtual ~QueryClauseBin() {
    if(l) delete l;
    if(r) delete r;
  }

  virtual bool matches_set(const Tag::tagging_map& tags) const {
    if(type == QueryClauseAnd) {
      return l->matches_set(tags) && r->matches_set(tags);
    }
    else {
      return r->matches_set(tags) || l->matches_set(tags);
    }
  }

  virtual int depth()        const { return std::max(l->depth(), r->depth()) + 1;      }
  virtual int num_children() const { return l->num_children() + r->num_children() + 1; }
  virtual int entity_count() const {
    if(type == QueryClauseAnd) {
      return std::min(l->entity_count(), r->entity_count());
    }
    else {
      return std::max(l->entity_count(), r->entity_count());
    }
  }

  virtual QueryClauseBin *dup() const {
    return new QueryClauseBin(type, l->dup(), r->dup());
  }

  virtual void debug_print(int indent = 0) const {
    print_indent(indent);
    std::cerr <<
      "bin(" << (type == QueryClauseAnd ? "and" : "or") << ")" <<
      "(" << entity_count() << ") ->" << std::endl;

    l->debug_print(indent + 1);
    r->debug_print(indent + 1);
  }
};

// literal tag match
struct QueryClauseLit : public QueryClause {
  Tag *t;
  rel_type rel_mask;

  QueryClauseLit(Tag *t_, rel_type rel_mask_) :
    t(t_), rel_mask(rel_mask_) {}

  virtual ~QueryClauseLit() { t = nullptr; }
  virtual bool matches_set(const Tag::tagging_map& tags) const {
    return QueryClauseLit::matches_set(t, rel_mask, tags);
  }

  static inline bool matches_set(Tag *const tag, const rel_type rel_mask, const Tag::tagging_map& tags) {
    auto iter = tags.find(tag);
    if(iter == tags.end()) {
      return false;
    }

    rel_type rel = (*iter).second;
    return rel & rel_mask;
  }

  virtual int depth()        const { return 0; }
  virtual int num_children() const { return 0; }
  virtual int entity_count() const { return t->entity_count(); }
  virtual QueryClauseLit *dup() const {
    return new QueryClauseLit(t, rel_mask);
  }

  virtual void debug_print(int indent = 0) const {
    print_indent(indent);
    std::cerr << "lit(" << t->entity_count() << ") (" << std::bitset<8>(rel_mask) << ") -> " << t->id << std::endl;
  }
};

struct QueryClauseMetaNode : public QueryClause {
  SCCMetaNode* node;
  rel_type rel;

  QueryClauseMetaNode(SCCMetaNode *node_, rel_type rel_) : node(node_), rel(rel_) {}
  virtual ~QueryClauseMetaNode() { node = nullptr; }

  virtual bool matches_set(const Tag::tagging_map& tags) const {
    return QueryClauseMetaNode::matches_set(node, rel, tags);
  }

  static inline bool matches_set(SCCMetaNode const* node, const rel_type rel, const Tag::tagging_map& tags) {
    assert(node);
    // do any of the tags belong to this metanode
    // TODO: store set of relevant meta_nodes on posts instead of
    // tags directly
    for(auto t : tags) {
      if((t.first->meta_node() == node) && (t.second & rel)) {
        return true;
      }
    }

    return false;
  }

  virtual int depth()        const { return 0; }
  virtual int num_children() const { return 0; }
  virtual int entity_count() const;
  virtual QueryClauseMetaNode *dup() const {
    return new QueryClauseMetaNode(node, rel);
  }

  virtual void debug_print(int indent = 0) const;
};

// represents an empty clause (matches everything)
struct QueryClauseAny : public QueryClause {
  virtual bool matches_set(const Tag::tagging_map& tags) const {
    (void)tags;
    return true;
  }

  QueryClauseAny() {}
  virtual ~QueryClauseAny() {}

  virtual int depth()        const { return 0; }
  virtual int num_children() const { return 0; }
  virtual int entity_count() const { return 999999999; }

  virtual void debug_print(int indent = 0) const {
    print_indent(indent);
    std::cerr << "any(999999999)" << std::endl;
  }

  virtual QueryClauseAny* dup() const {
    return new QueryClauseAny();
  }
};

#endif /* __QUERY_H__ */
