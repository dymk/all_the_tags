#include <queue>
#include <stack>
#include <functional>

#include "context.h"
#include "tag.h"

struct Tag;

static bool debug = false;

int SCCMetaNode::entity_count() const {
  int sum = 0;
  for(auto t : tags) { sum += t->entity_count(); }
  return sum;
}

Context::~Context() {
  for(auto node : meta_nodes) {
    delete node;
  }
  for(auto pair : id_to_tag) {
    delete pair.second;
  }
  for(auto pair : id_to_entity) {
    delete pair.second;
  }
}

bool path_between(SCCMetaNode *from, SCCMetaNode *to) {
  // do a DFS between 'from' and 'to'

  std::function<bool(SCCMetaNode*)> check = [&](SCCMetaNode *node) {
    if(node == to) return true;
    for(auto child : node->children) {
      if(check(child)) return true;
    }
    return false;
  };

  return check(from);
}

void Context::dirty_tag_imply_dag(Tag* tag, bool gained_imply, Tag* target) {
  // if the metagraph is already stale, then don't do
  // an incremental update of the metagraph
  if(this->recalc_metagraph) return;

  SCCMetaNode
    *tag_mn    = tag->meta_node,
    *target_mn = target->meta_node;

  if(gained_imply) {
    // tag now implies target

    if(!tag_mn || !target_mn) {
      if(!tag_mn) {
        tag->meta_node = tag_mn = new SCCMetaNode();
        tag_mn->tags.insert(tag);
        meta_nodes.insert(tag_mn);
      }
      if(!target_mn) {
        target->meta_node = target_mn = new SCCMetaNode();
        target_mn->tags.insert(target);
        meta_nodes.insert(target_mn);
      }

      assert(tag_mn->add_child(target_mn));
      sink_meta_nodes.erase(tag_mn);

      if(target_mn->children.size() == 0) {
        sink_meta_nodes.insert(target_mn);
      }
      else {
        sink_meta_nodes.erase(target_mn);
      }
    }
    else {
      // both tag and target already have a metanode
      // check if target has a path to tag
      // if it does, collapse the two into a single metanode
      assert(target_mn && tag_mn);

      // all of the SCCs that would be in the cycle to tag_mn
      // (and thus need to be collapsed into a single metanode)
      std::unordered_set<SCCMetaNode*> in_scc;
      std::function<void(SCCMetaNode*)> recurse = [&](SCCMetaNode* node) {
        assert(node);

        // TODO: make this more performant (shouldn't need a path_between call
        // for every check )
        if(path_between(node, tag_mn)) {
          in_scc.insert(node);
          for(auto child : node->children) {
            recurse(child);
          }
        }
      };
      recurse(target_mn);


      if(in_scc.size()) {
        auto tmp_in_scc = in_scc;

        // incoming/outgoing edges that aren't in the SCC set being collapsed
        std::unordered_set<SCCMetaNode*> inedges;
        std::unordered_set<SCCMetaNode*> outedges;

        if(debug) {
          std::cerr << "collapsing " << in_scc.size() << " nodes into one" << std::endl;
        }

        while(tmp_in_scc.size()) {
          auto from = *(tmp_in_scc.begin());
          tmp_in_scc.erase(from);

          if(debug) {
            std::cerr << "collapsing node with tags ";
            from->print_tag_set(std::cerr);
            std::cerr << std::endl;
          }

          for(auto scc : from->parents) {
            if(in_scc.find(scc) == in_scc.end()) {
              // not in the collapse set - add to inedges list
              inedges.insert(scc);
            }
          }
          for(auto scc : from->children) {
            if(in_scc.find(scc) == in_scc.end()) {
              // not in the collapse set - add to inedges list
              outedges.insert(scc);
            }
          }
        }

        if(debug) {
          std::cerr << "inedges: " << inedges.size() << std::endl;
          std::cerr << "outedges: " << outedges.size() << std::endl;
        }

        auto new_scc_node = new SCCMetaNode();

        // transfer all tags into 'new_scc_node'
        for(auto scc : in_scc) {
          for(auto t : scc->tags) {
            if(debug) {
              std::cerr << "transfering tag " << t->id << std::endl;
            }
            t->meta_node = new_scc_node;
            assert(new_scc_node->tags.insert(t).second == true);
          }
        }

        // remove all the other nodes from the graph
        for(auto scc : in_scc) {
          if(debug) {
            std::cerr << "removing from graph: ";
            scc->print_tag_set(std::cerr);
            std::cerr << std::endl;
          }
          scc->tags.clear();
          scc->remove_from_graph();
          sink_meta_nodes.erase(scc);
          meta_nodes.erase(scc);
          delete scc;
        }

        // set up edges to the SCC nodes that had incoming edges from one of
        // the collapsed nodes
        for(auto scc : outedges) {
          new_scc_node->add_child(scc);
        }
        for(auto scc : inedges) {
          scc->add_child(new_scc_node);
        }

        // insert new metanode into graph
        meta_nodes.insert(new_scc_node);
        if(new_scc_node->children.size() == 0) {
          sink_meta_nodes.insert(new_scc_node);
        }
      }
      else {
        // no path between the two, won't create a cycle
        // can safely add link directly between the two
        if(debug) {
          std::cerr << "can add edge directly between tags without collapsing nodes" << std::endl;
        }
        tag_mn->add_child(target_mn);
        sink_meta_nodes.erase(tag_mn);
      }
    }
  }
  else {
    // the two nodes must be part of the DAG if they're having an implication
    // between them removed
    assert(tag_mn);
    assert(target_mn);

    // check if there's a path between target_mn and tag_mn
    // if there is then a cycle was broken
    if(tag_mn == target_mn) {
      this->recalc_metagraph = true;
    }
    else {
      // tags were in different SCCs, simply disconnect the two DAG nodes
      // and update sinks/remove node entirely if possible

      // collect all the outgoing edges from tags in this, if any
      // go to tags that are in the metanode 'target_mn' there's another
      // way to get to 'target_mn'
      bool another_path_to_target = false;
      for(auto tag : tag_mn->tags) {
        for(auto implied : tag->implies) {
          if(implied->meta_node && implied->meta_node == target_mn) {
            another_path_to_target = true;
            goto Louter;
          }
        }
      }
      Louter:

      if(another_path_to_target) {
        // another direct edge to the target; don't remove the DAG edge
        // nop <- is intentional
      }
      else {
        assert(tag_mn->remove_child(target_mn));

        auto check_scc = [&](SCCMetaNode* node) {
          // if node has one tag, no children and no parents, it can be removed
          // from existance
          if(
            node->tags.size() == 1 &&
            node->children.empty() &&
            node->parents.empty()) {
            for(auto tag : node->tags) {
              tag->meta_node = nullptr;
            }

            sink_meta_nodes.erase(node);
            meta_nodes.erase(node);
            delete node;
          }
          else
          if(node->children.empty()) {
            // no children, means this is a sink node
            sink_meta_nodes.insert(node);
          }
        };

        check_scc(tag_mn);
        check_scc(target_mn);
      }
    }
  }
}

