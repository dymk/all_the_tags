#include "context.h"

#include <iostream>
#include <string>
#include <sstream>
#include <cassert>

QueryClause *parse(Context& c, const char*& clause_str);

// 0 -> inf are TokID (integer numbers)
// -1 -> -inf are reserved for other tokens
enum Token : int {
    TokInv    = -1,
    TokEnd    = -2,
    TokAnd    = -3,
    TokOr     = -4,
    TokNot    = -5,
};
Token token(const char*& clause_str);

int main(int argc, char** argv) {
    Context context;

    std::string line;
    while(true) {
        std::getline(std::cin, line);
        if(std::cin.eof()) {
            break;
        }

        std::stringstream stream(line);
        std::string cmd;
        int op1 = -1, op2 = -1, op3 = 0;
        bool have_cmd , have_op1, have_op2, have_op3;

        have_cmd = !!(stream >> cmd);
        have_op1 = !!(stream >> op1);
        have_op2 = !!(stream >> op2);
        have_op3 = !!(stream >> op3);

        if(!have_cmd) {
            std::cerr << "no op: " << line << std::endl;
        }
        if(cmd == "tbid") {
            if(have_op1) {
                auto t1 = context.tag_by_id(op1);
                if(t1) {
                    std::cerr << "got tag " << t1->id << " (" << t1 << ")" << std::endl;
                }
                else {
                    std::cerr << "no tag " << op1 << std::endl;
                }
            }
        }
        else
        if(cmd == "ebid") {
            if(have_op1) {
                auto e1 = context.entity_by_id(op1);
                if(e1) {
                    std::cerr << "got ent " << e1->id << " (" << e1 << ")" << std::endl;
                }
                else {
                    std::cerr << "no ent " << op1 << std::endl;
                }
            }
        }
        else
        if(cmd == "tag") {
            std::cerr << "add tag" << std::endl;
            if(have_op1) {
                context.new_tag(op1);
            }
            else {
                context.new_tag();
            }
        }
        else
        if(cmd == "ent") {
            std::cerr << "add ent" << std::endl;
            if(have_op1) {
                context.new_entity(op1);
            }
            else {
                context.new_entity();
            }
        }
        else
        if(cmd == "rmt") {
            std::cerr << "rm tag: ";
            if(have_op1) {
                Tag *t = context.tag_by_id(op1);
                if(t) {
                    context.destroy_tag(t);
                    std::cerr << op1 << std::endl;
                }
                else {
                    std::cerr << "none such" << std::endl;
                }
            }
            else {
                std::cerr << "error: " << line << std::endl;
            }
        }
        else
        if(cmd == "rme") {
            std::cerr << "rm ent: ";
            if(have_op1) {
                Entity *e = context.entity_by_id(op1);
                if(e) {
                    context.destroy_entity(e);
                    std::cerr << op1 << std::endl;
                }
                else {
                    std::cerr << "none such" << std::endl;
                }
            }
            else {
                std::cerr << "error: " << line << std::endl;
            }
        }
        else
        if(cmd == "add" || cmd == "rem") {
            std::cerr << "taggify: ";
            if(have_op1 && have_op2) {
                Tag* tag = context.tag_by_id(op1);
                Entity* ent = context.entity_by_id(op2);

                if(!tag || !ent) {
                    std::cerr << "error: ";
                    if(!tag) {
                        std::cerr << "no tag " << op1 << ", ";
                    }
                    if(!ent) {
                        std::cerr << "no ent " << op2 << ", ";
                    }
                    std::cerr << "line: " << line << std::endl;
                }
                else {
                    if(cmd == "add") {
                        std::cerr << op2 << " gets " << op1;
                        if(have_op3) {
                            std::cerr << " (" << op3 << ")";
                            ent->add_tag(tag, op3);
                        }
                        else {
                            ent->add_tag(tag);
                        }
                        std::cerr << std::endl;
                    }
                    else {
                        std::cerr << op2 << " rems " << op1 << std::endl;
                        ent->remove_tag(tag);
                    }
                }
            }
            else {
                std::cerr << "error: " << line << std::endl;
            }
        }
        else
        if(cmd == "imp" || cmd == "uimp") {
            std::cerr << "implicate: ";
            if(have_op1 && have_op2) {
                Tag* tag1 = context.tag_by_id(op1);
                Tag* tag2 = context.tag_by_id(op2);
                if(!tag1 || !tag2) {
                    std::cerr << "error: ";
                    if(!tag1) {
                        std::cerr << "no tag1 " << op1 << ", ";
                    }
                    if(!tag2) {
                        std::cerr << "no tag2 " << op2 << ", ";
                    }
                    std::cerr << "line: " << line << std::endl;
                }
                else {
                    if(cmd == "imp") {
                        std::cerr << op1 << " implies " << op2 << std::endl;
                        tag1->imply(tag2);
                    }
                    else {
                        std::cerr << op1 << " unimplies " << op2 << std::endl;
                        tag1->unimply(tag2);
                    }
                }
            }
            else {
                std::cerr << "error: " << line << std::endl;
            }
        }
        else
        if(cmd == "mkd") {
            std::cerr << "mark dirty" << std::endl;
            context.mark_dirty();
        }
        else
        if(cmd == "mkc") {
            std::cerr << "make clean" << std::endl;
            context.make_clean();
        }
        else
        if(cmd == "qry") {
            std::string str_clause;
            if(line.length() >= 4) {
                str_clause = line.substr(4, line.length());
            }

            std::cerr << "querying '" << str_clause << "'...";

            const char* c_clause = str_clause.c_str();

            // first digit is optimization flags
            QueryOptFlags flags = (QueryOptFlags) 0;
            if(isdigit(*c_clause)) {
                int tok = token(c_clause);
                assert(tok >= 0);
                flags = (QueryOptFlags) tok;
            }

            QueryClause *clause = parse(context, c_clause);
            if(clause) {
                std::cerr << std::endl << "before opt: " << std::endl;
                clause->debug_print();

                clause = optimize(clause, flags);
                std::cerr << std::endl << "after opt: " << std::endl;
                clause->debug_print();

                context.query(clause, [&](const Entity *e) {
                    std::cerr << e->id << ", ";
                });

                delete clause;
            }
            else {
                std::cerr << "couldn't parse" << std::endl;
            }
        }
    }

    return 0;
}

