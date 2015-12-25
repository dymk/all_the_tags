#include "query.h"
#include "context.h"

#include <asmjit/asmjit.h>

#include <stack>
#include <functional>
#include <queue>
#include <vector>

int QueryClauseMetaNode::entity_count() const {
  return node->entity_count();
}
void QueryClauseMetaNode::debug_print(int indent) const {
  print_indent(indent);
  std::cerr << "meta(" << entity_count() << ") : ";
  node->print_tag_set(std::cerr);
  std::cerr << std::endl;
}

QueryClause *build_lit(Tag *tag) {
  return build_lit(tag, ALL_REL_MASK);
}

QueryClause *build_lit(Tag *tag, rel_type rel) {
  QueryClause *clause = nullptr;

  if(tag->meta_node) {
    // if tag has a metanode, query against it rather than
    // the literal tag

    // collect all metanodes that imply this metanode
    std::unordered_set<SCCMetaNode*> meta_nodes;

    std::function<void(SCCMetaNode*)> recurse = [&](SCCMetaNode *node) {
      assert(node);
      meta_nodes.insert(node);
      for(auto parent : node->parents) {
        recurse(parent);
      }
    };

    // collect all reachable metanodes from this node into meta_nodes
    recurse(tag->meta_node);

    for(auto node : meta_nodes) {
      auto built = new QueryClauseMetaNode(node);
      if(!clause) { clause = built; }
      else        { clause = build_or(clause, built); }
    }
  }
  else {
    // if literal tag is only one on it, query against it
    clause = new QueryClauseLit(tag, rel);
  }

  return clause;
}

QueryClauseBin *build_and(QueryClause *r, QueryClause *l) {
  return new QueryClauseBin(QueryClauseAnd, r, l);
}
QueryClauseBin *build_or (QueryClause *r, QueryClause *l) {
  return new QueryClauseBin(QueryClauseOr, r, l);
}
QueryClauseNot *build_not(QueryClause *c) {
  return new QueryClauseNot(c);
}

// forward decls
QueryClause* hc_tree_optimize(QueryClauseBin* clause);
QueryClause* jit_optimize(QueryClause* clause);

QueryClause *optimize(QueryClause *clause, QueryOptFlags flags) {

  if(flags & QueryOptFlags_Reorder) {
    auto cast_bin = dynamic_cast<QueryClauseBin*>(clause);
    // not a binary clause; ignore
    if(cast_bin) {
      clause = hc_tree_optimize(cast_bin);
    }
  }

  if(flags & QueryOptFlags_JIT) {
    auto old = clause;
    clause = jit_optimize(clause);
    delete old;
  }

  return clause;
}

class QueryClauseCompare
{
  QueryClauseBinType type;
public:
  QueryClauseCompare(QueryClauseBinType type_) : type(type_) {}
  bool operator() (const QueryClause* l, const QueryClause* r) const {
    if(type == QueryClauseAnd) {
      // reverse priority (lower entites at top) for left
      return l->entity_count() > r->entity_count();
    }
    else {
      return l->entity_count() < r->entity_count();
    }
  }
};

QueryClause* hc_tree_optimize(QueryClauseBin* clause) {
  // used when building the HC tree
  std::priority_queue<
    QueryClause*,
    std::vector<QueryClause*>,
    QueryClauseCompare> queued_leafs(QueryClauseCompare(clause->type));

  std::vector<QueryClause*> unsorted_leafs;

  std::stack<QueryClauseBin*> bin_nodes;
  std::stack<QueryClauseBin*> recycle_bin;

  // push a given query clause to the bin_nodes or the leafs
  // queue depending on its type
  auto enqueue_node = [&](QueryClause* c) {
    auto casted = dynamic_cast<QueryClauseBin*>(c);
    if(casted && casted->type == clause->type) {
      bin_nodes.push(casted);
    }
    else {
      unsorted_leafs.push_back(c);
    }
  };

  // return a recycled (or new) binary node
  auto get_bin_node = [&]() {
    if(recycle_bin.size()) {
      auto ret = recycle_bin.top();
      recycle_bin.pop();
      assert(ret->type == clause->type);
      return ret;
    }
    else {
      return new QueryClauseBin(clause->type, nullptr, nullptr);
    }
  };

  bin_nodes.push(clause);
  while(bin_nodes.size()) {
    auto top = bin_nodes.top();
    bin_nodes.pop();

    // get leafs that are also of clause type
    enqueue_node(top->l);
    enqueue_node(top->r);
    top->r = top->l = nullptr;
    recycle_bin.push(top);
  }

  // remove duplicate metanodes within the leafs
  {
    std::unordered_set<SCCMetaNode*> meta_leafs;
    for(auto&& leaf : unsorted_leafs) {
      auto ml = dynamic_cast<QueryClauseMetaNode*>(leaf);
      if(!ml) continue;

      // is a metanode, include only a unique set
      if(meta_leafs.find(ml->node) != meta_leafs.end()) {
        // metanode is already in this "or", remove it
        delete leaf;
        leaf = nullptr;
      }
      else {
        meta_leafs.insert(ml->node);
      }
    }
  }

  // recursivly optimize all the leafs (which reside in unsorted_leafs)
  for(auto&& leaf : unsorted_leafs) {
    if(!leaf) continue;

    auto old_count = leaf->entity_count();
    leaf = optimize(leaf);
    assert(old_count == leaf->entity_count());
  }

  // insert all the leafs into the priority queue
  for(auto leaf : unsorted_leafs) {
    if(!leaf) continue;

    queued_leafs.push(leaf);
  }

  // build the huffman coding tree
  while(queued_leafs.size() > 1) {
    auto first  = queued_leafs.top(); queued_leafs.pop();
    auto second = queued_leafs.top(); queued_leafs.pop();
    auto parent = get_bin_node();
    parent->l = first;
    parent->r = second;
    queued_leafs.push(parent);
  }

  // delete the unused recycled nodes
  while(recycle_bin.size()) {
    delete recycle_bin.top();
    recycle_bin.pop();
  }

  // last element in the
  return queued_leafs.top();
}

