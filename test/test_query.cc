#include "test_helper.h"
#include "context.h"

static bool debug = true;

class QueryTest : public ::testing::Test {
public:
  Context ctx;
  Entity *e1, *e2;
  Tag *a, *b, *c, *d, *e;

  void SetUp() {
    e1 = ctx.new_entity();
    e2 = ctx.new_entity();
    assert(e1 && e2);

    a = ctx.new_tag();
    b = ctx.new_tag();
    c = ctx.new_tag();
    d = ctx.new_tag();
    e = ctx.new_tag();
  }
};

#define OPTIM_AND_CAST(clause) dynamic_cast<QueryClauseBin*>(optimize(clause))

TEST_F(QueryTest, QueryOrOptims1) {
  // add tag 'b' to e1, it should be first in the or clause
  e1->add_tag(b);
  auto qc = build_or(build_lit(a), build_lit(b));
  qc = OPTIM_AND_CAST(qc);
  ASSERT_TRUE(qc);
  if(debug) qc->debug_print();
  ASSERT_EQ(((QueryClauseLit*)qc->l)->t, b);
  ASSERT_EQ(((QueryClauseLit*)qc->r)->t, a);

  delete qc;
}

TEST_F(QueryTest, QueryOrOptims2) {
  // add tag 'b' to e1, it should be first in the or clause
  e1->add_tag(b);
  auto qc = build_or(build_lit(b), build_lit(a));
  qc = OPTIM_AND_CAST(qc);
  if(debug) qc->debug_print();
  ASSERT_TRUE(qc);
  ASSERT_EQ(((QueryClauseLit*)qc->l)->t, b);
  ASSERT_EQ(((QueryClauseLit*)qc->r)->t, a);

  delete qc;
}

TEST_F(QueryTest, QueryAndOptims1) {
  // add tag 'a' to e1, it should be last in the and clause
  e1->add_tag(a);
  auto qc = build_and(build_lit(a), build_lit(b));
  qc = OPTIM_AND_CAST(qc);
  if(debug) qc->debug_print();
  ASSERT_TRUE(qc);
  ASSERT_EQ(((QueryClauseLit*)qc->l)->t, b);
  ASSERT_EQ(((QueryClauseLit*)qc->r)->t, a);

  delete qc;
}

TEST_F(QueryTest, QueryAndOptims2) {
  // add tag 'a' to e1, it should be last in the and clause
  e1->add_tag(a);
  auto qc = build_and(build_lit(b), build_lit(a));
  qc = OPTIM_AND_CAST(qc);
  if(debug) qc->debug_print();
  ASSERT_TRUE(qc);
  ASSERT_EQ(((QueryClauseLit*)qc->l)->t, b);
  ASSERT_EQ(((QueryClauseLit*)qc->r)->t, a);

  delete qc;
}

TEST_F(QueryTest, NestedOrs) {
  for(int i = 0; i <  5; i++) { ctx.new_entity()->add_tag(a); }
  for(int i = 0; i < 10; i++) { ctx.new_entity()->add_tag(b); }
  for(int i = 0; i < 20; i++) { ctx.new_entity()->add_tag(c); }

  auto query =
    build_or(
      build_lit(a),
      build_or(
        build_lit(c),
        build_lit(b)));

  if(debug) query->debug_print();
  query = OPTIM_AND_CAST(query);
  if(debug) query->debug_print();

  auto left_tag    = (dynamic_cast<QueryClauseBin*>(query->l));
  auto right_query = (dynamic_cast<QueryClauseLit*>(query->r));
  ASSERT_TRUE(left_tag);
  ASSERT_TRUE(right_query);
  delete query;
}

TEST_F(QueryTest, NestedAnds) {
  for(int i = 0; i <  5; i++) { ctx.new_entity()->add_tag(a); }
  for(int i = 0; i < 10; i++) { ctx.new_entity()->add_tag(b); }
  for(int i = 0; i < 20; i++) { ctx.new_entity()->add_tag(c); }

  auto query =
    build_and(
      build_lit(b),
      build_and(
        build_lit(a),
        build_lit(c)));

  if(debug) query->debug_print();
  query = OPTIM_AND_CAST(query);
  if(debug) query->debug_print();

  auto left_tag    = (dynamic_cast<QueryClauseBin*>(query->l));
  auto right_query = (dynamic_cast<QueryClauseLit*>(query->r));
  ASSERT_TRUE(left_tag);
  ASSERT_TRUE(right_query);
  delete query;
}

TEST_F(QueryTest, Implication1) {
  for(int i = 0; i <  5; i++) { ctx.new_entity()->add_tag(a); }
  for(int i = 0; i < 10; i++) { ctx.new_entity()->add_tag(b); }
  for(int i = 0; i < 20; i++) { ctx.new_entity()->add_tag(c); }

  a->imply(b);
  b->imply(a);

  c->imply(d);
  d->imply(c);

  a->imply(c);

  auto query =
    build_and(
      build_lit(b),
      build_and(
        build_lit(a),
        build_lit(c)));

  if(debug) query->debug_print();
  query = OPTIM_AND_CAST(query);
  if(debug) query->debug_print();

  delete query;
}

TEST_F(QueryTest, QueryOptimJITLit) {
  auto query = build_lit(a);

  if(debug) query->debug_print();
  query = dynamic_cast<QueryClause*>(optimize(query, QueryOptFlags_JIT));
  if(debug) query->debug_print();

  ASSERT_FALSE(query->matches_set(e1->tags));
  e1->add_tag(a);
  ASSERT_TRUE(query->matches_set(e1->tags));

  delete query;
}

TEST_F(QueryTest, QueryOptimJITLit_Ors) {
  QueryClause* query = build_or(
    build_lit(a),
    build_lit(b));

  if(debug) query->debug_print();
  query = dynamic_cast<QueryClause*>(optimize(query, QueryOptFlags_JIT));
  if(debug) query->debug_print();

  ASSERT_FALSE(query->matches_set(e1->tags));
  ASSERT_FALSE(query->matches_set(e2->tags));
  e1->add_tag(a);
  ASSERT_TRUE(query->matches_set(e1->tags));
  ASSERT_FALSE(query->matches_set(e2->tags));

  e2->add_tag(b);
  ASSERT_TRUE(query->matches_set(e1->tags));
  ASSERT_TRUE(query->matches_set(e2->tags));

  delete query;
}

TEST_F(QueryTest, QueryOptimJITLit_Ands) {
  QueryClause* query = build_and(
    build_lit(a),
    build_lit(b));

  if(debug) query->debug_print();
  query = dynamic_cast<QueryClause*>(optimize(query, QueryOptFlags_JIT));
  if(debug) query->debug_print();

  ASSERT_FALSE(query->matches_set(e1->tags));
  e1->add_tag(a);
  ASSERT_FALSE(query->matches_set(e1->tags));
  e1->add_tag(b);
  ASSERT_TRUE(query->matches_set(e1->tags));

  delete query;
}
