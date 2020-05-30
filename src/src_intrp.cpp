
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
#include <fstream>

#include "token.h"
#include "ast.h"
#include "parse.h"
#include "eval.h"

/*
	<macro> := macro /name/ <macro-case>, + ;
	<macro-case> := [ <pattern>* ] => { <code> }
	<pattern> := /token/ | $/name/:<type> | $/name/*
*/

/*

macro for:stmt
	($init:stmt; $check:expr; $incr:stmts) $body:stmts => {
		$init;
		loop {
			if $check { break; };
			$body;
			$incr;
		};
	};

macro cond:stmt
	{ $test:expr => $body:stmt ; $rest* } => {
		if $test { $body } else cond { $rest }
	},
	{ else => $body:stmt } => {
		$body:stmt
	},
	{ } => {
		{}
	};

macro mad:expr
	$a:expr + $b:expr * $c:expr => { $a + ($b * $c) };

*/

/*

	mod thing2 {
		// stmts
	}

	fn start() {
		thing1::do_stuff();
		thing2::do_stuff();
	}

	- mod global
	|
	-> mod thing1
	|	-> do_stuff()
	-> mod thing2
	|	-> do_stuff()
	-> start()
*/

std::tuple<bool, std::optional<std::filesystem::path>> process_args(const std::vector<std::string>& args) {
	bool use_repl = false;
	auto file = std::optional<std::filesystem::path>();

	if (args.size() == 0) {
		std::cout << "pass a filename and/or -i to open the REPL" << std::endl;
	}

	for (auto i = 0; i < args.size(); ++i) {
		if (args[i] == "-i") {
			use_repl = true;
		}
		else if(args[i][0] == '-' || file.has_value()) {
			std::cout << "unknown argument " << args[i] << std::endl;
		}
		else {
			file = std::filesystem::path(args[i]).lexically_normal();
		}
	}
	
	return std::tuple{ use_repl, file };
}

void load_file(tokenizer* tok, parser* par, std::shared_ptr<eval::scope> cx, std::filesystem::path path) {
	std::ifstream input_stream(path);
	tok->reset(&input_stream);

	ast::printer printer(std::cout, &tok->identifiers, 0);
	while (!tok->peek().is_eof()) {
		try {
			auto stmt = par->next_stmt();
			stmt->visit(&printer);
			std::cout << std::endl;
			eval::analyzer anl(&tok->identifiers, path.parent_path());
			eval::interpreter intp(cx, anl.analyze(stmt));
			std::cout << std::endl;
			for (auto c : intp.code) c->print(std::cout);
			intp.run();
		}
		catch (const parse_error& pe) {
			std::cout << "parse error: " << pe.what()
				<< " [file= " << path << " line= " << tok->line_number
				<< " token type=" << pe.irritant.type << " data=" << pe.irritant.data;
			if (pe.irritant.type == token::identifer) std::cout << " id=" << tok->identifiers.at(pe.irritant.data);
			else if (pe.irritant.type == token::keyword) {
				auto k = std::find_if(keywords.begin(), keywords.end(), [&](auto p) {
					return p.second == (keyword_type)pe.irritant.data;
				});
				if (k != keywords.end()) std::cout << " kwd=" << k->first;
				else std::cout << " kwd=unk";
			}
			std::cout << "]";
			break;
		}
		catch (const std::runtime_error& e) {
			std::cout << "error: " << e.what() << " in file " << path << std::endl;
		}
	}
}

std::shared_ptr<eval::value> mk_sys_fn(std::initializer_list<std::string>&& args, std::function<void(eval::interpreter* intrp)> f) {
	return std::make_shared<eval::fn_value>(std::vector<std::string>(args),
		std::vector<std::shared_ptr<eval::instr>> {
		std::make_shared<eval::system_instr>(f)
	});
}

struct ios_value : eval::value {
	FILE* f;

	ios_value(const std::string& path, const char* mode): f(nullptr) {
		if (fopen_s(&f, path.c_str(), mode) != 0) {
			throw std::runtime_error("error opening file " + path);
		}
	}

	void print(std::ostream& out) override {
		out << "<filestream@0x" << std::hex << (size_t)f << ">" << std::dec;
	}

	bool equal(std::shared_ptr<value> other) override {
		return false;
	}

	eval::value* clone() override {
		throw std::runtime_error("cannot clone file handle");
	}

	~ios_value() {
		if(f) fclose(f);
	}
};

std::shared_ptr<eval::scope> build_file_api() {
	auto mod = std::make_shared<eval::scope>(nullptr);
	mod->bind("open", mk_sys_fn({"path"}, [](eval::interpreter* intrp) {
		auto path = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("path"));
		intrp->stack.push(std::make_shared<ios_value>(path->value, "r"));
	}));
	mod->bind("next_char", mk_sys_fn({"file"}, [](eval::interpreter* intrp) {
		auto f = std::dynamic_pointer_cast<ios_value>(intrp->current_scope->binding("file"));
		intrp->stack.push(std::make_shared<eval::int_value>(fgetc(f->f)));
	}));
	mod->bind("peek_char", mk_sys_fn({ "file" }, [](eval::interpreter* intrp) {
		auto f = std::dynamic_pointer_cast<ios_value>(intrp->current_scope->binding("file"));
		intrp->stack.push(std::make_shared<eval::int_value>(fgetc(f->f)));
		fseek(f->f, -1, SEEK_CUR);
	}));
	mod->bind("eof", mk_sys_fn({"file"}, [](eval::interpreter* intrp) {
		auto f = std::dynamic_pointer_cast<ios_value>(intrp->current_scope->binding("file"));
		intrp->stack.push(std::make_shared<eval::bool_value>(feof(f->f) != 0));
	}));
	return mod;
}

