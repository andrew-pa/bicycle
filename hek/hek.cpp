
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

// TODO:
// + function definition
// + system functions
// + lists
// + hashmaps
// + multiple files
// + dot operator
// - tests/cmake
// - macros
// - closures

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

std::tuple<bool, std::optional<std::string>> process_args(const std::vector<std::string>& args) {
	bool use_repl = false;
	auto file = std::optional<std::string>();

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
			file = args[i];
		}
	}
	
	return std::tuple{ use_repl, file };
}

void load_file(tokenizer* tok, parser* par, std::shared_ptr<eval::scope> cx, std::string path) {
	std::ifstream input_stream(path);
	tok->reset(&input_stream);

	ast::printer printer(std::cout, &tok->identifiers, 1);
	while (!tok->peek().is_eof()) {
		try {
			auto stmt = par->next_stmt();
			stmt->visit(&printer);
			std::cout << std::endl;
			eval::analyzer anl(&tok->identifiers);
			eval::interpreter intp(cx, anl.analyze(stmt));
			intp.run();
		}
		catch (const parse_error& pe) {
			std::cout << "parse error: " << pe.what()
				<< " [file= " << path << "line= " << tok->line_number
				<<" token type=" << pe.irritant.type << " data=" << pe.irritant.data << "]";
		}
		catch (const std::runtime_error& e) {
			std::cout << "error: " << e.what() << " in file " << path << std::endl;
		}
	}
}

int main(int argc, char* argv[]) {
	std::vector<std::string> args;
	for (auto i = 1; i < argc; ++i) args.push_back(std::string(argv[i]));
	auto [use_repl, file] = process_args(args);

	auto cx = std::make_shared<eval::scope>(nullptr);
	cx->bind("print", std::make_shared<eval::fn_value>(std::vector<std::string>{ "str" },
		std::vector<std::shared_ptr<eval::instr>> {
			std::make_shared<eval::system_instr>(std::function([](eval::interpreter* intrp) {
				auto v = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("str"));
				std::cout << v->value << std::endl;
				return nullptr;
			}))
		}));

	cx->bind("printv", std::make_shared<eval::fn_value>(std::vector<std::string>{ "val" },
		std::vector<std::shared_ptr<eval::instr>> {
			std::make_shared<eval::system_instr>(std::function([](eval::interpreter* intrp) {
				auto v = intrp->current_scope->binding("val");
				v->print(std::cout);
				std::cout << std::endl;
				return nullptr;
			}))
		}));

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

				eval::analyzer anl(&tk.identifiers);
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
