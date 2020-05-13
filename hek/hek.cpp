
#include <iostream>
#include <vector>
#include <stack>
#include <map>
#include <memory>
#include <optional>
#include <exception>
#include <algorithm>
#include <string>
#include <sstream>
#include <functional>

/*
    fn f(x) {
        loop {
            x = x / 3;
            if x == 0 {
                break;
            }
        }
    }
*/

enum class symbol_type {
    open_paren, close_paren,
    open_brace, close_brace,
    semicolon, comma
};

enum class op_type {
    add, eq
};

enum class keyword_type {
    fn, loop, break_, continue_, return_, if_, else_, let, true_, false_
};

struct token {
    enum type_e {
        keyword, number, op, identifer, symbol, eof, str
    } type;

    size_t data;

    token(type_e type, size_t data)
        : type(type), data(data) {}
    token(symbol_type data)
        : type(symbol), data((size_t)data) {}
    token(op_type data)
        : type(op), data((size_t)data) {}
    token(keyword_type data)
        : type(op), data((size_t)data) {}
};

const std::map<std::string, op_type> operators = {
	  { "+", op_type::add },
	  { "=", op_type::eq },
};
const std::map<std::string, keyword_type> keywords = {
	{ "fn", keyword_type::fn },
	{ "let", keyword_type::let },
	{ "loop", keyword_type::loop },
	{ "break", keyword_type::break_ },
	{ "continue", keyword_type::continue_ },
	{ "return", keyword_type::return_ },
	{ "if", keyword_type::if_ },
	{ "else", keyword_type::else_ },
	{ "true", keyword_type::true_ },
	{ "false", keyword_type::false_ },
};

class tokenizer {
public:
    std::vector<std::string> identifiers;
    std::vector<std::string> string_literals;
	size_t line_number;
private:
    std::istream* _in;
    std::optional<token> next_token;

    token next_in_stream() {
        if (!_in) return token(token::eof, 0);
        auto ch = _in->get();
        if (ch < 0) return token(token::eof, 0);

        while (isspace(ch)) {
            if (ch == '\n') line_number++;
            ch = _in->get();
        }

        switch (ch) {
        case '{': return token(symbol_type::open_brace);
        case '}': return token(symbol_type::close_brace);
        case '(': return token(symbol_type::open_paren);
        case ')': return token(symbol_type::close_paren);
        case ';': return token(symbol_type::semicolon);
        case ',': return token(symbol_type::comma);
        }

        if (isdigit(ch)) {
            size_t value = ch - '0';
            while(_in && isdigit(_in->peek())) {
                value = value * 10 + (_in->get() - '0');
            }
            return token(token::number, value);
        } else if (ch == '"') {
            std::string str;
            while (_in && _in->peek() != '"') {
                ch = _in->get();
                if (ch > 0) str += ch;
            }
            _in->get();
            auto id = string_literals.size();
            string_literals.push_back(str);
            return token(token::str, id);
        } else if (!isalnum(ch)) {
            std::string op;
            op += ch;
            while (_in && !isalnum(_in->peek()) && !isspace(_in->peek())) {
                ch = _in->get();
                if (ch > 0) op += ch;
            }
            auto opt = operators.find(op);
            if (opt != operators.end()) {
                return token(opt->second);
            }
            else {
                throw std::runtime_error("unknown operator " + op);
            }
        } else {
            std::string id;
            id += ch;
            while (_in && (isalnum(_in->peek()) || _in->peek() == '_')) {
                ch = _in->get();
                if (ch > 0) id += ch;
            }
            auto kwd = keywords.find(id);
            if (kwd != keywords.end()) {
                return token(token::keyword, (size_t)kwd->second);
            }
            else {
                auto idc = std::find(this->identifiers.begin(), this->identifiers.end(), id);
                size_t index;
                if (idc != this->identifiers.end()) {
                    index = std::distance(this->identifiers.begin(), idc);
                }
                else {
                    index = this->identifiers.size();
                    this->identifiers.push_back(id);
                }
				return token(token::identifer, index);
            }
        }
    }
public:

    tokenizer(std::istream* input) : _in(input), line_number(0) {
    }

    void reset(std::istream* input) {
        _in = input;
        line_number = 0;
        next_token = std::optional<token>();
    }

    token next() {
        if (this->next_token.has_value()) {
            auto tok = this->next_token.value();
            this->next_token.reset();
            return tok;
        }
        else {
            return this->next_in_stream();
        }
    }

