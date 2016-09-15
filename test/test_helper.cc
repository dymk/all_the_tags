#include "test_helper.h"

std::unordered_set<Tag*> query(const Context& c, const QueryClause& query) {
  std::unordered_set<Tag*> set;
  c.query(&query, [&](Tag* e) {
    set.insert(e);
  });
  return std::move(set);
}
