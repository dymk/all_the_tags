#include "gtest/gtest.h"
#include "test_helper.h"

TEST(EntityTest, MakeEntity) {
  Context c;
  auto entity = c.new_tag();
  ASSERT_TRUE(entity);
}

class EntityAndTagTest : public ::testing::Test {
public:
  Context ctx;
  Tag *e1, *e2;
  Tag *foo, *bar;

  void SetUp() {
    e1 = ctx.new_tag();
    e2 = ctx.new_tag();
    assert(e1 && e2);
    foo = ctx.new_tag();
    bar = ctx.new_tag();
  }
};

class EntityAndTagTest2 : public ::testing::Test {
public:
  Context ctx;
  Tag *e1, *e2;
  Tag *a, *b, *c, *d;

  void SetUp() {
    e1 = ctx.new_tag();
    e2 = ctx.new_tag();
    assert(e1 && e2);

    a = ctx.new_tag();
    b = ctx.new_tag();
    c = ctx.new_tag();
    d = ctx.new_tag();
  }
};

class EntityAndRealTagTest : public ::testing::Test {
public:
  Context ctx;
  Tag *e1, *e2;
  Tag *cat, *dog, *feline, *cainine, *zoo, *zoo2;

  void SetUp() {
    e1 = ctx.new_tag();
    e2 = ctx.new_tag();
    assert(e1 && e2);

    cat = ctx.new_tag();
    dog = ctx.new_tag();
    feline = ctx.new_tag();
    cainine = ctx.new_tag();
    zoo = ctx.new_tag();
    zoo2 = ctx.new_tag();
  }
};

TEST_F(EntityAndTagTest, TagEntity) {
  ASSERT_TRUE(e1->add_tag(foo));
  ASSERT_EQ(e1->tags, Tag::tagging_map({{foo, 1}}));

  ASSERT_TRUE(e1->remove_tag(foo));
  ASSERT_FALSE(e1->remove_tag(foo));
  ASSERT_EQ(e1->tags, Tag::tagging_map({}));
}

TEST_F(EntityAndTagTest, TagEntityWithRels) {
  ASSERT_EQ(0, foo->entity_count());
  ASSERT_FALSE(e1->add_tag(foo, 0));
  ASSERT_EQ(0, foo->entity_count());

  ASSERT_TRUE(e1->add_tag(foo, 1));
  ASSERT_EQ(1, foo->entity_count());

  ASSERT_FALSE(e1->add_tag(foo, 1));
  ASSERT_EQ(1, foo->entity_count());

  ASSERT_TRUE(e1->add_tag(foo, 4));
  ASSERT_EQ(1, foo->entity_count());

  ASSERT_EQ(e1->tags, Tag::tagging_map({{foo, 4|1}}));

  ASSERT_TRUE(e1->remove_tag(foo, 1));
  ASSERT_EQ(1, foo->entity_count());
  ASSERT_EQ(e1->tags, Tag::tagging_map({{foo, 4}}));
}

TEST_F(EntityAndTagTest, DestroyEntity) {
  ASSERT_EQ(ctx.num_tags(), 4);
  ASSERT_TRUE(e1->add_tag(foo));
  ASSERT_EQ(foo->entity_count(), 1);

  ctx.destroy_tag(e1);

  ASSERT_EQ(ctx.num_tags(), 3);
  ASSERT_EQ(foo->entity_count(), 0);
}

TEST_F(EntityAndTagTest, MatchesQuery) {
  e1->add_tag(foo);
  e2->add_tag(bar);

  auto *q_foo = enforce(build_lit(foo, ALL_REL_MASK));
  auto *q_bar = enforce(build_lit(bar, ALL_REL_MASK));

  auto ret = query(ctx, *q_foo);
  ASSERT_EQ(ret, SET(Tag*, {e1}));

  QueryClauseBin q_or(QueryClauseOr, q_foo, q_bar);
  ret = query(ctx, q_or);
  ASSERT_EQ(ret, SET(Tag*, {e1, e2}));
}

TEST_F(EntityAndTagTest, ParentMatches) {
  // foo's parent is bar, so either
  // "foo" or "bar" should match e1
  e1->add_tag(foo);
  foo->imply(bar);

  auto q_foo = enforce(build_lit(foo, ALL_REL_MASK));
  auto q_bar = enforce(build_lit(bar, ALL_REL_MASK));

  auto ret = query(ctx, *q_foo);
  ASSERT_EQ(SET(Tag*, {e1}), ret);

  ret = query(ctx, *q_bar);
  ASSERT_EQ(SET(Tag*, {e1}), ret);

  delete q_foo;
  delete q_bar;
}

TEST_F(EntityAndTagTest, NumEntities) {
  // foo's parent is bar, so either
  // "foo" or "bar" should match e1
  foo->imply(bar);
  e1->add_tag(foo);

  ASSERT_EQ(1, foo->entity_count());
  ASSERT_EQ(0, bar->entity_count());

  ASSERT_FALSE(e1->remove_tag(bar));
  ASSERT_EQ(1, foo->entity_count());
  ASSERT_EQ(0, bar->entity_count());

  ASSERT_TRUE(e1->remove_tag(foo));
  ASSERT_EQ(0, foo->entity_count());
  ASSERT_EQ(0, bar->entity_count());
}

TEST_F(EntityAndTagTest2, RemovingEdge) {
  e1->add_tag(a);
  e2->add_tag(b);

  a->imply(b);
  b->imply(a);
  c->imply(d);
  d->imply(c);

  a->imply(c);
  b->imply(d);
  a->unimply(c);

  ctx.make_clean();

  // SCCs: {a, b} and {c, d}
  //  a    c
  //  |    |
  //  b -> d

  for(auto node : {a, b, c, d}) {
    auto q = build_lit(node, ALL_REL_MASK);
    ASSERT_EQ(false, ctx.is_dirty());
    ASSERT_EQ(SET(Tag*, {e1, e2}), query(ctx, *q));
    delete q;
  }

  e1->remove_tag(a);
  e2->remove_tag(b);
  e1->add_tag(c);
  e2->add_tag(d);

  for(auto node : {a, b}) {
    auto q = build_lit(node, ALL_REL_MASK);
    ASSERT_EQ(SET(Tag*, {}), query(ctx, *q));
    delete q;
  }
  for(auto node : {c, d}) {
    auto q = build_lit(node, ALL_REL_MASK);
    ASSERT_EQ(SET(Tag*, {e1, e2}), query(ctx, *q));
    delete q;
  }

  e2->remove_tag(d);
  e2->add_tag(b);

  for(auto node : {a, b}) {
    auto q = build_lit(node, ALL_REL_MASK);
    ASSERT_EQ(SET(Tag*, {e2}), query(ctx, *q));
    delete q;
  }
  for(auto node : {c, d}) {
    auto q = build_lit(node, ALL_REL_MASK);
    ASSERT_EQ(SET(Tag*, {e1, e2}), query(ctx, *q));
    delete q;
  }
}

TEST_F(EntityAndRealTagTest, DestroyingCatTags) {
  e1->add_tag(cat);
  ASSERT_EQ(e1->tags, Tag::tagging_map({{cat, 1}}));
  ASSERT_EQ(ctx.num_tags(), 8);

  ctx.destroy_tag(cat);
  ASSERT_EQ(e1->tags, Tag::tagging_map({}));

  ASSERT_EQ(ctx.num_tags(), 7);
}