    token peek() {
        if (!this->next_token.has_value()) {
            next_token = this->next_in_stream();
        }
		return this->next_token.value();
    }
};

namespace ast {
    class stmt_visitor {
    public:
        virtual void visit(struct seq_stmt* s) = 0;
        virtual void visit(struct block_stmt* s) = 0;
        virtual void visit(struct let_stmt* s) = 0;
        virtual void visit(struct expr_stmt* s) = 0;
        virtual void visit(struct if_stmt* s) = 0;
        virtual void visit(struct continue_stmt* s) = 0;
        virtual void visit(struct break_stmt* s) = 0;
        virtual void visit(struct loop_stmt* s) = 0;
        virtual void visit(struct return_stmt* s) = 0;
    };

    class expr_visitor {
    public:
        virtual void visit(struct named_value* x) = 0;
        virtual void visit(struct integer_value* x) = 0;
        virtual void visit(struct str_value* x) = 0;
        virtual void visit(struct bool_value* x) = 0;
        virtual void visit(struct binary_op* x) = 0;
        virtual void visit(struct fn_call* x) = 0;
        virtual void visit(struct fn_value* x) = 0;
    };

    struct statement {
        virtual void visit(stmt_visitor* v) = 0;
    };

#define stmt_visit_impl void visit(stmt_visitor* v) override { v->visit(this); }

    struct expression {
        virtual void visit(expr_visitor* v) = 0;
    };

#define expr_visit_impl void visit(expr_visitor* v) override { v->visit(this); }

    struct named_value : expression {
        size_t identifier;

        named_value(size_t id) : identifier(id) {}

        expr_visit_impl
    };

    struct integer_value : expression {
        size_t value;

        integer_value(size_t v) : value(v) {}

        expr_visit_impl
    };

    struct bool_value : expression {
        bool value;

        bool_value(bool v) : value(v) {}

        expr_visit_impl
    };

    struct str_value : expression {
        std::string value;

        str_value(const std::string& s) : value(s) {}

		expr_visit_impl
    };

    struct fn_value : expression {
        std::vector<size_t> args;
        std::shared_ptr<statement> body;

        fn_value(std::vector<size_t> args, std::shared_ptr<statement> body)
            : args(args), body(body) {}

		expr_visit_impl
    };

    struct binary_op : expression {
        std::shared_ptr<expression> left, right;
        op_type op;

        binary_op(op_type op, std::shared_ptr<expression> l, std::shared_ptr<expression> r) : op(op), left(l), right(r) {}

        expr_visit_impl
    };

    struct fn_call : expression {
        std::shared_ptr<expression> fn;
        std::vector<std::shared_ptr<expression>> args;

        fn_call(std::shared_ptr<expression> fn, std::vector<std::shared_ptr<expression>> args)
            : fn(fn), args(args) {}

        expr_visit_impl
    };

    struct seq_stmt : statement {
        std::shared_ptr<statement> first, second;
        seq_stmt(std::shared_ptr<statement> first, std::shared_ptr<statement> second) : first(first), second(second) {}

        stmt_visit_impl
    };

    struct block_stmt : statement {
        std::shared_ptr<statement> body;

        block_stmt(std::shared_ptr<statement> body) : body(body) {}

        stmt_visit_impl
    };

    struct let_stmt : statement {
        size_t identifer;
        std::shared_ptr<expression> value;

        let_stmt(size_t id, std::shared_ptr<expression> v) : identifer(id), value(v) {}

        stmt_visit_impl
    };

    struct expr_stmt : statement {
        std::shared_ptr<expression> expr;

        expr_stmt(std::shared_ptr<expression> x) : expr(x) {}

        stmt_visit_impl
    };

    struct return_stmt : statement {
        std::shared_ptr<expression> expr;

        return_stmt(std::shared_ptr<expression> x) : expr(x) {}

        stmt_visit_impl
    };


    struct if_stmt : statement {
        std::shared_ptr<expression> condition;
        std::shared_ptr<statement> if_true;
        std::shared_ptr<statement> if_false;

        if_stmt(std::shared_ptr<expression> cond, std::shared_ptr<statement> if_true, std::shared_ptr<statement> if_false = nullptr)
            : condition(cond), if_true(if_true), if_false(if_false) {}

        stmt_visit_impl
    };

    struct continue_stmt : statement {
        stmt_visit_impl
    };
    struct break_stmt : statement {
        stmt_visit_impl
    };

    struct loop_stmt : statement {
        std::shared_ptr<statement> body;

