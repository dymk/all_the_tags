#include "test_helper.h"

class TagImplicationTest : public ::testing::Test {
public:
  Context ctx;
  Tag *a, *b, *c, *d, *e;

  void SetUp() {
    a = ctx.new_tag();
    b = ctx.new_tag();
    c = ctx.new_tag();
    d = ctx.new_tag();
    e = ctx.new_tag();
  }
};

TEST_F(TagImplicationTest, MatchesEntity) {
  auto e = ctx.new_entity();
  e->add_tag(a);

  auto q = build_lit(b);
  auto res = query(ctx, *q);
  ASSERT_EQ(SET(Entity*, {}), res);
  delete q;

  // e tagged with 'a' implies tagged with 'b'
  a->imply(b);
  ASSERT_FALSE(ctx.is_dirty());

  q = build_lit(b);
  q->debug_print();
  res = query(ctx, *q);
  ASSERT_EQ(SET(Entity*, {e}), res);
  delete q;
}

TEST_F(TagImplicationTest, MakesRightDAG) {
  // initially, no SCCs (by default, nodes don't create an SCC)
  ASSERT_EQ(0, ctx.meta_nodes.size());

  // should have SCC of set {a} and of set {b}
  a->imply(b);
  ASSERT_FALSE(ctx.is_dirty());
  ASSERT_EQ(2, ctx.meta_nodes.size());
  ASSERT_EQ(a->meta_node->parents,  SET(SCCMetaNode*, {}));
  ASSERT_EQ(a->meta_node->children, SET(SCCMetaNode*, {b->meta_node}));
  ASSERT_EQ(b->meta_node->parents,  SET(SCCMetaNode*, {a->meta_node}));
  ASSERT_EQ(b->meta_node->children, SET(SCCMetaNode*, {}));

  // should have SCC of set {a}, {b}, {c}
  b->imply(c);
  ASSERT_FALSE(ctx.is_dirty());
  ASSERT_EQ(3, ctx.meta_nodes.size());
  ASSERT_EQ(b->meta_node->parents,  SET(SCCMetaNode*, {a->meta_node}));
  ASSERT_EQ(b->meta_node->children, SET(SCCMetaNode*, {c->meta_node}));
  ASSERT_EQ(c->meta_node->parents,  SET(SCCMetaNode*, {b->meta_node}));
  ASSERT_EQ(c->meta_node->children, SET(SCCMetaNode*, {}));

  // collapse all the SCCs into {a,c,c}
  c->imply(a);
  ASSERT_FALSE(ctx.is_dirty());
  ASSERT_EQ(1, ctx.meta_nodes.size());

  auto mn = *(ctx.meta_nodes.begin());
  ASSERT_TRUE(mn);

  ASSERT_EQ(mn->tags,     SET(Tag*,         {a, b, c}));
  ASSERT_EQ(mn->children, SET(SCCMetaNode*, {}));
  ASSERT_EQ(mn->parents,  SET(SCCMetaNode*, {}));

  ASSERT_EQ(a->meta_node, mn);
  ASSERT_EQ(b->meta_node, mn);
  ASSERT_EQ(c->meta_node, mn);
}

TEST_F(TagImplicationTest, ABC_SCC) {
  auto e = ctx.new_entity();
  e->add_tag(b);

  // first check that no tags match
  auto q = build_lit(c);
  auto res = query(ctx, *q);
  ASSERT_EQ(SET(Entity*, {}), res);
  delete q;

  // make an SCC of a,b,c
  a->imply(b);
  b->imply(c);
  c->imply(a);
  ASSERT_FALSE(ctx.is_dirty());

  // verify {e} matches for any of them
  for(auto node : {a, b, c}) {
    q = build_lit(node);
    q->debug_print();
    res = query(ctx, *q);
    ASSERT_EQ(SET(Entity*, {e}), res);
    delete q;
  }
}

TEST_F(TagImplicationTest, DiamondTest) {
  ASSERT_EQ(0, ctx.sink_meta_nodes.size());

  a->imply(b);
  ASSERT_FALSE(ctx.is_dirty());
  ASSERT_EQ(1, ctx.sink_meta_nodes.size());

  a->imply(c);
  ASSERT_FALSE(ctx.is_dirty());
  ASSERT_EQ(2, ctx.sink_meta_nodes.size());

  b->imply(d);
  ASSERT_FALSE(ctx.is_dirty());
  ASSERT_EQ(2, ctx.sink_meta_nodes.size());

  c->imply(d);
  ASSERT_FALSE(ctx.is_dirty());
  ASSERT_EQ(1, ctx.sink_meta_nodes.size());

  //      a
  //     / \
  //    b   c
  //     \ /
  //      d

  // a, b, c, d all are separate metanodes
  // should only print out {d}
  for(auto node : ctx.sink_meta_nodes) {
    node->print_tag_set(std::cerr); std::cerr << std::endl;
  }

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {d->meta_node}));

  ASSERT_EQ(4, ctx.meta_nodes.size());
  ASSERT_EQ(1, ctx.sink_meta_nodes.size());

  // d -> a creates a cycle, collapse all
  d->imply(a);
  ASSERT_EQ(1, ctx.meta_nodes.size());
  ASSERT_EQ(1, ctx.sink_meta_nodes.size());
}

