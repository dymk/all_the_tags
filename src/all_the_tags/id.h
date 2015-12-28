#ifndef __ID_H__
#define __ID_H__

#include <cstdint>

// 4 billion or so of an object allowed should be enough for anyone!
typedef uint32_t id_type;
typedef uint32_t rel_type;

const uint32_t ALL_REL_MASK = 0xFFFFFFFF;

#endif
