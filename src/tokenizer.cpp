#include "token.h"
#include <istream>

token tokenizer::next_in_stream() {
	if (!_in) return token(token::eof, 0);
	auto ch = _in->get();
	if (ch < 0)
		return token(token::eof, 0);

	while (isspace(ch)) {
		if (ch == '\n') line_number++;
		ch = _in->get();
		if (ch < 0)
			return token(token::eof, 0);
	}
	


	switch (ch) {
	case '{': return token(symbol_type::open_brace);
	case '}': return token(symbol_type::close_brace);
	case '(': return token(symbol_type::open_paren);
	case ')': return token(symbol_type::close_paren);
	case '[': return token(symbol_type::open_sq);
	case ']': return token(symbol_type::close_sq);
	case ':':
		if (_in->peek() == ':') {
			_in->get();
			return token(symbol_type::double_colon);
		}
		return token(symbol_type::colon);
	case ';': return token(symbol_type::semicolon);
	case ',': return token(symbol_type::comma);
	case '$': return token(symbol_type::dollar);
	case '=': if (_in->peek() == '>') {
		_in->get();
		return token(symbol_type::thick_arrow);
	}
	}

	if (isdigit(ch) || (ch == '-' && isdigit(_in->peek()))) {
		int sign = 1;
		if (ch == '-') {
			ch = _in->get();
			sign = -1;
		}
		intptr_t value = ch - '0';
		while (_in && isdigit(_in->peek())) {
			value = value * 10 + (_in->get() - '0');
		}
		return token(token::number, value*sign);
	}
	else if (ch == '"') {
		std::string str;
		while (_in && _in->peek() != '"') {
			ch = _in->get();
			if (ch == '\\') {
				auto nch = _in->get();
				if (nch < 0) break;
				else if (nch == '\\') ch = '\\';
				else if (nch == 'n') ch = '\n';
				else if (nch == 't') ch = '\t';
				else if (nch == '"') ch = '"';
				str += ch;
			}
			else if (ch > 0) str += ch;
			else break;
		}
		_in->get();
		auto id = string_literals.size();
		string_literals.push_back(str);
		return token(token::str, id);
	}
	else if (!isalnum(ch) && ch != '_') {
		std::string op;
		op += ch;
		while (_in && !isalnum(_in->peek()) && !isspace(_in->peek()) && _in->peek() != ';') {
			ch = _in->get();
			if (ch > 0) op += ch;
			else break;
		}
		auto opt = operators.find(op);
		if (opt != operators.end()) {
			return token(opt->second);
		}
		else {
			throw std::runtime_error("unknown operator " + op);
		}
	}
	else {
		std::string id;
		id += ch;
		while (_in && (isalnum(_in->peek()) || _in->peek() == '_')) {
			ch = _in->get();
			if (ch > 0) id += ch;
			else break;
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