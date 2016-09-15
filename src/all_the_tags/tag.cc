#include "all_the_tags/tag.h"
#include "all_the_tags/context.h"

bool Tag::imply(Tag *other) {
  if(this == other) {
    return false;
  }

  auto a = other->implied_by.insert(this).second;
  auto b = implies.insert(other).second;
  assert(a == b);

  if(a) context->dirty_tag_imply_dag(this, true, other);

  assert(this->meta_node());
  assert(other->meta_node());
  return a;
}
bool Tag::unimply(Tag *other) {
  if(this == other) {
    return false;
  }

  auto a = other->implied_by.erase(this) == 1;
  auto b = implies.erase(other) == 1;
  if(a != b) {
    std::cerr << "assertion fail: " << a << ", " << b << std::endl;
    assert(false);
  }

  if(a) context->dirty_tag_imply_dag(this, false, other);

  return a;
}
