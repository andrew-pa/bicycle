#pragma once

#include <stack>
#include <functional>
#include <filesystem>
#include "ast.h"

namespace eval {
	struct value {
		virtual void print(std::ostream& out) = 0;
		virtual bool equal(std::shared_ptr<value> other) = 0;
		virtual value* clone() = 0;
	};

	struct nil_value : public value {
		void print(std::ostream& out) override {
			out << "nil";
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			return std::dynamic_pointer_cast<nil_value>(other) != nullptr;
		}
		
		value* clone() override { return new nil_value;  }
	};

	struct str_value : public value {
		std::string value;

		str_value(std::string v) : value(v) {}

		void print(std::ostream& out) override {
			out << "\"" << value << "\"";
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			auto iv = std::dynamic_pointer_cast<str_value>(other);
			if (iv != nullptr) return value == iv->value;
			else return false;
		}

		eval::value* clone() override { return new str_value(value);  }
	};

	struct int_value : public value {
		intptr_t value;

		int_value(intptr_t v) : value(v) {}

		void print(std::ostream& out) override {
			out << value;
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			auto iv = std::dynamic_pointer_cast<int_value>(other);
			auto sv = std::dynamic_pointer_cast<str_value>(other);
			if (iv != nullptr) return value == iv->value;
			else if (sv != nullptr && sv->value.size() == 1) {
				return value == sv->value[0];
			}
			else return false;
		}

		eval::value* clone() override { return new int_value(value); }
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

		eval::value* clone() override { return new bool_value(value); }
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