        loop_stmt(std::shared_ptr<statement> body) : body(body) {}

        stmt_visit_impl
    };

#undef stmt_visit_impl
#undef expr_visit_impl

    class printer : public stmt_visitor, public expr_visitor {
        std::ostream& out;
        std::vector<std::string>* ids;
        size_t indent_level;
        void newline() {
            out << std::endl;
            for (size_t i = 0; i < indent_level; ++i)
                out << "    ";
        }
    public:
        printer(std::ostream& os, std::vector<std::string>* ids, size_t idl = 0) : out(os), ids(ids),
            indent_level(idl) {
            if (idl > 0) {
                for (size_t i = 0; i < indent_level; ++i)
                    out << "    ";
            }
        }

        // Inherited via stmt_visitor
        virtual void visit(seq_stmt* s) override {
            s->first->visit(this);
            out << ";";
            if(s->second != nullptr) {
				newline();
				s->second->visit(this);
            }
        }
        virtual void visit(block_stmt* s) override {
            out << "{";
            indent_level++;
            newline();
            s->body->visit(this);
            indent_level--;
            newline();
            out << "}";
        }
        virtual void visit(let_stmt* s) override {
            out << "let ";
            out << ids->at(s->identifer);
            out << " = ";
            s->value->visit(this);
        }
        virtual void visit(expr_stmt* s) override {
            s->expr->visit(this);
        }
        virtual void visit(if_stmt* s) override {
            out << "if ";
            s->condition->visit(this);
            out << " ";
            s->if_true->visit(this);
            if (s->if_false != nullptr) {
                out << " else ";
                s->if_false->visit(this);
            }
        }
        virtual void visit(continue_stmt* s) override {
            out << "continue";
        }
        virtual void visit(break_stmt* s) override {
            out << "break";
        }
        virtual void visit(loop_stmt* s) override {
            out << "loop ";
            s->body->visit(this);
        }
        virtual void visit(return_stmt* s) override {
            out << "return ";
            s->expr->visit(this);
        }

        // Inherited via expr_visitor
        virtual void visit(named_value* x) override {
            out << ids->at(x->identifier);
        }
        virtual void visit(integer_value* x) override {
            out << x->value;
        }
        virtual void visit(bool_value* x) override {
            out << (x->value ? "true" : "false");
        }
        virtual void visit(str_value* x) override {
            out << "\"" << x->value << "\"";
        }
        virtual void visit(fn_value* x) override {
            out << "fn (";
            for (auto i = 0; i < x->args.size(); ++i) {
                out << ids->at(x->args[i]);
                if(i+1 < x->args.size()) out << ", ";
            }
            out << ") ";
            x->body->visit(this);
        }
        virtual void visit(binary_op* x) override {
            out << "(";
            x->left->visit(this);
            out << " ";
            switch (x->op) {
            case op_type::add: out << "+"; break;
            case op_type::eq: out << "="; break;
            }
            out << " ";
            x->right->visit(this);
            out << ")";
        }
        virtual void visit(fn_call* x) override {
            x->fn->visit(this);
            out << "(";
            for (auto i = 0; i < x->args.size(); ++i) {
                x->args[i]->visit(this);
                if(i+1 < x->args.size()) out << ", ";
            }
            out << ")";
        }
    };
}

struct parse_error : public std::runtime_error {
    token irritant;
    parse_error(token t, const std::string& msg) : std::runtime_error(msg), irritant(t) {}
};

class parser {
    void error(token t, const std::string& msg) {
        throw parse_error(t, msg);
    }