struct QueryClauseJitNode : public QueryClause {
  asmjit::JitRuntime runtime;

  typedef bool (*func_type)(const Entity::tags_set*);
  func_type func;

  QueryClauseJitNode() {}
  virtual ~QueryClauseJitNode() {}

  virtual int depth()        const { return 0; }
  virtual int num_children() const { return 0; }
  virtual int entity_count() const { return 0; }

  virtual bool matches_set(const Entity::tags_set& tags) const {
    return func(&tags);
  }

  virtual QueryClauseJitNode *dup() const {
    assert(false);
    return nullptr;
  }

  virtual void debug_print(int indent = 0) const {
    print_indent(indent);
    std::cerr << "jit()" << std::endl;
  }
};

// TODO: merge this logic with the matches_set methods on MetaNode and LitNode
bool extern_set_has_tag(const Entity::tags_set* tags, Tag* tag) {
  return tags->find(tag) != tags->end();
}
bool extern_set_has_meta(const Entity::tags_set* tags, const SCCMetaNode* node) {
  for(auto t : *tags) {
    if(t.tag->meta_node == node) return true;
  }

  return false;
}

QueryClause* jit_optimize(QueryClause* clause) {
  using namespace asmjit;

  auto ret = new QueryClauseJitNode();

  X86Assembler a(&(ret->runtime));
  X86Compiler c(&a);
  c.addFunc(FuncBuilder1<int, Entity::tags_set*>(kCallConvHost));

  X86GpVar tag_set_ptr = c.newIntPtr("tag_set_ptr");
  c.setArg(0, tag_set_ptr);

  X86GpVar test_var = c.newInt8("test_var");

  X86GpVar has_tag_func_ptr = c.newIntPtr("tag_fn");
  X86GpVar has_meta_func_ptr = c.newIntPtr("meta_fn");
  c.mov(has_tag_func_ptr, imm_ptr((void*)extern_set_has_tag));
  c.mov(has_meta_func_ptr, imm_ptr((void*)extern_set_has_meta));

  std::function<void(const QueryClause*, X86GpVar&)> codegen_tree =
    [&c, &has_tag_func_ptr, &has_meta_func_ptr, &tag_set_ptr, &codegen_tree]
    (const QueryClause* clause, X86GpVar& res_var)
  {
    if(auto bin = dynamic_cast<const QueryClauseBin*>(clause)) {
      codegen_tree(bin->l, res_var);
      Label Lcompare_done(c);

      c.test(res_var, res_var);
      if(bin->type == QueryClauseAnd) {
        // first clause is false? short circuit
        c.je(Lcompare_done);
      }
      else {
        // first clause true? short circuit
        c.jne(Lcompare_done);
      }

      codegen_tree(bin->r, res_var);
      c.bind(Lcompare_done);
    }
    else if(auto lit = dynamic_cast<const QueryClauseLit*>(clause)) {
      X86CallNode* call = c.call(has_tag_func_ptr, FuncBuilder2<int, int*, int*>(kCallConvHost));
      call->setArg(0, tag_set_ptr);
      call->setArg(1, imm_ptr(lit->t));
      call->setRet(0, res_var);
    }
    else if(auto meta = dynamic_cast<const QueryClauseMetaNode*>(clause)) {
      X86CallNode* call = c.call(has_meta_func_ptr, FuncBuilder2<int, int*, int*>(kCallConvHost));
      call->setArg(0, tag_set_ptr);
      call->setArg(1, imm_ptr(meta->node));
      call->setRet(0, res_var);
    }
    else if(auto any = dynamic_cast<const QueryClauseAny*>(clause)) {
      c.mov(res_var, 1);
    }
    else if(auto not_ = dynamic_cast<const QueryClauseNot*>(clause)) {
      codegen_tree(not_->c, res_var);
      c.xor_(res_var, 1);
    }
    else {
      clause->debug_print();
      assert(false && "don't know how to handle that node type");
    }
  };

  codegen_tree(clause, test_var);
  c.ret(test_var);
  c.endFunc();
  c.finalize();

  void* func = a.make();
  ret->func = (QueryClauseJitNode::func_type) func;

  return ret;
}
