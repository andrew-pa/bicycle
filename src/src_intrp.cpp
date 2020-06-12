
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
#include "intrp_std.h"

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

std::tuple<bool, std::optional<std::filesystem::path>, std::vector<std::string>> process_args(const std::vector<std::string>& args) {
	bool use_repl = false;
	auto file = std::optional<std::filesystem::path>();
	auto prog_args = std::vector<std::string>();

	if (args.size() == 0) {
		std::cout << "pass a filename and/or -i to open the REPL" << std::endl;
	}

	for (auto i = 0; i < args.size(); ++i) {
		if (args[i] == "-i") {
			use_repl = true;
		}
		else if (args[i] == "--") {
			for (auto j = i + 1; j < args.size(); ++j)
				prog_args.push_back(args[j]);
			break;
		}
		else if(args[i][0] == '-' || file.has_value()) {
			std::cout << "unknown argument " << args[i] << std::endl;
		}
		else {
			file = std::filesystem::path(args[i]).lexically_normal();
		}
	}
	
	return std::tuple{ use_repl, file, prog_args };
}

void load_file(tokenizer* tok, parser* par, std::shared_ptr<eval::scope> cx, std::filesystem::path path) {
	std::ifstream input_stream(path);
	tok->reset(&input_stream);

	ast::printer printer(std::cout, &tok->identifiers, 0);
	while (!tok->peek().is_eof()) {
		try {
			auto stmt = par->next_stmt();
			//stmt->visit(&printer);
			//std::cout << std::endl;
			eval::analyzer anl(&tok->identifiers, path.parent_path());
			eval::interpreter intp(cx, anl.analyze(stmt));
			//std::cout << std::endl;
			//for (auto c : intp.code) c->print(std::cout);
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


int main(int argc, char* argv[]) {
	std::vector<std::string> args;
	for (auto i = 1; i < argc; ++i) args.push_back(std::string(argv[i]));
	auto [use_repl, file, prog_args] = process_args(args);

	tokenizer tk(nullptr);
	parser p(&tk);

	auto cx = create_global_std_scope();

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
			if (line == "!q") {
				break;
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
	} else if(file.has_value()) {
		auto vargs = std::vector<std::shared_ptr<eval::value>>();
		vargs.reserve(prog_args.size() + 1);
		vargs.push_back(std::make_shared<eval::str_value>(file.value().u8string()));
		for (auto a : prog_args) {
			vargs.push_back(std::make_shared<eval::str_value>(a));
		}

		std::vector<std::shared_ptr<eval::instr>> code;
		code.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::list_value>(vargs)));
		code.push_back(std::make_shared<eval::get_binding_instr>("start"));
		code.push_back(std::make_shared<eval::call_instr>(1));

		eval::interpreter intp(cx, code);
		try {
			auto res = std::dynamic_pointer_cast<eval::int_value>(intp.run());
			if (res != nullptr) return res->value;
			else return 0;
		}
		catch (const std::runtime_error& e) {
			std::cout << "error in start: " << e.what() << std::endl;
			return -1;
		}
	}
}
