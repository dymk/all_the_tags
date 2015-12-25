#ifndef __TEST_HELPER_H__
#define __TEST_HELPER_H__

#include "gtest/gtest.h"
#include "context.h"
#include "query.h"

#define SET(T, ARR...) std::unordered_set<T>(ARR)

std::unordered_set<Entity*> query(const Context& c, const QueryClause& query);

#endif