		eval::value* clone() override {
			std::vector<std::shared_ptr<value>> nv;
			for (auto v : values) {
				nv.push_back(std::shared_ptr<value>(v->clone()));
			}
			return new list_value(nv);
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

		eval::value* clone() override {
			std::map<std::string, std::shared_ptr<value>> nv;
			for (auto v : values) {
				nv[v.first] = std::shared_ptr<value>(v.second->clone());
			}
			return new map_value(nv);
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


	struct scope {
		std::shared_ptr<scope> parent;
		std::map<std::string, std::shared_ptr<value>> bindings;
		std::map<std::string, std::shared_ptr<scope>> modules;

		scope(std::shared_ptr<scope> parent) : parent(parent), bindings(), modules() {}
		scope(std::string name, std::shared_ptr<scope> parent) : parent(parent), bindings(), modules() {}

		std::shared_ptr<value> binding(const std::string& name) {
			auto f = bindings.find(name);
			if (f != bindings.end()) return f->second;
			else if (parent != nullptr) {
				return parent->binding(name);
			}
			else throw std::runtime_error("unbound identifier " + name);
		}

		std::shared_ptr<value> qualified_binding(const std::vector<std::string>& path, int index = 0) {
			if (index == path.size()-1) {
				return binding(path[index]);
			} else {
				auto m = modules.find(path[index]);
				if (m != modules.end()) {
					return m->second->qualified_binding(path, index + 1);
				}
				else if(parent != nullptr) {
					return parent->qualified_binding(path, index);
				}
				else {
					std::string s;
					for (auto p : path) s += p + "|";
					throw std::runtime_error("unbound path: " + s);
				}
			}
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

	struct fn_value : public value {
		std::optional<std::string> name;
		std::vector<std::string> arg_names;
		std::vector<std::shared_ptr<instr>> body;
		std::shared_ptr<scope> closure;

		fn_value(std::vector<std::string> an,
			std::vector<std::shared_ptr<instr>> body,
			std::shared_ptr<scope> c,
			std::optional<std::string> name = std::nullopt) : arg_names(an), body(body), closure(c), name(name) {}

		void print(std::ostream& out) override {
			out << "fn";
			if (name.has_value()) {
				out << " " << name.value() << " ";
			}
			out << "(";
			for (auto i = 0; i < arg_names.size(); ++i) {
				out << arg_names[i];
				if (i + 1 < arg_names.size()) out << ", ";
			}
			out << ")";
			if (closure != nullptr) {
				out << "&";
			}
			out << std::endl;
			for (auto i : body) {
				out << "\t";
				i->print(out);
			}
		}

		bool equal(std::shared_ptr<eval::value> other) override {
			return false;
		}


		eval::value* clone() override {
			return new fn_value(arg_names, body, closure, name);
		}
	};

	struct interpreter {
		std::shared_ptr<scope> current_scope, global_scope;
		size_t pc; std::vector<std::shared_ptr<instr>> code;
		std::stack<std::shared_ptr<value>> stack;

		interpreter(std::shared_ptr<scope> global_scope, std::vector<std::shared_ptr<instr>> code)
			: global_scope(global_scope), current_scope(global_scope), pc(0), stack(), code(code) {}

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
			intp->stack.push(std::shared_ptr<value>(val->clone()));
		}
	};

	struct get_binding_instr : public instr {
		std::string name;
		get_binding_instr(const std::string& name) : name(name) {}
		void print(std::ostream& out) override { out << "get(" << name << ")" << std::endl; }
		void exec(interpreter* intp) override {
			intp->stack.push(intp->current_scope->binding(name));
		}
	};

	struct get_qualified_binding_instr : public instr {
		std::vector<std::string> path;
		get_qualified_binding_instr(const std::vector<std::string>& path) : path(path) {}
		void print(std::ostream& out) override {
			out << "get q(";
			for (auto i = 0; i < path.size(); ++i) {
				out << path[i];
				if (i + 1 < path.size()) out << "::";
			}
			out << ")" << std::endl;
		}
		void exec(interpreter* intp) override {
			intp->stack.push(intp->current_scope->qualified_binding(path));
		}
	};

	struct set_binding_instr : public instr {
		std::string name;
		set_binding_instr(const std::string& name) : name(name) {}
		void print(std::ostream& out) override { out << "set(" << name << ")" << std::endl; }
		void exec(interpreter* intp) override {
			intp->current_scope->binding(name, intp->stack.top());
			intp->stack.pop();
		}
	};

	struct bind_instr : public instr {
		std::string name;
		bind_instr(const std::string& name) : name(name) {}
		void print(std::ostream& out) override { out << "bind(" << name << ")" << std::endl; }
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

	struct exit_scope_as_new_module_instr : public instr {
		std::string name;

		exit_scope_as_new_module_instr(std::string name) : name(name) {}

		void exec(interpreter* intp) override {
			auto parent = intp->current_scope->parent;
			auto exm = parent->modules.find(name);
			if (exm != parent->modules.end()) {
				exm->second->bindings.insert(intp->current_scope->bindings.begin(),
					intp->current_scope->bindings.end());
				exm->second->modules.insert(intp->current_scope->modules.begin(),
					intp->current_scope->modules.end());
			}
			else parent->modules[name] = intp->current_scope;
			intp->current_scope = parent;
		}
		void print(std::ostream& out) override { out << "] new module(" << name << ")" << std::endl; }
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
				auto b = std::dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
				auto a = std::dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
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
				auto b = std::dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
				auto a = std::dynamic_pointer_cast<int_value>(stack.top())->value; stack.pop();
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
			else if (op == op_type::and_l || op == op_type::or_l) {
				auto b = std::dynamic_pointer_cast<bool_value>(stack.top())->value; stack.pop();
				auto a = std::dynamic_pointer_cast<bool_value>(stack.top())->value; stack.pop();
				auto value = false;
				switch (op) {
				case op_type::and_l: value = a && b; break;
				case op_type::or_l:  value = a || b; break;
				default: throw std::runtime_error("unknown op");
				}
				stack.push(std::make_shared<bool_value>(value));
			}
			else throw std::runtime_error("unexpected op");
		}
		void print(std::ostream& out) override { out << "bin op "; ast::print_op(op, out); out << std::endl; }
	};

	struct log_not_instr : public instr {
		void exec(interpreter* intrp) override {
			auto a = std::dynamic_pointer_cast<bool_value>(intrp->stack.top()); intrp->stack.pop();
			intrp->stack.push(std::make_shared<bool_value>(!a->value));
		}
		void print(std::ostream& out) override { out << "notl" << std::endl; }
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

	struct make_closure_instr : public instr {
		std::optional<std::string> name;
		std::vector<std::string> arg_names;
		std::vector<std::shared_ptr<instr>> body;
		make_closure_instr(const std::vector<std::string>& arg_names, 
			const std::vector<std::shared_ptr<instr>>& body, std::optional<std::string> name = std::nullopt)
			: arg_names(arg_names), body(body), name(name) {}

		void print(std::ostream& out) override {
			out << "closure fn"; 
			if (name.has_value()) {
				out << " " << name.value();
			}
			out << "(";
			for (auto i = 0; i < arg_names.size(); ++i) {
				out << arg_names[i];
				if (i + 1 < arg_names.size()) out << ", ";
			}
			out << ")" << std::endl;
			for (auto i : body) {
				out << "\t";
				i->print(out);
			}
			out << std::endl;
		}
		void exec(interpreter* intp) override {
			intp->stack.push(std::make_shared<fn_value>(arg_names, body, intp->current_scope, name));
		}
	};

	struct call_instr : public instr {
		size_t num_args;

		call_instr(size_t xar) : num_args(xar) {}

		void exec(interpreter* intp) override {
			auto fn = std::dynamic_pointer_cast<fn_value>(intp->stack.top()); intp->stack.pop();
			if(fn->name.has_value()) std::cout << "call to " << fn->name.value() << std::endl;
			auto fncx = std::make_shared<scope>(fn->closure == nullptr ? intp->global_scope : fn->closure);
			if (num_args != fn->arg_names.size()) {
				throw std::runtime_error("expected " + std::to_string(fn->arg_names.size()) +
					" arguments but only got " + std::to_string(num_args));
			}
			for (auto an : fn->arg_names) {
				if (intp->stack.empty()) throw std::runtime_error("expected more arguments for fn call, stack bottomed out");
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
			auto top = intp->stack.top(); intp->stack.pop();
			auto list = std::dynamic_pointer_cast<list_value>(top);
			if (list != nullptr) {
				auto i = std::dynamic_pointer_cast<int_value>(ix);
				if (i == nullptr) throw std::runtime_error("expected int index to list");
				intp->stack.push(list->values[i->value]);
				return;
			}
			auto map = std::dynamic_pointer_cast<map_value>(top);
			if (map != nullptr) {
				auto n = std::dynamic_pointer_cast<str_value>(ix);
				if (n == nullptr) throw std::runtime_error("expected string key");
				intp->stack.push(map->values[n->value]);
				return;
			}
			auto str = std::dynamic_pointer_cast<str_value>(top);
			if (str != nullptr) {
				auto i = std::dynamic_pointer_cast<int_value>(ix);
				if (i == nullptr) throw std::runtime_error("expected int index to string");
				intp->stack.push(std::make_shared<int_value>(str->value[i->value]));
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
			if (n == nullptr)
				throw std::runtime_error("expected string key");
			auto map = std::dynamic_pointer_cast<map_value>(intp->stack.top()); intp->stack.pop();
			auto val = map->values.find(n->value);
			intp->stack.push(val == map->values.end() ? std::make_shared<nil_value>() : val->second);
		}
		void print(std::ostream& out) override { out << "get key" << std::endl; }
	};

	struct set_key_instr : public instr {
		void exec(interpreter* intp) override {
			auto v = intp->stack.top(); intp->stack.pop();
			auto n = std::dynamic_pointer_cast<str_value>(intp->stack.top()); intp->stack.pop();
			if (n == nullptr)
				throw std::runtime_error("expected string key");
			auto map = std::dynamic_pointer_cast<map_value>(intp->stack.top());
			map->values[n->value] = v;
		}
		void print(std::ostream& out) override { out << "set key" << std::endl; }
	};

	struct system_instr : public instr {
		std::function<void(interpreter*)> f;

		system_instr(std::function<void(interpreter*)> f)
			: f(f) {}


		// Inherited via instr
		virtual void print(std::ostream& out) override {
			out << "system" << std::endl;
		}

		virtual void exec(interpreter* intrp) override {
			f(intrp);
		}

	};

	std::vector<std::shared_ptr<eval::instr>> load_and_assemble(const std::filesystem::path& path);

	class analyzer : public ast::stmt_visitor, public ast::expr_visitor {

		std::vector<std::string>* ids;
		std::vector<std::shared_ptr<instr>> instrs;
		size_t next_marker;
		// (name, start - location, end - marker)
		std::vector<std::tuple<std::optional<size_t>, size_t, size_t>> loop_marker_stack;
		std::filesystem::path root_path;

		inline size_t new_marker() { return ++next_marker;  }
	public:
		analyzer(std::vector<std::string>* ids, std::filesystem::path root_path)
			: ids(ids), instrs(), next_marker(1), root_path(root_path) {}

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
		virtual void visit(ast::module_stmt* s) override;

		// Inherited via expr_visitor
		virtual void visit(ast::named_value* x) override;
		virtual void visit(ast::qualified_value* x) override;
		virtual void visit(ast::integer_value* x) override;
		virtual void visit(ast::str_value* x) override;
		virtual void visit(ast::bool_value* x) override;
		virtual void visit(ast::list_value* x) override;
		virtual void visit(ast::map_value* x) override;
		virtual void visit(ast::binary_op* x) override;
		virtual void visit(ast::logical_negation* x) override;
		virtual void visit(ast::index_into* x) override;
		virtual void visit(ast::fn_call* x) override;
		virtual void visit(ast::fn_value* x) override;
	};
}