    std::shared_ptr<ast::expression> next_basic_expr() {
        auto t = tok->next();
        if (t.type == token::symbol && t.data == (size_t)symbol_type::open_paren) {
            auto inside = this->next_expr();
            auto t = tok->next();
            if (t.type != token::symbol && t.data != (size_t)symbol_type::close_paren) {
                error(t, "expected closing paren");
            }
            return inside;
        } else if (t.type == token::identifer) {
            auto name = t.data;
            
            return std::make_shared<ast::named_value>(name);
        } else if (t.type == token::number) {
            return std::make_shared<ast::integer_value>(t.data);
        } else if (t.type == token::str) {
            return std::make_shared<ast::str_value>(tok->string_literals[t.data]);
        } else if (t.type == token::keyword && t.data == (size_t)keyword_type::true_) {
            return std::make_shared<ast::bool_value>(true);
        } else if (t.type == token::keyword && t.data == (size_t)keyword_type::false_) {
            return std::make_shared<ast::bool_value>(false);
        } else if (t.type == token::keyword && t.data == (size_t)keyword_type::fn) {
			auto name = std::optional<size_t>();
            t = tok->next();
			if (t.type == token::identifer) {
				name.value() = t.data;
				t = tok->next();
			}
			if (t.type != token::symbol && t.data != (size_t)symbol_type::open_paren) {
				error(t, "expected open paren for function");
			}
			std::vector<size_t> arg_names;
			while (true) {
				t = tok->next();
                if (t.type == token::identifer) {
                    arg_names.push_back(t.data);
                    t = tok->next();
                }
                if (t.type == token::symbol) {
					if (t.data == (size_t)symbol_type::comma) continue;
					else if (t.data == (size_t)symbol_type::close_paren) break;
				}
				error(t, "expected either a comma or closing paren in fn def");
			}
            auto body = this->next_stmt();
            if (name.has_value()) {
            }
            else {
                return std::make_shared<ast::fn_value>(arg_names, body);
            }
		} else {
			error(t, "expected start of expression");
		}
	}

    std::shared_ptr<ast::statement> next_basic_stmt() {
        auto t = tok->peek();
        if (t.type == token::symbol && t.data == (size_t)symbol_type::open_brace) {
            tok->next();
            auto body = this->next_stmt();
            t = tok->next();
            if (t.type != token::symbol && t.data != (size_t)symbol_type::close_brace) {
                error(t, "expected closing brace");
            }
            return std::make_shared<ast::block_stmt>(body);
        } else if (t.type == token::keyword) {
            switch ((keyword_type)t.data) {
            case keyword_type::if_: {
				tok->next();
                auto cond = this->next_expr();
                auto ift = this->next_stmt();
                std::shared_ptr<ast::statement> iff = nullptr;
                t = tok->peek();
                if (t.type == token::keyword && t.data == (size_t)keyword_type::else_) {
                    tok->next();
                    iff = this->next_stmt();
                }
                return std::make_shared<ast::if_stmt>(cond, ift, iff);
            }
            case keyword_type::loop: {
				tok->next();
                auto body = this->next_stmt();
                return std::make_shared<ast::loop_stmt>(body);
            }
            case keyword_type::break_:
				tok->next();
                return std::make_shared<ast::break_stmt>();
            case keyword_type::continue_:
				tok->next();
                return std::make_shared<ast::continue_stmt>();
            case keyword_type::return_:
				tok->next();
                return std::make_shared<ast::return_stmt>(this->next_expr());
            case keyword_type::let: {
                tok->next();
                t = tok->next();
                if (t.type != token::identifer) {
                    error(t, "expected name");
                }
                auto id = t.data;
                t = tok->next();
                if (t.type != token::op && t.data != (size_t)op_type::eq) {
                    error(t, "expected = in let stmt");
                }
                return std::make_shared<ast::let_stmt>(id, this->next_expr());
            }
            case keyword_type::true_:
            case keyword_type::false_:
            case keyword_type::fn: 
                return std::make_shared<ast::expr_stmt>(this->next_expr());
            default: error(t, "unexpected keyword");
            }
        } else {
            return std::make_shared<ast::expr_stmt>(this->next_expr());
        }
    }
public:
    tokenizer* tok;
    parser(tokenizer* t) : tok(t) {}

    std::shared_ptr<ast::expression> next_expr() {
        auto x = this->next_basic_expr();

        auto t = tok->peek();
        if (t.type == token::op) {
            tok->next();
            return std::make_shared<ast::binary_op>((op_type)t.data, x, this->next_expr());
        }
        else if (t.type == token::symbol && t.data == (size_t)symbol_type::open_paren) {
			tok->next();
			std::vector<std::shared_ptr<ast::expression>> args;
			while (true) {
				args.push_back(this->next_expr());
				t = tok->next();
				if (t.type == token::symbol) {
					if (t.data == (size_t)symbol_type::comma) continue;
					else if (t.data == (size_t)symbol_type::close_paren) break;
				}
				error(t, "expected either a comma or closing paren in fn call");
			}
			return std::make_shared<ast::fn_call>(x, args);
		}

        return x;
    }

    std::shared_ptr<ast::statement> next_stmt() {
        auto s = this->next_basic_stmt();

        auto t = tok->peek();
        if (t.type == token::symbol && t.data == (size_t)symbol_type::semicolon) {
            tok->next();
            std::shared_ptr<ast::statement> next = nullptr;
            t = tok->peek();
            if (t.type != token::eof && t.type != token::symbol) next = this->next_stmt();
            return std::make_shared<ast::seq_stmt>(s, next);
        }

        return s;
    }
};

