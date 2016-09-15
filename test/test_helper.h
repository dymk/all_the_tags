#ifndef __TEST_HELPER_H__
#define __TEST_HELPER_H__

#include "gtest/gtest.h"
#include "all_the_tags/context.h"
#include "all_the_tags/query.h"

#include <memory>
#include <unordered_set>

#define SET(T, ARR...) std::unordered_set<T>(ARR)

template <typename T>
T* enforce(T* p) {
    assert(p);
    return p;
}

std::unordered_set<Tag*> query(const Context& c, const QueryClause& query);

#endif
