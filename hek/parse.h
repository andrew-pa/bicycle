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

class parser {
	void error(token t, const std::string& msg) {
		throw parse_error(t, msg);
	}

	std::vector<size_t> parse_fn_args(token t);

	std::shared_ptr<ast::expression> next_basic_expr();

	std::shared_ptr<ast::statement> next_basic_stmt();
public:
	tokenizer* tok;
	parser(tokenizer* t) : tok(t) {}

	std::shared_ptr<ast::expression> next_expr();

	std::shared_ptr<ast::statement> next_stmt();
};

