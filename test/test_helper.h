#ifndef __TEST_HELPER_H__
#define __TEST_HELPER_H__

#include "gtest/gtest.h"
#include "all_the_tags/context.h"
#include "all_the_tags/query.h"

#include <memory>

#define SET(T, ARR...) std::unordered_set<T>(ARR)

std::unordered_set<Entity*> query(const Context& c, const QueryClause& query);

#endif