Token token(const char*& clause_str) {
    while(isspace(*clause_str)) {
        clause_str++;
    }

    if(!*clause_str) {
        return TokEnd;
    }

    switch(*clause_str) {
        case '+': clause_str++; return TokAnd;
        case '|': clause_str++; return TokOr;
        case '!': clause_str++; return TokNot;
    }

    if(isdigit(*clause_str)) {
        int accum = *clause_str - '0';
        int tmp_accum = accum;
        clause_str++;

        while(isdigit(*clause_str)) {
            accum *= 10;
            accum += *clause_str - '0';
            clause_str++;

            // check overflow
            if(tmp_accum > accum) {
                accum = tmp_accum;
                break;
            }
            tmp_accum = accum;
        }

        assert(accum >= 0);
        return (Token)accum;
    }

    return TokInv;
}

QueryClause *parse(Context& c, const char*& clause_str) {
    const char *saved = clause_str;
    Token t1 = token(clause_str);
    QueryClause *sub1 = nullptr, *sub2 = nullptr;

    switch(t1) {
        case TokAnd:
        case TokOr:
            sub1 = parse(c, clause_str);
            sub2 = parse(c, clause_str);
            if(sub1 && sub2) {
                if(t1 == TokAnd) {
                    return build_and(sub1, sub2);
                }
                else {
                    return build_or(sub1, sub2);
                }
            }
            else {
                if(sub1) return sub1;
                if(sub2) return sub2;
                return nullptr;
            }

        case TokNot:
            sub1 = parse(c, clause_str);
            if(sub1) {
                return build_not(sub1);
            }
            else {
                return nullptr;
            }

        case TokEnd:
        case TokInv:
            return nullptr;

        default:
            assert(t1 >= 0);
            Tag *t = c.tag_by_id(t1);
            if(t) {
                std::cerr << "built lit tag for " << t->id << " (" << t << ")" << std::endl;
                return build_lit(t, (int) t1);
            }
            else {
                return nullptr;
            }
    }
    assert(false);
}
