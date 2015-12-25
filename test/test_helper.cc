#include "test_helper.h"

std::unordered_set<Entity*> query(const Context& c, const QueryClause& query) {
  auto set = SET(Entity*, {});
  c.query(&query, [&](Entity* e) {
    set.insert(e);
  });
  return std::move(set);
}
