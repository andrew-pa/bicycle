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

	struct list_value : public value {
		std::vector<std::shared_ptr<value>> values;

		list_value(std::vector<std::shared_ptr<value>> values = {}) : values(values) {}

		void print(std::ostream& out) {
			out << "[ ";
			for (auto i = 0; i < values.size(); ++i) {
				values[i]->print(out);
				if (i + 1 < values.size()) out << ", ";
			}
			out << " ]";
		}

		bool equal(std::shared_ptr<value> other) {
			auto lv = std::dynamic_pointer_cast<list_value>(other);
			if (lv != nullptr) {
				if (lv->values.size() != values.size()) return false;
				for (auto i = 0; i < values.size(); ++i) {
					if (!values[i]->equal(lv->values[i])) return false;
				}
				return true;
			}
			else return false;
		}
	};

	struct map_value : public value {
		std::map<std::string, std::shared_ptr<value>> values;

		map_value(std::map<std::string, std::shared_ptr<value>> v = {}) : values(v) {}

		void print(std::ostream& out) {
			out << "{ ";
			auto i = values.begin();
			while(i != values.end()) {
				out << i->first << ": ";
				i->second->print(out);
				i++;
				if (i != values.end()) out << ", ";
				else break;
			}
			out << " }";
		}

		bool equal(std::shared_ptr<value> other) {
			auto mv = std::dynamic_pointer_cast<map_value>(other);
			if (mv != nullptr) {
				// reference equality really isn't what we want here
				return &*mv == this;
			}
			return false;
		}
	};

	struct instr {
		virtual void print(std::ostream& out) = 0;
		virtual void exec(struct interpreter*) = 0;
		virtual std::optional<size_t> get_marker_id() { return std::optional<size_t>(); }
	};

	struct marker_instr : public instr {
		size_t id;
		marker_instr(size_t id) : id(id) {}
		void exec(struct interpreter* intp) override { }
		void print(std::ostream& out) override { out << "mark " << id << ":" << std::endl; }
		std::optional<size_t> get_marker_id() override { return id;  }
	};


	struct fn_value : public value {
		std::vector<std::string> arg_names;
		std::vector<std::shared_ptr<instr>> body;

		fn_value(std::vector<std::string> an, std::vector<std::shared_ptr<instr>> body) : arg_names(an), body(body) {}

		void print(std::ostream& out) override {
			out << "fn(";
			for (auto i = 0; i < arg_names.size(); ++i) {
				out << arg_names[i];
				if (i + 1 < arg_names.size()) out << ", ";
			}
			out << ")" << std::endl;
			for (auto i : body) {
				out << "\t";
				i->print(out);
			}
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			return false;
		}
	};

	struct scope {
		std::shared_ptr<scope> parent;
		std::map<std::string, std::shared_ptr<value>> bindings;

		scope(std::shared_ptr<scope> parent) : parent(parent), bindings() {}

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

	struct interpreter {
		std::shared_ptr<scope> current_scope, global_scope;
		size_t pc; std::vector<std::shared_ptr<instr>> code;
		std::stack<std::shared_ptr<value>> stack;

		interpreter(std::shared_ptr<scope> global_scope, std::vector<std::shared_ptr<instr>> code)
			: global_scope(global_scope), current_scope(std::make_shared<scope>(global_scope)), pc(0), stack(), code(code) {}

		void debug_print_state() {
			std::cout << "stack [";
			if(stack.size() > 0) stack.top()->print(std::cout);
			std::cout << "] scope {";
			for (auto b : current_scope->bindings) {
				std::cout << b.first << "=";
				b.second->print(std::cout);
				std::cout << " ";
			}
			std::cout << "} cur instr =";
			code[pc]->print(std::cout);
		}

		std::shared_ptr<value> run() {
			pc = 0;
			while (pc < code.size()) {
				//debug_print_state();
				code[pc]->exec(this);
				pc++;
			}
			if (!stack.empty()) return stack.top();
			else return nullptr;
		}

		void go_to_marker(size_t marker_id, intptr_t offset = -1) {
			for (auto i = pc; i < code.size(); ++i) {
				auto marker = code[i]->get_marker_id();
				if (marker.has_value() && marker.value() == marker_id) {
					pc = i + offset;
					return;
				}
			}
			throw std::runtime_error("unknown marker jump");
		}
	};

	struct discard_instr : public instr {
		void print(std::ostream& out) override { out << "discard" << std::endl; }
		void exec(interpreter* intp) override {
			if(!intp->stack.empty())intp->stack.pop();
		}
	};

	struct duplicate_instr : public instr {
		void print(std::ostream& out) override { out << "duplicate" << std::endl; }
		void exec(interpreter* intp) override {
			intp->stack.push(intp->stack.top());
		}
	};

	struct literal_instr : public instr {
		std::shared_ptr<value> val;
		literal_instr(std::shared_ptr<value> v) : val(v) {}
		void print(std::ostream& out) override { out << "literal "; val->print(out); out << std::endl; }
		void exec(interpreter* intp) override {
			intp->stack.push(val);
		}
	};

	struct get_binding_instr : public instr {
		std::string name;
		get_binding_instr(const std::string& name) : name(name) {}
		void print(std::ostream& out) override { out << "get~" << name << std::endl; }
		void exec(interpreter* intp) override {
			intp->stack.push(intp->current_scope->binding(name));
		}
	};

	struct set_binding_instr : public instr {
		std::string name;
		set_binding_instr(const std::string& name) : name(name) {}
		void print(std::ostream& out) override { out << "set~" << name << std::endl; }
		void exec(interpreter* intp) override {
			intp->current_scope->binding(name, intp->stack.top());
			intp->stack.pop();
		}
	};

	struct bind_instr : public instr {
		std::string name;
		bind_instr(const std::string& name) : name(name) {}
		void print(std::ostream& out) override { out << "bind~" << name << std::endl; }
		void exec(interpreter* intp) override {
			intp->current_scope->bind(name, intp->stack.top());
			intp->stack.pop();
		}
	};

	struct enter_scope_instr : public instr {
		void exec(interpreter* intp) override {
			intp->current_scope = std::make_shared<scope>(intp->current_scope);
		}
		void print(std::ostream& out) override { out << "scope [" << std::endl; }
	};

	struct exit_scope_instr : public instr {
		void exec(interpreter* intp) override {
			intp->current_scope = intp->current_scope->parent;
		}
		void print(std::ostream& out) override { out << "] end scope" << std::endl; }
	};


	struct if_instr : public instr {
		size_t true_branch, false_branch;
		if_instr(size_t t, size_t f) : true_branch(t), false_branch(f) {}
		void exec(interpreter* intp) override {
			auto cond = std::dynamic_pointer_cast<bool_value>(intp->stack.top()); intp->stack.pop();
			if (cond->value) {
				intp->go_to_marker(true_branch);
			}
			else {
				intp->go_to_marker(false_branch);
			}
		}
		void print(std::ostream& out) override { out << "if then " << true_branch << " else " << false_branch << std::endl; }
	};

	struct bin_op_instr : public instr {
		op_type op;
		bin_op_instr(op_type op) : op(op) {}
		void exec(interpreter* intp) override {
			auto& stack = intp->stack;
			if (op <= op_type::div) { // math ops
				auto b = dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
				auto a = dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
				auto value = 0;
				switch (op) {
				case op_type::add: value = a + b; break;
				case op_type::sub: value = a - b; break;
				case op_type::mul: value = a * b; break;
				case op_type::div: value = a / b; break;
				default: throw std::runtime_error("unknown op");
				}
				stack.push(std::make_shared<int_value>(value));
			}
			else if (op == op_type::eq || op == op_type::neq) {
				auto b = stack.top(); stack.pop();
				auto a = stack.top(); stack.pop();
				auto value = a->equal(b);
				if (op == op_type::neq) value = !value;
				stack.push(std::make_shared<bool_value>(value));
			}
			else if (op <= op_type::greater_eq) { // compare ops
			 // for now, we can only compare ints
				auto b = dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
				auto a = dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
				auto value = false;
				switch (op) {
				case op_type::less: value = a < b; break;
				case op_type::less_eq: value = a <= b; break;
				case op_type::greater: value = a > b; break;
				case op_type::greater_eq: value = a >= b; break;
				default: throw std::runtime_error("unknown op");
				}
				stack.push(std::make_shared<bool_value>(value));
			}
			else throw std::runtime_error("unexpected op");
		}
		void print(std::ostream& out) override { out << "bin op "; ast::print_op(op, out); out << std::endl; }
	};

	struct jump_instr : public instr {
		size_t loc;
		jump_instr(size_t loc) : loc(loc) {}
		void exec(interpreter* intp) override {
			intp->pc = loc - 1;
		}
		void print(std::ostream& out) override { out << "jmp " << loc << std::endl; }
	};
	struct jump_to_marker_instr : public instr {
		size_t id;
		jump_to_marker_instr(size_t id) : id(id) {}
		void exec(interpreter* intp) {
			intp->go_to_marker(id);
		}
		void print(std::ostream& out) override { out << "jmp mark " << id << std::endl; }
	};

	struct call_instr : public instr {
		void exec(interpreter* intp) override {
			auto fn = std::dynamic_pointer_cast<fn_value>(intp->stack.top()); intp->stack.pop();
			auto fncx = std::make_shared<scope>(intp->global_scope);
			for (auto an : fn->arg_names) {
				fncx->bind(an, intp->stack.top()); intp->stack.pop();
			}
			interpreter fn_intp(fncx, fn->body);
			auto rv = fn_intp.run();
			if (rv != nullptr) intp->stack.push(rv);
		}
		void print(std::ostream& out) override { out << "call" << std::endl; }
	};

	struct ret_instr : public instr {
		void exec(interpreter* intp) override {
			intp->pc = intp->code.size() + 1;
		}
		void print(std::ostream& out) override { out << "ret" << std::endl; }
	};

	struct get_index_instr : public instr {
		void exec(interpreter* intp) override {
			auto ix = intp->stack.top(); intp->stack.pop();
			auto list = std::dynamic_pointer_cast<list_value>(intp->stack.top()); intp->stack.pop();
			if (list != nullptr) {
				auto i = std::dynamic_pointer_cast<int_value>(ix);
				if (i == nullptr) throw std::runtime_error("expected int index to list");
				intp->stack.push(list->values[i->value]);
				return;
			}
			auto map = std::dynamic_pointer_cast<map_value>(intp->stack.top());
			if (map != nullptr) {
				auto n = std::dynamic_pointer_cast<str_value>(ix);
				if (n == nullptr) throw std::runtime_error("expected string key");
				intp->stack.push(map->values[n->value]);
				return;
			}
			throw std::runtime_error("attempted to index unindexable value");
		}
		void print(std::ostream& out) override { out << "index" << std::endl; }
	};

	struct set_index_instr : public instr {
		void exec(interpreter* intp) override {
			auto v = intp->stack.top(); intp->stack.pop();
			auto ix = intp->stack.top(); intp->stack.pop();
			auto list = std::dynamic_pointer_cast<list_value>(intp->stack.top()); intp->stack.pop();
			if (list != nullptr) {
				auto i = std::dynamic_pointer_cast<int_value>(ix);
				if (i == nullptr) throw std::runtime_error("expected int index to list");
				list->values[i->value] = v;
				return;
			}
			auto map = std::dynamic_pointer_cast<map_value>(intp->stack.top());
			if (map != nullptr) {
				auto n = std::dynamic_pointer_cast<str_value>(ix);
				if (n == nullptr) throw std::runtime_error("expected string key");
				map->values[n->value] = v;
				return;
			}
			throw std::runtime_error("attempted to index unindexable value");
		}
		void print(std::ostream& out) override { out << "set index" << std::endl; }
	};

	struct append_list_instr : public instr {
		void exec(interpreter* intp) override {
			auto v = intp->stack.top(); intp->stack.pop();
			auto list = std::dynamic_pointer_cast<list_value>(intp->stack.top());
			list->values.push_back(v);
		}
		void print(std::ostream& out) override { out << "append" << std::endl; }
	};

	struct get_key_instr : public instr {
		void exec(interpreter* intp) override {
			auto n = std::dynamic_pointer_cast<str_value>(intp->stack.top()); intp->stack.pop();
			if (n == nullptr) throw std::runtime_error("expected string key");
			auto map = std::dynamic_pointer_cast<map_value>(intp->stack.top()); intp->stack.pop();
			intp->stack.push(map->values[n->value]);
		}
		void print(std::ostream& out) override { out << "get key" << std::endl; }
	};

	struct set_key_instr : public instr {
		void exec(interpreter* intp) override {
			auto v = intp->stack.top(); intp->stack.pop();
			auto n = std::dynamic_pointer_cast<str_value>(intp->stack.top()); intp->stack.pop();
			if (n == nullptr) throw std::runtime_error("expected string key");
			auto map = std::dynamic_pointer_cast<map_value>(intp->stack.top());
			map->values[n->value] = v;
		}
		void print(std::ostream& out) override { out << "set key" << std::endl; }
	};

	struct system_instr : public instr {
		std::function<std::shared_ptr<value>(interpreter*)> f;

		system_instr(std::function<std::shared_ptr<value>(interpreter*)> f)
			: f(f) {}


		// Inherited via instr
		virtual void print(std::ostream& out) override {
			out << "system" << std::endl;
		}

		virtual void exec(interpreter* intrp) override {
			f(intrp);
		}

	};

	class analyzer : public ast::stmt_visitor, public ast::expr_visitor {

		std::vector<std::string>* ids;
		std::vector<std::shared_ptr<instr>> instrs;
		size_t next_marker;
		// (name, start - location, end - marker)
		std::vector<std::tuple<std::optional<size_t>, size_t, size_t>> loop_marker_stack;

		inline size_t new_marker() { return ++next_marker;  }
	public:
		analyzer(std::vector<std::string>* ids) : ids(ids), instrs(), next_marker(1) {}

		std::vector<std::shared_ptr<instr>> analyze(std::shared_ptr<ast::statement> code) {
			code->visit(this);
			return instrs;
		}

		// Inherited via stmt_visitor
		virtual void visit(ast::seq_stmt* s) override;
		virtual void visit(ast::block_stmt* s) override;
		virtual void visit(ast::let_stmt* s) override;
		virtual void visit(ast::expr_stmt* s) override;
		virtual void visit(ast::if_stmt* s) override;
		virtual void visit(ast::continue_stmt* s) override;
		virtual void visit(ast::break_stmt* s) override;
		virtual void visit(ast::loop_stmt* s) override;
		virtual void visit(ast::return_stmt* s) override;

		// Inherited via expr_visitor
		virtual void visit(ast::named_value* x) override;
		virtual void visit(ast::integer_value* x) override;
		virtual void visit(ast::str_value* x) override;
		virtual void visit(ast::bool_value* x) override;
		virtual void visit(ast::list_value* x) override;
		virtual void visit(ast::map_value* x) override;
		virtual void visit(ast::binary_op* x) override;
		virtual void visit(ast::index_into* x) override;
		virtual void visit(ast::fn_call* x) override;
		virtual void visit(ast::fn_value* x) override;
	};
}