void Context::make_clean() {
  if(!this->recalc_metagraph) return;

  this->recalc_metagraph = false;

  // clear metanode for all tags
  for(auto id_tag : id_to_tag) {
    id_tag.second->meta_node = nullptr;
  }

  auto get_new_scc = [&]() {
    if(meta_nodes.empty()) {
      return new SCCMetaNode();
    }
    else {
      auto ret = *(meta_nodes.begin());
      assert(meta_nodes.erase(ret) == 1);
      ret->children.clear();
      ret->parents.clear();
      ret->tags.clear();
      return ret;
    }
  };

  // holds the new DAG of metanodes

  // run Tarjan's SCC algorithm
  struct TarjanWrapper {
    Tag *tag;
    int index;
    int low_link;
    bool on_stack;

    TarjanWrapper(Tag *t) :
      tag(t), index(-1), low_link(-1), on_stack(false) {}
  };

  int index = 0;
  std::vector<TarjanWrapper> tarjan_nodes; // bookkeping area for the tarjan nodes
  std::unordered_map<Tag*, TarjanWrapper*> tag_to_tarjan; // maps tag -> its tarjan node in the vector
  std::stack<TarjanWrapper*> tarjan_stack; // stack required by the algo
  std::stack<SCCMetaNode*> metanode_stack; // stack of metanodes generated by Tarjan's algo (in topological descending order)

  tarjan_nodes.reserve(id_to_tag.size());
  for(auto tag_id : id_to_tag) {
    // if the tag isn't part of the implication graph, don't
    // run SCC algo on it
    auto tag = tag_id.second;
    if(tag->implies.empty() && tag->implied_by.empty()) { continue; }

    if(debug) {
      std::cerr << "tarjan: " << tag->id << " will be in the metagraph" << std::endl;
    }

    tarjan_nodes.push_back(TarjanWrapper(tag));
    auto iter = tarjan_nodes.end(); // iterator to the last element in the vector
    iter--;
    tag_to_tarjan.insert(std::make_pair(tag, &(*iter)));
  }

  std::function<void(TarjanWrapper*)> strongconnect = [&](TarjanWrapper *v) {
    v->index    = index;
    v->low_link = index;
    index++;

    tarjan_stack.push(v);
    v->on_stack = true;

    if(debug) {
      std::cerr <<
        "tag " << v->tag->id <<
        " implies " << v->tag->implies.size() <<
        " others" << std::endl;
    }
    for(auto implied : v->tag->implies) {
      auto mapped = tag_to_tarjan.find(implied);
      assert(mapped != tag_to_tarjan.end());
      TarjanWrapper* w = (*mapped).second;

      if(debug) {
        std::cerr << "(-> " << w->tag->id << ")" << std::endl;
      }

      if(w->index == -1) {
        // successor w not visited, recurse on it
        strongconnect(w);
        v->low_link = std::min(v->low_link, w->low_link);
      }
      else if(w->on_stack) {
        // successor w is in stack S and hence in the current SCC
        v->low_link = std::min(v->low_link, w->index);
      }
    }

    // if v is a root node, pop the stack and generate an SCC
    if(v->low_link == v->index) {
      // start a new SCC
      auto component = get_new_scc();
      assert(component);

      while(true) {
        auto w = tarjan_stack.top();
        tarjan_stack.pop();

        component->tags.insert(w->tag);
        w->tag->meta_node = component;
        w->on_stack = false;

        if(w == v) break;
      }
      metanode_stack.push(component);
    }
  };

  {
    auto b = tarjan_nodes.begin();
    auto e = tarjan_nodes.end();
    while(b != e) {
      auto tarjan_node = &(*b);
      if(tarjan_node->index == -1) {
        strongconnect(tarjan_node);
      }
      b++;
    }
  }

  // destroy the remaining metanodes in the old set
  for(auto node : meta_nodes) {
    delete node;
  }
  meta_nodes.clear();

  if(debug) {
    std::cerr << "tarjan: " << metanode_stack.size() << " metanodes total" << std::endl;
  }

  // set up links between metanodes in the graph
  while(metanode_stack.size()) {
    auto top = metanode_stack.top();
    metanode_stack.pop();
    meta_nodes.insert(top);

    if(debug) {
      std::cerr << "tarjan: linking ";
      top->print_tag_set(std::cerr) << std::endl;
    }

    // top is a source SCC - set up links to successor metanodes
    for(auto tag : top->tags) {
      for(auto implied : tag->implies) {

        if(debug) {
          std::cerr << "tarjan: checking edge " << tag->id << " -> " << implied->id << std::endl;
        }

        auto imn = implied->meta_node;
        assert(imn);
        if(imn != top) {
          auto ret = top->add_child(imn);
          if(debug && ret) {
            std::cerr << "adding SCC edge ";
            top->print_tag_set(std::cerr) << " -> ";
            imn->print_tag_set(std::cerr) << std::endl;
          }
        }
      }
    }
  }

  // identify all the sink metanodes
  sink_meta_nodes.clear();
  for(auto node : meta_nodes) {
    if(node->children.empty()) {
      assert(sink_meta_nodes.insert(node).second);
    }
  }
}

