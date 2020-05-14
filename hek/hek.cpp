
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
// - lists
// - hashmaps
// + multiple files
// - tests/cmake
// - macros

int main() {
	auto cx = std::make_shared<eval::context>(nullptr);
	cx->bind("print", std::make_shared<eval::fn_value>(std::vector<std::string>{ "str" },
		std::make_shared<eval::system_function_body>(std::function([](std::shared_ptr<eval::context> cx) {
			auto v = dynamic_pointer_cast<eval::str_value>(cx->binding("str"));
			std::cout << v->value << std::endl;
			return nullptr;
			}))));
	cx->bind("print_val", std::make_shared<eval::fn_value>(std::vector<std::string>{ "val" },
		std::make_shared<eval::system_function_body>(std::function([](std::shared_ptr<eval::context> cx) {
			auto v = cx->binding("val");
			v->print(std::cout);
			std::cout << std::endl;
			return nullptr;
			}))));
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
			eval::evaluator ev(&tk.identifiers);
			std::cout << std::endl << " = ";
			auto res = ev.exec(cx, stmt);
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
