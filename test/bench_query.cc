#include <hayai.hpp>
#include "test_helper.h"

class BenchQuery : public ::hayai::Fixture
{
public:
  std::unordered_set<Tag*> tags;
  std::unordered_set<QueryClause*> queries;
  Context c;

  Tag *foo, *bar, *baz, *qux;
  QueryClause *q_foo, *q_bar, *q_baz, *q_qux;

  Entity* e1;


  virtual void SetUp() {
    foo = c.new_tag();
    bar = c.new_tag();
    baz = c.new_tag();
    qux = c.new_tag();
    tags = SET(Tag*, {foo, bar, baz, qux});

    // baz implies qux
    // anything tagged with baz will appear in a query for qux
    baz->imply(qux);

    // create a few dummy entities
    for(int i = 0; i < 1000; i++) { c.new_entity(); }

    e1 = c.new_entity();
    e1->add_tag(foo, 1);
    e1->add_tag(bar, 1);

    for(int i = 0; i < 1000; i++) { c.new_entity()->add_tag(baz); }
    for(int i = 0; i < 1000; i++) { c.new_entity()->add_tag(qux); }
    for(int i = 0; i < 1000; i++) { c.new_entity(); }

    // 1 entity with "foo" and "bar"
    // 1000 with baz
    // 1000 with qux
    // 1000 with no tags

    q_foo = build_lit(foo, ALL_REL_MASK);
    q_bar = build_lit(bar, ALL_REL_MASK);
    q_baz = build_lit(baz, ALL_REL_MASK);
    q_qux = build_lit(qux, ALL_REL_MASK);

    queries = SET(QueryClause*, {q_foo, q_bar, q_baz, q_qux});
  }

  virtual void TearDown() {
    for(auto t : queries) {
      delete t;
    }
  }
};

BENCHMARK(BenchQuery, NewEntity500, 10, 50) {
  Context c;
  for(int i = 0; i < 500; i++) { c.new_entity(); }
}
BENCHMARK(BenchQuery, NewTag1000, 10, 50) {
  Context c;

  for(int i = 0; i < 1000; i++) {
    c.new_tag(i);
  }
}

BENCHMARK_F(BenchQuery, DoQuery1, 10, 100) {
  auto set = SET(Entity*, {});

  c.query(q_foo, [&](Entity* e) {
    set.insert(e);
  });

  assert(set == SET(Entity*, {e1}));
}

BENCHMARK_F(BenchQuery, QueryForBaz, 10, 100) {
  int count = 0;
  c.query(q_baz, [&](Entity* e) {
    count++;
  });

  assert(count == 1000);
}

BENCHMARK_F(BenchQuery, QueryForQux, 10, 100) {
  int count = 0;
  c.query(q_qux, [&](Entity* e) {
    count++;
  });

  // matches all with tag qux and baz
  assert(count == 2000);
}

class DeepBenchQuery : public ::hayai::Fixture
{
public:
  std::unordered_set<Tag*> tags;
  std::unordered_set<QueryClause*> queries;
  Context c;

  Tag *tag_c, *tag_b1, *tag_b2, *tag_a;
  QueryClause *query_c, *query_b1, *query_b2, *query_a, *query_c_opt, *query_b1_opt, *query_b2_opt, *query_a_opt;

  Entity* e1;

  virtual void SetUp() {
    tag_c = c.new_tag();
    tag_b1 = c.new_tag();
    tag_b2 = c.new_tag();
    tag_a = c.new_tag();

    tag_a->imply(tag_b1);
    tag_b1->imply(tag_c);
    tag_b2->imply(tag_c);
    // parents -> children
    // c -> b1 -> a
    //   -> b2
    // query for 'c' returns all
    // query for 'a' returns only 'a' tagged
    // query for 'b1 returns 'b1' and 'a'
    // query for 'b2' returns only 'b2'
    // etc

    for(int i = 0; i < 1000; i++) { c.new_entity()->add_tag(tag_c); }
    for(int i = 0; i < 400; i++)  { c.new_entity()->add_tag(tag_b1); }
    for(int i = 0; i < 400; i++)  { c.new_entity()->add_tag(tag_b2); }
    for(int i = 0; i < 400; i++)  { c.new_entity()->add_tag(tag_a);  }

    query_c  = build_lit(tag_c, ALL_REL_MASK);
    query_b1 = build_lit(tag_b1, ALL_REL_MASK);
    query_b2 = build_lit(tag_b2, ALL_REL_MASK);
    query_a  = build_lit(tag_a, ALL_REL_MASK);

    query_c_opt  = optimize(query_c->dup());
    query_b1_opt = optimize(query_b1->dup());
    query_b2_opt = optimize(query_b2->dup());
    query_a_opt  = optimize(query_a->dup());

    tags = SET(Tag*, {tag_c, tag_b1, tag_b2, tag_a});
    queries = SET(QueryClause*, {query_c, query_b1, query_b2, query_a, query_c_opt, query_b1_opt, query_b2_opt, query_a_opt});
  }

  virtual void TearDown() {
    for(auto t : queries) {
      delete t;
    }
  }
};

BENCHMARK_F(DeepBenchQuery, QueryA, 10, 100) {
  int count = 0;
  c.query(query_a, [&](Entity* e) {
    count++;
  });

  // matches all with tag a
  assert(count == 400);
}

BENCHMARK_F(DeepBenchQuery, QueryB1, 10, 100) {
  int count = 0;
  c.query(query_b1, [&](Entity* e) {
    count++;
  });

  // matches all with tag b1 or a
  assert(count == 800);
}

BENCHMARK_F(DeepBenchQuery, QueryB2, 10, 100) {
  int count = 0;
  c.query(query_b2, [&](Entity* e) {
    count++;
  });

  // matches all with tag b2
  assert(count == 400);
}
BENCHMARK_F(DeepBenchQuery, OptQueryB2, 10, 100) {
  int count = 0;
  c.query(query_b2_opt, [&](Entity* e) {
    count++;
  });

  // matches all with tag qux and baz
  assert(count == 400);
}

BENCHMARK_F(DeepBenchQuery, QueryC, 10, 100) {
  int count = 0;
  c.query(query_c, [&](Entity* e) {
    count++;
  });

  // matches all posts
  assert(count == 2200);
}
BENCHMARK_F(DeepBenchQuery, OptQueryC, 10, 100) {
  int count = 0;
  c.query(query_c_opt, [&](Entity* e) {
    count++;
  });

  // matches all posts
  assert(count == 2200);
}