TEST_F(TagImplicationTest, TwoDiamondTest) {
  a->imply(b);
  a->imply(c);
  b->imply(d);
  c->imply(d);
  b->imply(e);
  ASSERT_FALSE(ctx.is_dirty());

  //      a
  //     / \
  //    b   c
  //   / \ /
  //  e   d

  // a, b, c, d, e all are separate metanodes
  ASSERT_EQ(5, ctx.meta_nodes.size());
  ASSERT_EQ(2, ctx.sink_meta_nodes.size());

  // d -> a should collapse {a, b, c, d} into single node
  d->imply(a);
  ASSERT_FALSE(ctx.is_dirty());

  // {a,b,c,d}
  //     |
  //     e

  ASSERT_EQ(2, ctx.meta_nodes.size());
  ASSERT_EQ(1, ctx.sink_meta_nodes.size());

  auto sink = *(ctx.sink_meta_nodes.begin());
  ASSERT_EQ(sink->tags, SET(Tag*, {e}));
  ASSERT_EQ(e->meta_node, sink);

  for(auto node : {a, b, c, d}) {
    ASSERT_EQ(node->meta_node->tags, SET(Tag*, {a, b, c, d}));
  }
}

TEST_F(TagImplicationTest, InNodesCollapse) {
  d->imply(b);
  e->imply(c);

  a->imply(b);
  b->imply(c);
  c->imply(a);
  ASSERT_FALSE(ctx.is_dirty());

  // {a, b, c} cycle
  // d -> b
  // e -> c
  // check that d & e -> {a, b, c}

  // {a,b,c} is the sink metanode
  // {e}, {d} the other two
  ASSERT_EQ(3, ctx.meta_nodes.size());
  ASSERT_EQ(1, ctx.sink_meta_nodes.size());


  // a, b, c collapsed
  for(auto t : {a, b, c}) {
    for(auto s : {a, b, c}) {
      ASSERT_EQ(t->meta_node, s->meta_node);
    }
  }

  // d and e point into {a,b,c} metanode
  ASSERT_EQ(d->meta_node->children, SET(SCCMetaNode*, {a->meta_node}));
  ASSERT_EQ(e->meta_node->children, SET(SCCMetaNode*, {a->meta_node}));
  ASSERT_EQ(d->meta_node->parents, SET(SCCMetaNode*, {}));
  ASSERT_EQ(e->meta_node->parents, SET(SCCMetaNode*, {}));

  // only sink is the {a,b,c} metanode
  auto sink = *(ctx.sink_meta_nodes.begin());
  ASSERT_EQ(sink->tags, SET(Tag*, {a,b,c}));
}

TEST_F(TagImplicationTest, OutNodesCollapse) {
  a->imply(d);
  b->imply(e);

  a->imply(b);
  b->imply(c);
  c->imply(a);
  ASSERT_FALSE(ctx.is_dirty());

  // {a, b, c} cycle
  // a -> d
  // b -> e
  // check that {a,b,c} -> {d} and {e}

  // {e}, {d} sinks
  ASSERT_EQ(3, ctx.meta_nodes.size());
  ASSERT_EQ(2, ctx.sink_meta_nodes.size());

  // a, b, c collapsed
  for(auto t : {a, b, c}) {
    for(auto s : {a, b, c}) {
      ASSERT_EQ(t->meta_node, s->meta_node);
    }
  }

  // abc point into d,e metanode
  auto d_mn = d->meta_node;
  auto e_mn = e->meta_node;

  ASSERT_NE(d_mn, e_mn);

  ASSERT_EQ(a->meta_node->children, SET(SCCMetaNode*, {d_mn, e_mn}));
  ASSERT_EQ(a->meta_node->parents,  SET(SCCMetaNode*, {}));

  // only sink is the {d}, {e} metanode
  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {d_mn, e_mn}));
}

TEST_F(TagImplicationTest, ImplyRemoveOnce) {
  ASSERT_TRUE(a->imply(b));
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {b->meta_node}));
  ASSERT_EQ(ctx.meta_nodes.size(), 2);

  ASSERT_TRUE(a->unimply(b));
  ASSERT_FALSE(a->unimply(b));
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes.size(), 0);
  ASSERT_EQ(ctx.meta_nodes.size(), 0);

  ASSERT_EQ(a->meta_node, nullptr);
  ASSERT_EQ(b->meta_node, nullptr);
}

