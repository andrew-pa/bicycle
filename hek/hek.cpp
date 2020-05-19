
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
#include "parse.h"
#include "eval.h"

// TODO:
// + function definition
// + system functions
// + lists
// + hashmaps
// + multiple files
// - dot operator
// - tests/cmake
// - macros
// - closures

/*

macro for
	[($init:stmt; $check:expr; $incr:stmt) $body:stmt] => {
		$init;
		loop {
			if $check { break; };
			$body;
			$incr;
		};
	};

macro cond
	[{ $test:expr => $body:stmt ; $rest }] => {
		if $test { $body } else cond { $rest }
	},
	[{ else => $body:stmt }] => {
		$body:stmt
	},
	[{ }] => {
		{}
	};

macro list!
	[[ $val:expr, $rest ]] => {
		list_cons($val, list![$rest])
	},
	[[]] => { list_empty() };

macro map!
	[{ $key:id : $val:expr, $rest }] => {
		map_insert(map![$rest], stringify!(key), val)
	},
	[{}] => { map_empty() };

*/


int main() {
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

	auto tk = tokenizer(nullptr);
	while (true) {
		std::string line;
		std::cout << std::endl << ">";
		std::getline(std::cin, line);
		std::istringstream s(line);
		tk.reset(&s);
		ast::printer printer(std::cout, &tk.identifiers, 1);
		parser p(&tk);
		try {
			auto stmt = p.next_stmt();
			stmt->visit(&printer);

			eval::analyzer anl(&tk.identifiers);
			auto code = anl.analyze(stmt);
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
