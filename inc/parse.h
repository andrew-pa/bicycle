#pragma once

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

#include "token.h"
#include "ast.h"

struct parse_error : public std::runtime_error {
	token irritant;
	parse_error(token t, const std::string& msg) : std::runtime_error(msg), irritant(t) {}
};

struct macro_pattern_token {
	union {
		token pattok;
		size_t named;
	};
	enum type_e {
		exact, named_stmt, named_exact_stmt, named_expr
	} type;

	macro_pattern_token(token t) : pattok(t), type(exact) { }
	macro_pattern_token(type_e type, size_t name) : named(name), type(type) {
		if (type == exact) throw std::runtime_error("!");
	}
};

struct macro_rule {
	std::vector<macro_pattern_token> parts;
	std::vector<token> expanded;
};

struct macro_def {
	std::string name;
	enum type_e {
		stmt, expr
	} type;
	std::vector<macro_rule> rules;
};

class parser {
	void error(token t, const std::string& msg) {
		throw parse_error(t, msg);
	}

	std::vector<size_t> parse_fn_args(token t);

	std::shared_ptr<ast::expression> next_basic_expr();

	std::shared_ptr<ast::statement> next_basic_stmt();
	std::optional<size_t> expr_tok_id, stmt_tok_id;
public:
	tokenizer* tok;
	parser(tokenizer* t) : tok(t) {
		auto ex = std::find(t->identifiers.begin(), t->identifiers.end(), "expr");
		if (ex != t->identifiers.end())
			expr_tok_id = std::distance(t->identifiers.begin(), ex);
		auto st = std::find(t->identifiers.begin(), t->identifiers.end(), "stmt");
		if (st != t->identifiers.end())
			stmt_tok_id = std::distance(t->identifiers.begin(), st);
	}

	std::shared_ptr<ast::expression> next_expr(bool noop = false);

	std::shared_ptr<ast::statement> next_stmt();
};