std::shared_ptr<eval::scope> build_str_api() {
	auto mod = std::make_shared<eval::scope>(nullptr);
	mod->bind("length", mk_sys_fn({ "str" }, [](eval::interpreter* intrp) {
		auto s = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("str"));
		intrp->stack.push(std::make_shared<eval::int_value>(s->value.size()));
	}));
	mod->bind("concat", mk_sys_fn({ "a", "b" }, [](eval::interpreter* intrp) {
		auto a = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("a"));
		auto b = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("b"));
		intrp->stack.push(std::make_shared<eval::str_value>(a->value + b->value));
	}));
	mod->bind("append", mk_sys_fn({ "str", "char" }, [](eval::interpreter* intrp) {
		auto s = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("str"));
		auto c = std::dynamic_pointer_cast<eval::int_value>(intrp->current_scope->binding("char"));
		s->value.append(1, (char)c->value);
		intrp->stack.push(s);
	}));
	return mod;
}

std::shared_ptr<eval::scope> build_list_api() {
	auto mod = std::make_shared<eval::scope>(nullptr);
	mod->bind("length", mk_sys_fn({ "lst" }, [](eval::interpreter* intrp) {
		auto lst = std::dynamic_pointer_cast<eval::list_value>(intrp->current_scope->binding("lst"));
		intrp->stack.push(std::make_shared<eval::int_value>(lst->values.size()));
	}));
	mod->bind("concat", mk_sys_fn({ "a", "b" }, [](eval::interpreter* intrp) {
		auto a = std::dynamic_pointer_cast<eval::list_value>(intrp->current_scope->binding("a"));
		auto b = std::dynamic_pointer_cast<eval::list_value>(intrp->current_scope->binding("b"));
		std::vector<std::shared_ptr<eval::value>> vals;
		vals.insert(vals.end(), a->values.begin(), a->values.end());
		vals.insert(vals.end(), b->values.begin(), b->values.end());
		intrp->stack.push(std::make_shared<eval::list_value>(vals));
	}));
	mod->bind("append", mk_sys_fn({ "lst", "x" }, [](eval::interpreter* intrp) {
		auto s = std::dynamic_pointer_cast<eval::list_value>(intrp->current_scope->binding("lst"));
		auto c = intrp->current_scope->binding("x");
		s->values.push_back(c);
		intrp->stack.push(s);
	}));
	return mod;
}

int main(int argc, char* argv[]) {
	std::vector<std::string> args;
	for (auto i = 1; i < argc; ++i) args.push_back(std::string(argv[i]));
	auto [use_repl, file] = process_args(args);

	auto cx = std::make_shared<eval::scope>(nullptr);
	cx->bind("nil", std::make_shared<eval::nil_value>());
	cx->bind("print", std::make_shared<eval::fn_value>(std::vector<std::string>{ "str" },
		std::vector<std::shared_ptr<eval::instr>> {
			std::make_shared<eval::system_instr>(std::function([](eval::interpreter* intrp) {
				auto v = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("str"));
				std::cout << v->value << std::endl;
			}))
		}));

	cx->bind("printv", std::make_shared<eval::fn_value>(std::vector<std::string>{ "val" },
		std::vector<std::shared_ptr<eval::instr>> {
			std::make_shared<eval::system_instr>(std::function([](eval::interpreter* intrp) {
				auto v = intrp->current_scope->binding("val");
				v->print(std::cout);
				std::cout << std::endl;
			}))
		}));
	cx->modules["file"] = build_file_api();
	cx->modules["str"] = build_str_api();
	cx->modules["list"] = build_list_api();

	tokenizer tk(nullptr);
	parser p(&tk);

	if(file.has_value()) load_file(&tk, &p, cx, file.value());

	if (use_repl) {
		ast::printer printer(std::cout, &tk.identifiers, 1);
		while (true) {
			std::string line;
			std::cout << std::endl << ">";
			std::getline(std::cin, line);
			if (line == "!r") {
				if (file.has_value()) load_file(&tk, &p, cx, file.value());
				continue;
			}
			std::istringstream s(line);
			tk.reset(&s);
			try {
				auto expr = p.next_expr();
				expr->visit(&printer);

				eval::analyzer anl(&tk.identifiers, std::filesystem::current_path());
				auto code = anl.analyze(std::make_shared<ast::return_stmt>(expr));
				std::cout << std::endl;
				for (auto c : code) c->print(std::cout);
				eval::interpreter intp(cx, code);

				std::cout << " = ";
				auto res = intp.run();
				if (res != nullptr) {
					res->print(std::cout);
				}
				else {
					std::cout << "()";
				}
			}
			catch (const parse_error& pe) {
				std::cout << "parse error: " << pe.what()
					<< " [token type=" << pe.irritant.type << " data=" << pe.irritant.data << "]";
			}
			catch (const std::runtime_error& e) {
				std::cout << "error: " << e.what();
			}
		}
	}
}
