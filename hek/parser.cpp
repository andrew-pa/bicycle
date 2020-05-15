#include "parse.h"

std::vector<size_t> parser::parse_fn_args(token t) {
	std::vector<size_t> arg_names;
	if (t.type != token::symbol && t.data != (size_t)symbol_type::open_paren) {
		error(t, "expected open paren for function");
	}
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
	return arg_names;
}

std::shared_ptr<ast::expression> parser::next_basic_expr() {
	auto t = tok->next();
	if (t.type == token::symbol && t.data == (size_t)symbol_type::open_paren) {
		auto inside = this->next_expr();
		auto t = tok->next();
		if (t.type != token::symbol && t.data != (size_t)symbol_type::close_paren) {
			error(t, "expected closing paren");
		}
		return inside;
	}
	else if (t.type == token::identifer) {
		auto name = t.data;

		return std::make_shared<ast::named_value>(name);
	}
	else if (t.type == token::number) {
		return std::make_shared<ast::integer_value>(t.data);
	}
	else if (t.type == token::str) {
		return std::make_shared<ast::str_value>(tok->string_literals[t.data]);
	}
	else if (t.type == token::keyword && t.data == (size_t)keyword_type::true_) {
		return std::make_shared<ast::bool_value>(true);
	}
	else if (t.type == token::keyword && t.data == (size_t)keyword_type::false_) {
		return std::make_shared<ast::bool_value>(false);
	}
	else if (t.type == token::keyword && t.data == (size_t)keyword_type::fn) {
		t = tok->next();
		auto arg_names = this->parse_fn_args(t);
		auto body = this->next_basic_stmt();
		return std::make_shared<ast::fn_value>(arg_names, body);
	}
	else {
		error(t, "expected start of expression");
	}
}

std::shared_ptr<ast::statement> parser::next_basic_stmt() {
	auto t = tok->peek();
	if (t.type == token::symbol && t.data == (size_t)symbol_type::open_brace) {
		tok->next();
		t = tok->peek();
		if (t.type == token::symbol && t.data == (size_t)symbol_type::close_brace) {
			return std::make_shared<ast::block_stmt>(nullptr);
		}
		auto body = this->next_stmt();
		t = tok->next();
		if (t.type != token::symbol && t.data != (size_t)symbol_type::close_brace) {
			error(t, "expected closing brace");
		}
		return std::make_shared<ast::block_stmt>(body);
	}
	else if (t.type == token::keyword) {
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
			t = tok->peek();
			std::optional<size_t> name;
			if (t.type == token::identifer) {
				tok->next();
				name.value() = t.data;
			}
			auto body = this->next_stmt();
			return std::make_shared<ast::loop_stmt>(name, body);
		}
		case keyword_type::break_: {
			tok->next();
			t = tok->peek();
			std::optional<size_t> name;
			if (t.type == token::identifer) {
				tok->next();
				name.value() = t.data;
			}
			return std::make_shared<ast::break_stmt>(name);
		}
		case keyword_type::continue_: {
			tok->next();
			t = tok->peek();
			std::optional<size_t> name;
			if (t.type == token::identifer) {
				tok->next();
				name.value() = t.data;
			}
			return std::make_shared<ast::continue_stmt>(name);
		}
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
		case keyword_type::fn: {
			tok->next();
			t = tok->next();
			if (t.type != token::identifer) {
				error(t, "expected name");
			}
			auto name = t.data;
			t = tok->next();
			auto arg_names = this->parse_fn_args(t);
			auto body = this->next_basic_stmt();
			return std::make_shared<ast::let_stmt>(name, std::make_shared<ast::fn_value>(arg_names, body));
		}
		case keyword_type::true_:
		case keyword_type::false_:
			return std::make_shared<ast::expr_stmt>(this->next_expr());
		default: error(t, "unexpected keyword");
		}
	}
	else {
		return std::make_shared<ast::expr_stmt>(this->next_expr());
	}
}

std::shared_ptr<ast::expression> parser::next_expr() {
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

std::shared_ptr<ast::statement> parser::next_stmt() {
	auto s = this->next_basic_stmt();

	auto t = tok->peek();
	if (t.type == token::symbol && t.data == (size_t)symbol_type::semicolon) {
		tok->next();
		std::shared_ptr<ast::statement> next = nullptr;
		t = tok->peek();
		if (t.type != token::eof && !(t.type == token::symbol && (t.data == (size_t)symbol_type::close_brace || t.data == (size_t)symbol_type::semicolon))) next = this->next_stmt();
		return std::make_shared<ast::seq_stmt>(s, next);
	}

	return s;
}
