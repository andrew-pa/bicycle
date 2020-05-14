#pragma once

#include <stack>
#include <functional>
#include "ast.h"

namespace eval {
	struct value {
		virtual void print(std::ostream& out) = 0;
		virtual bool equal(std::shared_ptr<value> other) = 0;
	};

	struct int_value : public value {
		size_t value;

		int_value(size_t v) : value(v) {}

		void print(std::ostream& out) override {
			out << value;
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			auto iv = std::dynamic_pointer_cast<int_value>(other);
			if (iv != nullptr) return value == iv->value;
			else return false;
		}
	};

	struct str_value : public value {
		std::string value;

		str_value(std::string v) : value(v) {}

		void print(std::ostream& out) override {
			out << value;
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			auto iv = std::dynamic_pointer_cast<str_value>(other);
			if (iv != nullptr) return value == iv->value;
			else return false;
		}
	};

	struct bool_value : public value {
		bool value;

		bool_value(bool v) : value(v) {}

		void print(std::ostream& out) override {
			out << (value ? "true" : "false");
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			auto iv = std::dynamic_pointer_cast<bool_value>(other);
			if (iv != nullptr) return value == iv->value;
			else return false;
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

		bool equal(std::shared_ptr<eval::value> other) override {
			return false;
		}
	};

	struct context {
		std::shared_ptr<context> parent;
		std::map<std::string, std::shared_ptr<value>> bindings;
		bool loop_should_break, should_quit_eval;

		context(std::shared_ptr<context> parent) : parent(parent), bindings(), loop_should_break(false), should_quit_eval(false) {}

		std::shared_ptr<value> binding(const std::string& name) {
			auto f = bindings.find(name);
			if (f != bindings.end()) return f->second;
			else if (parent != nullptr) {
				return parent->binding(name);
			}
			else throw std::runtime_error("unbound identifier " + name);
		}

		void binding(const std::string& name, std::shared_ptr<value> v) {
			auto f = bindings.find(name);
			if (f != bindings.end()) f->second = v;
			else if (parent != nullptr) {
				parent->binding(name, v);
			}
			else throw std::runtime_error("unbound identifier " + name);
		}

		void bind(const std::string& name, std::shared_ptr<value> v) {
			bindings[name] = v;
		}

	};

	class evaluator : public ast::expr_visitor, public ast::stmt_visitor {
		std::shared_ptr<context> ccx;
		std::stack<std::shared_ptr<value>> temps;
		std::vector<std::string>* ids;
	public:
		evaluator(std::vector<std::string>* ids)
			: ids(ids), ccx(nullptr), temps() {}

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
			if (x->op == op_type::assign) {
				auto name = std::dynamic_pointer_cast<ast::named_value>(x->left);
				if (name != nullptr) {
					ccx->binding(ids->at(name->identifier), temps.top());
				}
				else {
					throw std::runtime_error("cannot assign const l-value");
				}
			}
			else {
				x->left->visit(this);
				if (x->op <= op_type::div) { // math ops
					auto a = dynamic_pointer_cast<int_value>(temps.top())->value; temps.pop();
					auto b = dynamic_pointer_cast<int_value>(temps.top())->value; temps.pop();
					auto value = 0;
					switch (x->op) {
					case op_type::add: value = a + b; break;
					case op_type::sub: value = a - b; break;
					case op_type::mul: value = a * b; break;
					case op_type::div: value = a / b; break;
					default: throw std::runtime_error("unknown op");
					}
					temps.push(std::make_shared<int_value>(value));
				}
				else if (x->op == op_type::eq || x->op == op_type::neq) {
					auto a = temps.top(); temps.pop();
					auto b = temps.top(); temps.pop();
					auto value = a->equal(b);
					if (x->op == op_type::neq) value = !value;
					temps.push(std::make_shared<bool_value>(value));
				}
				else if (x->op <= op_type::greater_eq) { // compare ops
				 // for now, we can only compare ints
					auto a = dynamic_pointer_cast<int_value>(temps.top())->value; temps.pop();
					auto b = dynamic_pointer_cast<int_value>(temps.top())->value; temps.pop();
					auto value = false;
					switch (x->op) {
					case op_type::less: value = a < b; break;
					case op_type::less_eq: value = a <= b; break;
					case op_type::greater: value = a > b; break;
					case op_type::greater_eq: value = a >= b; break;
					default: throw std::runtime_error("unknown op");
					}
					temps.push(std::make_shared<bool_value>(value));
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
			if (ccx->should_quit_eval) {
				ccx->should_quit_eval = false;
				return;
			}
			if (s->second != nullptr) s->second->visit(this);
		}
		virtual void visit(ast::block_stmt* s) override {
			if (s->body != nullptr) {
				ccx = make_shared<context>(ccx);
				s->body->visit(this);
				ccx = ccx->parent;
			}
		}
		virtual void visit(ast::let_stmt* s) override {
			s->value->visit(this);
			ccx->bind(ids->at(s->identifer), temps.top()); temps.pop();
		}
		virtual void visit(ast::expr_stmt* s) override {
			auto ss = temps.size();
			s->expr->visit(this);
			if (temps.size() != ss) temps.pop(); // discard result
		}
		virtual void visit(ast::if_stmt* s) override {
			s->condition->visit(this);
			auto cond = std::dynamic_pointer_cast<bool_value>(temps.top()); temps.pop();
			if (cond->value) {
				s->if_true->visit(this);
			}
			else if (s->if_false != nullptr) {
				s->if_false->visit(this);
			}
		}
		virtual void visit(ast::continue_stmt* s) override {
			ccx->should_quit_eval = true;
		}
		virtual void visit(ast::break_stmt* s) override {
			ccx->loop_should_break = true;
			ccx->should_quit_eval = true;
		}
		virtual void visit(ast::return_stmt* s) override {
			ccx->loop_should_break = true;
			ccx->should_quit_eval = true;
			s->expr->visit(this);
		}
		virtual void visit(ast::loop_stmt* s) override {
			while (!ccx->loop_should_break) {
				s->body->visit(this);
			}
			ccx->loop_should_break = false;
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
			if (ev == nullptr) throw std::runtime_error("system_function_body used outside of evaluation");
			ev->exec_system(f);
		}
	};

}
