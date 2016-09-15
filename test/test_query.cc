#include "test_helper.h"

static bool debug = true;

class QueryTest : public ::testing::Test {
public:
  Context ctx;
  Tag *e1, *e2;
  Tag *a, *b, *c, *d, *e;

  void SetUp() {
    ASSERT_NE(nullptr, e1 = ctx.new_tag());
    ASSERT_NE(nullptr, e2 = ctx.new_tag());
    ASSERT_NE(nullptr, a = ctx.new_tag());
    ASSERT_NE(nullptr, b = ctx.new_tag());
    ASSERT_NE(nullptr, c = ctx.new_tag());
    ASSERT_NE(nullptr, d = ctx.new_tag());
    ASSERT_NE(nullptr, e = ctx.new_tag());
  }
};

#define OPTIM_AND_CAST(clause) dynamic_cast<QueryClauseBin*>(optimize(clause))

#define TEST_LIT(lit, ent, query, expect, err_str) \
    do { \
      if(ctx.is_dirty()) { ctx.make_clean(); } \
      if(query->matches_set(ent->tags) != expect) { \
        std::cerr << err_str << #lit << std::endl; \
        query->debug_print(); \
      } \
      ASSERT_EQ(query->matches_set(ent->tags), expect); \
    } while(0)

#define TEST_TRUE(lit, ent) \
  do { QueryClause *query = lit; \
    TEST_LIT(lit, ent, query, true, "lit failed: ");   \
    query = optimize(query, QueryOptFlags_Reorder);    \
    TEST_LIT(lit, ent, query, true, "reordered lit failed: "); \
    query = optimize(query, QueryOptFlags_JIT); \
    TEST_LIT(lit, ent, query, true, "jit/reordered lit failed: "); \
    delete query; \
  } while(0)

#define TEST_FALS(lit, ent) \
  do { QueryClause *query = lit; \
    TEST_LIT(lit, ent, query, false, "lit failed: ");   \
    query = optimize(query, QueryOptFlags_Reorder);    \
    TEST_LIT(lit, ent, query, false, "reordered lit failed: "); \
    query = optimize(query, QueryOptFlags_JIT); \
    TEST_LIT(lit, ent, query, false, "jit/reordered lit failed: "); \
    delete query; \
  } while(0)

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
  for(int i = 0; i <  5; i++) { ctx.new_tag()->add_tag(a); }
  for(int i = 0; i < 10; i++) { ctx.new_tag()->add_tag(b); }
  for(int i = 0; i < 20; i++) { ctx.new_tag()->add_tag(c); }

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
  for(int i = 0; i <  5; i++) { ctx.new_tag()->add_tag(a); }
  for(int i = 0; i < 10; i++) { ctx.new_tag()->add_tag(b); }
  for(int i = 0; i < 20; i++) { ctx.new_tag()->add_tag(c); }

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
  for(int i = 0; i <  5; i++) { ctx.new_tag()->add_tag(a); }
  for(int i = 0; i < 10; i++) { ctx.new_tag()->add_tag(b); }
  for(int i = 0; i < 20; i++) { ctx.new_tag()->add_tag(c); }

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
  TEST_FALS(build_lit(a), e1);
  e1->add_tag(a);
  TEST_TRUE(build_lit(a), e1);
}

TEST_F(QueryTest, QueryOptimJITLit_Ors) {
  TEST_FALS(build_or(build_lit(a), build_lit(b)), e1);
  TEST_FALS(build_or(build_lit(a), build_lit(b)), e2);

  e1->add_tag(a);
  TEST_TRUE(build_or(build_lit(a), build_lit(b)), e1);
  TEST_FALS(build_or(build_lit(a), build_lit(b)), e2);

  e2->add_tag(b);
  TEST_TRUE(build_or(build_lit(a), build_lit(b)), e1);
  TEST_TRUE(build_or(build_lit(a), build_lit(b)), e2);

}

TEST_F(QueryTest, QueryOptimJITLit_Ands) {
  TEST_FALS(build_and(build_lit(a), build_lit(b)), e1);
  e1->add_tag(a);
  TEST_FALS(build_and(build_lit(a), build_lit(b)), e1);
  e1->add_tag(b);
  TEST_TRUE(build_and(build_lit(a), build_lit(b)), e1);
}

TEST_F(QueryTest, TestQueryRels) {
  ASSERT_TRUE(a->imply(b));
  ASSERT_TRUE(b->imply(c));

  // e1 has a, b, c with rel 1
  ASSERT_TRUE(e1->add_tag(a, 1));

  // e1 has b, c with rel 2
  ASSERT_TRUE(e1->add_tag(b, 2));

  // e1 has: a(1), b(1, 2), c(1, 2)(implied)
  std::unique_ptr<QueryClause> query;

  TEST_TRUE(build_lit(a, 1), e1);
  TEST_TRUE(build_lit(b, 1), e1);
  TEST_TRUE(build_lit(c, 1), e1);

  TEST_FALS(build_lit(a, 2), e1);
  TEST_TRUE(build_lit(b, 2), e1);
  TEST_TRUE(build_lit(c, 2), e1);

  TEST_FALS(build_lit(a, 4), e1);
  TEST_FALS(build_lit(b, 4), e1);
  TEST_FALS(build_lit(c, 4), e1);

  e->imply(d);
  d->imply(e);
  c->imply(d);

  // e2 has c(4), d(4, 8)(implied), e(4, 8)
  e2->add_tag(c, 4);
  e2->add_tag(e, 8);

  TEST_FALS(build_lit(a), e2);
  TEST_TRUE(build_lit(c, 4), e2);
  TEST_FALS(build_lit(c, 8), e2);
  TEST_TRUE(build_or( build_lit(c, 4), build_lit(c, 8)), e2);
  TEST_FALS(build_and(build_lit(c, 4), build_lit(c, 8)), e2);

  TEST_TRUE(build_lit(d), e2);    TEST_TRUE(build_lit(e), e2);
  TEST_FALS(build_lit(d, 1), e2); TEST_FALS(build_lit(e, 1), e2);
  TEST_FALS(build_lit(d, 2), e2); TEST_FALS(build_lit(e, 2), e2);
  TEST_TRUE(build_lit(d, 4), e2); TEST_TRUE(build_lit(e, 4), e2);
  TEST_TRUE(build_lit(d, 8), e2); TEST_TRUE(build_lit(e, 8), e2);

  e->unimply(d);
  c->unimply(d);
  ASSERT_EQ(d->implied_by, SET(Tag*, {}));

  // e2 has c(4), e(4, 8)
  TEST_TRUE(build_lit(e), e2);
  TEST_FALS(build_lit(d), e2);

  // entity only has to have one of the given relationships for the rel mask
  // on a literal, not both
  TEST_TRUE(build_lit(c, 1|4), e1);
  TEST_TRUE(build_lit(c, 1|4), e2);

  // but an exact rel mask can be specified by AND'ing all the desired
  // rel masks
  TEST_FALS(build_and(build_lit(c, 1), build_lit(c, 4)), e1);
  TEST_FALS(build_and(build_lit(c, 1), build_lit(c, 4)), e2);
}