TEST_F(TagImplicationTest, TwoNodesRemoval) {
  ASSERT_TRUE(a->imply(b));
  ASSERT_TRUE(b->imply(a));
  ASSERT_TRUE(c->imply(d));
  ASSERT_TRUE(d->imply(c));

  ASSERT_TRUE(a->imply(c));
  ASSERT_TRUE(b->imply(d));
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {c->meta_node}));
  ASSERT_EQ(ctx.meta_nodes.size(), 2);

  // SCCs: {a, b} and {c, d}
  //  a -> c
  //  |    |
  //  b -> d

  // remove a->c
  // shouldn't change the DAG

  ASSERT_TRUE(a->unimply(c));
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {c->meta_node}));
  ASSERT_EQ(ctx.meta_nodes.size(), 2);

  ASSERT_EQ(a->meta_node, b->meta_node);
  ASSERT_EQ(c->meta_node, d->meta_node);
}

TEST_F(TagImplicationTest, ImplyCycleRemove) {
  a->imply(b);
  b->imply(c);
  c->imply(a);
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.meta_nodes.size(), 1);
  ASSERT_EQ(ctx.sink_meta_nodes.size(), 1);
  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {a->meta_node}));
  for(auto tag : {a, b, c}) {
    ASSERT_EQ(tag->meta_node, *(ctx.sink_meta_nodes.begin()));
  }

  ASSERT_TRUE(c->unimply(a));
  ASSERT_TRUE(ctx.is_dirty());
  ctx.make_clean();
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_NE(a->meta_node, b->meta_node);
  ASSERT_NE(b->meta_node, c->meta_node);

  // should bring DAG graph back to a -> b -> c
  ASSERT_EQ(3, ctx.meta_nodes.size());
  ASSERT_EQ(1, ctx.sink_meta_nodes.size());
  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {c->meta_node}));

  for(auto t : {a, b, c}) {
    for(auto s : {a, b, c}) {
      if(s == t) continue;
      ASSERT_NE(t->meta_node, s->meta_node);
    }
  }
}

TEST_F(TagImplicationTest, TwoNodesRemoval_Dirty) {
  ctx.mark_dirty();
  ASSERT_TRUE(ctx.is_dirty());
  ASSERT_TRUE(a->imply(b));
  ASSERT_TRUE(b->imply(a));
  ASSERT_TRUE(c->imply(d));
  ASSERT_TRUE(d->imply(c));

  ASSERT_TRUE(a->imply(c));
  ASSERT_TRUE(b->imply(d));
  ASSERT_TRUE(ctx.is_dirty());
  ctx.make_clean();
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {c->meta_node}));
  ASSERT_EQ(ctx.meta_nodes.size(), 2);

  // SCCs: {a, b} and {c, d}
  //  a -> c
  //  |    |
  //  b -> d

  // remove a->c
  // shouldn't change the DAG
  //  a    c
  //  |    |
  //  b -> d

  ctx.mark_dirty();
  ASSERT_TRUE(a->unimply(c));
  ASSERT_TRUE(ctx.is_dirty());
  ctx.make_clean();
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {c->meta_node}));
  ASSERT_EQ(ctx.meta_nodes.size(), 2);

  ASSERT_EQ(a->meta_node, b->meta_node);
  ASSERT_EQ(c->meta_node, d->meta_node);
}

TEST_F(TagImplicationTest, TwoNodesRemoval_AutoClean) {
  ctx.mark_dirty();
  ASSERT_TRUE(ctx.is_dirty());
  ASSERT_TRUE(a->imply(b));
  ASSERT_TRUE(b->imply(a));
  ASSERT_TRUE(c->imply(d));
  ASSERT_TRUE(d->imply(c));

  ASSERT_TRUE(a->imply(c));
  ASSERT_TRUE(b->imply(d));
  ASSERT_TRUE(ctx.is_dirty());
  ctx.make_clean();
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {c->meta_node}));
  ASSERT_EQ(ctx.meta_nodes.size(), 2);

  // SCCs: {a, b} and {c, d}
  //  a -> c
  //  |    |
  //  b -> d

  // remove a->c
  // shouldn't change the DAG
  //  a    c
  //  |    |
  //  b -> d

  ctx.mark_dirty();
  ASSERT_TRUE(a->unimply(c));
  ASSERT_TRUE(ctx.is_dirty());
  ctx.make_clean();
  ASSERT_FALSE(ctx.is_dirty());

  ASSERT_EQ(ctx.sink_meta_nodes, SET(SCCMetaNode*, {c->meta_node}));
  ASSERT_EQ(ctx.meta_nodes.size(), 2);

  ASSERT_EQ(a->meta_node, b->meta_node);
  ASSERT_EQ(c->meta_node, d->meta_node);
}
