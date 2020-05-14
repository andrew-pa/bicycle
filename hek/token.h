#pragma once
#include <map>
#include <string>
#include <vector>
#include <optional>

enum class symbol_type {
	open_paren, close_paren,
	open_brace, close_brace,
	semicolon, comma
};

enum class op_type {
	add, sub, mul, div,
	eq, neq, less, greater, less_eq, greater_eq,
	assign,
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
	  { "-", op_type::sub },
	  { "*", op_type::mul },
	  { "/", op_type::div },
	  { "==", op_type::eq },
	  { "!=", op_type::neq },
	  { "<", op_type::less },
	  { ">", op_type::greater },
	  { "<=", op_type::less_eq },
	  { ">=", op_type::greater_eq },
	  { "=", op_type::assign },
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

	token next_in_stream();
public:

	tokenizer(std::istream* input) : _in(input), line_number(0) { }

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