namespace eval {
    struct value {
        virtual void print(std::ostream& out) = 0;
    };

    struct int_value : public value {
        size_t value;

        int_value(size_t v) : value(v) {}

        void print(std::ostream& out) override {
            out << value;
        }
    };

    struct str_value : public value {
        std::string value;

        str_value(std::string v) : value(v) {}

        void print(std::ostream& out) override {
            out << value;
        }
    };

    struct bool_value : public value {
        bool value;

        bool_value(bool v) : value(v) {}

        void print(std::ostream& out) override {
            out << value;
        }
    };

    struct fn_value : public value {
        std::vector<std::string> arg_names;
        std::shared_ptr<ast::statement> body;

        fn_value(std::vector<std::string> an, std::shared_ptr<ast::statement> body) : arg_names(an), body(body) {}

        void print(std::ostream& out) override {
            out << "fn(";
            for (auto i = 0; i < arg_names.size(); ++i) {
                out << arg_names[i];
                if (i + 1 < arg_names.size()) out << ", ";
            }
            out << ")";
        }
    };

    struct context {
        std::shared_ptr<context> parent;
        std::map<std::string, std::shared_ptr<value>> bindings;

        context(std::shared_ptr<context> parent) : parent(parent), bindings() {}

        std::shared_ptr<value> binding(const std::string& name) {
            auto f = bindings.find(name);
            if (f != bindings.end()) return f->second;
            else if(parent != nullptr) {
                return parent->binding(name);
            }
            else throw new std::runtime_error("unbound identifier " + name);
        }

        void binding(const std::string& name, std::shared_ptr<value> v) {
			auto f = bindings.find(name);
			if (f != bindings.end()) f->second = v;
			else if (parent != nullptr) {
				parent->binding(name, v);
			}
			else throw new std::runtime_error("unbound identifier " + name);
        }

        void bind(const std::string& name, std::shared_ptr<value> v) {
            bindings[name] = v;
        }

    };

    class evaluator : public ast::expr_visitor, public ast::stmt_visitor {
        std::shared_ptr<context> ccx;
        std::stack<std::shared_ptr<value>> temps;
        std::vector<std::string> *ids;
        bool loop_should_break, should_quit_eval;
    public:
        evaluator(std::vector<std::string>* ids)
            : ids(ids), ccx(nullptr), temps(), loop_should_break(false), should_quit_eval(false) {}

        std::shared_ptr<value> eval(std::shared_ptr<context> cx, std::shared_ptr<ast::expression> x) {
            ccx = cx;
            temps = std::stack<std::shared_ptr<value>>();
            x->visit(this);
            return temps.top();
        }

        std::shared_ptr<value> exec(std::shared_ptr<context> cx, std::shared_ptr<ast::statement> x) {
            ccx = cx;
            temps = std::stack<std::shared_ptr<value>>();
            x->visit(this);
            return temps.size() > 0 ? temps.top() : nullptr;
        }

        // Inherited via expr_visitor
        virtual void visit(ast::named_value* x) override {
            temps.push(ccx->binding(ids->at(x->identifier)));
        }
        virtual void visit(ast::integer_value* x) override {
            temps.push(std::make_shared<int_value>(x->value));
        }
        virtual void visit(ast::str_value* x) override {
            temps.push(std::make_shared<str_value>(x->value));
        }
        virtual void visit(ast::bool_value* x) override {
            temps.push(std::make_shared<bool_value>(x->value));
        }
        virtual void visit(ast::fn_value* x) override {
            std::vector<std::string> argnames;
            for (auto ai : x->args) {
                argnames.push_back(ids->at(ai));
            }
            temps.push(std::make_shared<fn_value>(argnames, x->body));
        }
        virtual void visit(ast::binary_op* x) override {
            x->right->visit(this);
            if (x->op == op_type::eq) {
                auto name = std::dynamic_pointer_cast<ast::named_value>(x->left);
                if (name != nullptr) {
                    ccx->binding(ids->at(name->identifier), temps.top());
                }
            }
            else {
                x->left->visit(this);
                if (x->op == op_type::add) {
                    auto a = dynamic_pointer_cast<int_value>(temps.top()); temps.pop();
                    auto b = dynamic_pointer_cast<int_value>(temps.top()); temps.pop();
                    temps.push(std::make_shared<int_value>(a->value + b->value));
                }
            }
        }
        virtual void visit(ast::fn_call* x) override {
			x->fn->visit(this);
            auto f = dynamic_pointer_cast<fn_value>(temps.top()); temps.pop();
            ccx = make_shared<context>(ccx);
            for (auto i = 0; i < f->arg_names.size(); ++i) {
                x->args[i]->visit(this);
                ccx->bind(f->arg_names[i], temps.top());
                temps.pop();
            }
            f->body->visit(this);
            ccx = ccx->parent;
        }

