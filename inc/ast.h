#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

#include "token.h"

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
		virtual void visit(struct module_stmt* s) = 0;
	};

	class expr_visitor {
	public:
		virtual void visit(struct named_value* x) = 0;
		virtual void visit(struct qualified_value* x) = 0;
		virtual void visit(struct integer_value* x) = 0;
		virtual void visit(struct str_value* x) = 0;
		virtual void visit(struct bool_value* x) = 0;
		virtual void visit(struct list_value* x) = 0;
		virtual void visit(struct map_value* x) = 0;
		virtual void visit(struct binary_op* x) = 0;
		virtual void visit(struct logical_negation* x) = 0;
		virtual void visit(struct fn_call* x) = 0;
		virtual void visit(struct index_into* x) = 0;
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

	struct qualified_value : expression {
		std::vector<size_t> path;

		qualified_value(std::vector<size_t> path) : path(path) {}

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

	struct list_value : expression {
		std::vector<std::shared_ptr<expression>> values;

		list_value(std::vector<std::shared_ptr<expression>> values) : values(values) {}

		expr_visit_impl
	};

	struct map_value : expression {
		std::map<size_t, std::shared_ptr<expression>> values;

		map_value(std::map<size_t, std::shared_ptr<expression>> values) : values(values) {}

		expr_visit_impl
	};

	struct logical_negation : expression {
		std::shared_ptr<expression> value;

		logical_negation(std::shared_ptr<expression> v) : value(v) {}

		expr_visit_impl
	};

	static const std::map<op_type, size_t> operator_precendence = {
		{ op_type::add, 14 },
		{ op_type::sub, 14 },
		{ op_type::mul, 15 },
		{ op_type::div, 15 },

		{ op_type::eq, 11 },
		{ op_type::neq, 11 },

		{ op_type::less, 12 },
		{ op_type::greater, 12 },
		{ op_type::less_eq, 12 },
		{ op_type::greater_eq, 12 },

		{ op_type::and_l, 6 },
		{ op_type::or_l,  5 },

		{ op_type::assign, 3 },
		{ op_type::dot, 20 },
	};

	struct binary_op : expression {
		std::shared_ptr<expression> left, right;
		op_type op;

		binary_op(op_type op, std::shared_ptr<expression> l, std::shared_ptr<expression> r) {
			auto rightop = std::dynamic_pointer_cast<ast::binary_op>(r);
			if (rightop != nullptr) {
				auto oppd = ast::operator_precendence.at(op);
				auto ropd = ast::operator_precendence.at(rightop->op);
				if (oppd > ropd) {
					auto a = l, b = rightop->left, c = rightop->right;
					this->op = rightop->op;
					left = std::make_shared<ast::binary_op>(op, a, b);
					right = c;
					return;
				}
				else if(op == rightop->op && op == op_type::dot) {
					this->op = op;
					left = std::make_shared<ast::binary_op>(op, l, rightop->left);
					right = rightop->right;
					return;
				}
			}
			this->op = op;
			left = l; right = r;
		}

		expr_visit_impl
	};

	struct index_into : expression {
		std::shared_ptr<expression> collection;
		std::shared_ptr<expression> index;

		index_into(std::shared_ptr<expression> col, std::shared_ptr<expression> ix) : collection(col), index(ix) {}

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
		std::optional<size_t> name;
		
		continue_stmt(std::optional<size_t> name) : name(name) {}

		stmt_visit_impl
	};
	struct break_stmt : statement {
		std::optional<size_t> name;

		break_stmt(std::optional<size_t> name) : name(name) {}

		stmt_visit_impl
	};

	struct loop_stmt : statement {
		std::optional<size_t> name;
		std::shared_ptr<statement> body;

		loop_stmt(std::optional<size_t> name, std::shared_ptr<statement> body) : name(name), body(body) {}

		stmt_visit_impl
	};

	struct module_stmt : statement {
		size_t name;
		std::shared_ptr<statement> body;
		bool inner_import;

		module_stmt(size_t name, std::shared_ptr<statement> body) : name(name), body(body), inner_import(false) {}
		module_stmt(size_t name, std::shared_ptr<statement> body, bool ii)
			: name(name), body(body), inner_import(ii) {}

		stmt_visit_impl
	};

#undef stmt_visit_impl
#undef expr_visit_impl

	inline void print_op(op_type op, std::ostream& out) {
		switch (op) {
		case op_type::add: out << "+"; break;
		case op_type::sub: out << "-"; break;
		case op_type::mul: out << "*"; break;
		case op_type::div: out << "/"; break;
		case op_type::eq: out << "=="; break;
		case op_type::neq: out << "!="; break;
		case op_type::less: out << "<"; break;
		case op_type::greater: out << ">"; break;
		case op_type::less_eq: out << "<="; break;
		case op_type::greater_eq: out << ">="; break;
		case op_type::and_l: out << "&&"; break;
		case op_type::or_l: out << "||"; break;
		case op_type::not_l: out << "!"; break;
		case op_type::assign: out << "="; break;
		case op_type::dot: out << "."; break;
		}
	}

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

		inline void set_indent_level(size_t l) { indent_level = l; }

		// Inherited via stmt_visitor
		virtual void visit(seq_stmt* s) override {
			s->first->visit(this);
			out << ";";
			if (s->second != nullptr) {
				newline();
				s->second->visit(this);
			}
		}
		virtual void visit(block_stmt* s) override {
			if (s->body == nullptr) {
				out << "{ }";
				return;
			}
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
			if (s->name.has_value()) out << " " << ids->at(s->name.value());
		}
		virtual void visit(break_stmt* s) override {
			out << "break";
			if (s->name.has_value()) out << " " << ids->at(s->name.value());
		}
		virtual void visit(loop_stmt* s) override {
			out << "loop ";
			if (s->name.has_value()) out << ids->at(s->name.value()) << " ";
			s->body->visit(this);
		}
		virtual void visit(return_stmt* s) override {
			out << "return ";
			s->expr->visit(this);
		}
		virtual void visit(module_stmt* s) override {
			out << "mod " << ids->at(s->name);
			if (s->body != nullptr) {
				out << " {";
				indent_level++;
				newline();
				s->body->visit(this);
				indent_level--;
				newline();
				out << "}";
			}
		}

		// Inherited via expr_visitor
		virtual void visit(named_value* x) override {
			out << ids->at(x->identifier);
		}
		virtual void visit(qualified_value* x) override {
			for (auto i = 0; i < x->path.size(); ++i) {
				out << ids->at(x->path[i]);
				if (i + 1 < x->path.size()) out << "::";
			}
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
				if (i + 1 < x->args.size()) out << ", ";
			}
			out << ") ";
			x->body->visit(this);
		}
		virtual void visit(binary_op* x) override {
			out << "(";
			x->left->visit(this);
			out << " ";
			print_op(x->op, out);
			out << " ";
			x->right->visit(this);
			out << ")";
		}
		virtual void visit(logical_negation* x) override {
			out << "!(";
			x->value->visit(this);
			out << ")";
		}
		virtual void visit(index_into* x) override {
			x->collection->visit(this);
			out << "[";
			x->index->visit(this);
			out << "]";
		}
		virtual void visit(fn_call* x) override {
			x->fn->visit(this);
			out << "(";
			for (auto i = 0; i < x->args.size(); ++i) {
				x->args[i]->visit(this);
				if (i + 1 < x->args.size()) out << ", ";
			}
			out << ")";
		}

		// Inherited via expr_visitor
		virtual void visit(list_value* x) override {
			out << "[ ";
			for (auto i = 0; i < x->values.size(); ++i) {
				x->values[i]->visit(this);
				if (i + 1 < x->values.size()) out << ", ";
			}
			out << " ]";
		}

		virtual void visit(map_value* x) override {
			out << "{ ";
			auto i = x->values.begin();
			while(true) {
				out << ids->at(i->first) << ": ";
				i->second->visit(this);
				i++;
				if (i != x->values.end()) out << ", ";
				else break;
			}
			out << " }";
		}
};
}