Tag *Context::new_tag_common(id_type id) {
  auto t = new Tag(this, id);
  this->id_to_tag.insert(std::make_pair(id, t));
  return t;
}

Tag* Context::new_tag(id_type id) {
  if(id_to_tag.find(id) != id_to_tag.end()) {
    return nullptr;
  }

  return new_tag_common(id);
}

Tag *Context::new_tag() {
  while(true) {
    id_type id = last_tag_id++;
    if(id_to_tag.find(id) == id_to_tag.end()) {
      return new_tag_common(id);
    }
  }
}

void Context::destroy_tag(Tag *tag) {
  assert(tag->context == this && "tag must be on this context");

  // remove tag from all matching entities
  for(auto&& e : id_to_entity) {
    Entity *entity = e.second;
    entity->remove_tag(tag);
  }

  // remove all impliers
  while(tag->implies.size()) {
    tag->unimply(*(tag->implies.begin()));
  }

  // remove all implied by
  while(tag->implied_by.size()) {
    Tag *implier = *(tag->implied_by.begin());
    implier->unimply(tag);
  }

  assert(id_to_tag.erase(tag->id) == 1);
  delete tag;
}

void Context::destroy_entity(Entity *e) {
  auto found = id_to_entity.find(e->id);
  assert(found != id_to_entity.end());
  assert(found->second == e);

  // remove all the tags from this entity
  while(e->tags.size()) {
    e->remove_tag(e->tags.begin()->tag);
  }

  assert(id_to_entity.erase(e->id) == 1);
  delete e;
}

Entity* Context::new_entity(id_type id) {
  if(id_to_entity.find(id) == id_to_entity.end()) {
    // id not present
    auto e = new Entity(id);
    id_to_entity.insert(std::make_pair(id, e));
    return e;
  }

  return nullptr;
}

Entity* Context::new_entity() {
  // no ID given, keep looping until we find the next available ID
  while(true) {
    auto e = new_entity(last_entity_id++);
    if(e) {
      return e;
    }
  }
}

Tag* Context::tag_by_id(id_type tid) const {
  auto iter = id_to_tag.find(tid);
  if(iter != id_to_tag.end()) {
    return (*iter).second;
  }
  else {
    return nullptr;
  }
}

// entity lookup functions
Entity* Context::entity_by_id(id_type eid) const {
  auto iter = id_to_entity.find(eid);
  if(iter != id_to_entity.end()) {
    return (*iter).second;
  }
  else {
    return nullptr;
  }
}