        // Inherited via stmt_visitor
        virtual void visit(ast::seq_stmt* s) override {
            s->first->visit(this);
            if (should_quit_eval) {
                should_quit_eval = false;
                return;
            }
            if (s->second != nullptr) s->second->visit(this);
        }
        virtual void visit(ast::block_stmt* s) override {
            ccx = make_shared<context>(ccx);
            s->body->visit(this);
            ccx = ccx->parent;
        }
        virtual void visit(ast::let_stmt* s) override {
            s->value->visit(this);
            ccx->bind(ids->at(s->identifer), temps.top()); temps.pop();
        }
        virtual void visit(ast::expr_stmt* s) override {
            auto ss = temps.size();
            s->expr->visit(this);
            if(temps.size() != ss) temps.pop(); // discard result
        }
        virtual void visit(ast::if_stmt* s) override {
            s->condition->visit(this);
            auto cond = std::dynamic_pointer_cast<bool_value>(temps.top()); temps.pop();
            if (cond->value) {
                s->if_true->visit(this);
            } else if (s->if_false != nullptr) {
                s->if_false->visit(this);
            }
        }
        virtual void visit(ast::continue_stmt* s) override {
            should_quit_eval = true;
        }
        virtual void visit(ast::break_stmt* s) override {
            loop_should_break = true;
            should_quit_eval = true;
        }
        virtual void visit(ast::return_stmt* s) override {
            loop_should_break = true;
            should_quit_eval = true;
            s->expr->visit(this);
        }
        virtual void visit(ast::loop_stmt* s) override {
            while (!loop_should_break) {
                s->body->visit(this);
            }
            loop_should_break = false;
        }

        void exec_system(std::function<std::shared_ptr<value>(std::shared_ptr<context>)>& f) {
            auto v = f(ccx);
            if (v != nullptr) temps.push(v);
        }
    };

    struct system_function_body : public ast::statement {
        std::function<std::shared_ptr<value>(std::shared_ptr<context>)> f;

        system_function_body(std::function<std::shared_ptr<value>(std::shared_ptr<context>)> f)
            : f(f) {}

        virtual void visit(ast::stmt_visitor* v) override {
            auto ev = dynamic_cast<evaluator*>(v);
            if (ev == nullptr) throw new std::runtime_error("system_function_body used outside of evaluation");
            ev->exec_system(f);
        }
    };

}

// TODO:
// - function definition
// + system functions
// - multiple files
// - tests/cmake
// - macros

int main() {
	auto cx = std::make_shared<eval::context>(nullptr);
    cx->bind("print", std::make_shared<eval::fn_value>(std::vector<std::string>{ "str" },
        std::make_shared<eval::system_function_body>(std::function([](std::shared_ptr<eval::context> cx) {
                auto v = dynamic_pointer_cast<eval::str_value>(cx->binding("str"));
                std::cout << v->value << std::endl;
                return nullptr;
            }))));
    cx->bind("print_val", std::make_shared<eval::fn_value>(std::vector<std::string>{ "val" },
        std::make_shared<eval::system_function_body>(std::function([](std::shared_ptr<eval::context> cx) {
                auto v = cx->binding("val");
                v->print(std::cout);
                std::cout << std::endl;
                return nullptr;
            }))));
    auto tk = tokenizer(nullptr);
    while (true) {
        std::string line;
        std::cout << std::endl << ">";
        std::getline(std::cin, line);
        std::istringstream s(line);
        tk.reset(&s);
        ast::printer printer(std::cout, &tk.identifiers, 1);
        parser p(&tk);
        auto stmt = p.next_stmt();
        stmt->visit(&printer);
        eval::evaluator ev(&tk.identifiers);
		std::cout << std::endl << " = ";
        auto res = ev.exec(cx, stmt);
        if (res != nullptr) {
            res->print(std::cout);
        } else {
            std::cout << "()";
        }
	}
}
