#include "tag.h"
#include "context.h"

bool Tag::imply(Tag *other) {
  auto a = other->implied_by.insert(this).second;
  auto b = implies.insert(other).second;
  assert(a == b);

  if(a) context->dirty_tag_imply_dag(this, true, other);

  return a;
}
bool Tag::unimply(Tag *other) {
  auto a = other->implied_by.erase(this) == 1;
  auto b = implies.erase(other) == 1;
  assert(a == b);

  if(a) context->dirty_tag_imply_dag(this, false, other);

  return a;
}
