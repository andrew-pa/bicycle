
#include "eval.h"
#include <fstream>
#include "intrp_std.h"

/*
	module load table
	uint32 number_of_modules
	string module_path

	code
	uint64 num_instrs
	.. instructions ..
*/

std::vector<std::filesystem::path> load_module_table(char*& buf) {
	uint32_t num_modules = *((uint32_t*)buf); buf += sizeof(uint32_t);
	std::vector<std::filesystem::path> paths;
	for (auto i = 0; i < num_modules; ++i) {
		std::string data(buf);
		paths.push_back(data);
		buf += data.length() + 1;
	}
	return paths;
}

std::vector<std::shared_ptr<eval::instr>> load_code(char*& buf) {
	uint64_t num_instrs = *((uint64_t*)buf); buf += sizeof(uint64_t);
	std::vector<std::shared_ptr<eval::instr>> instrs;
	for (auto i = 0; i < num_instrs; ++i) {
		auto op = *buf; buf += 1;
		switch (op) {
		case 0: /*nop*/ break;
		case 1: instrs.push_back(std::make_shared<eval::discard_instr>()); break;
		case 2: instrs.push_back(std::make_shared<eval::duplicate_instr>()); break;
		case 3: /*literal*/ break;

		case 4: /*get binding*/ break;
		case 5: /*get qual*/ break;
		case 6: /*set bind*/ break;
		case 7: /*bind*/ break;

		case 8: instrs.push_back(std::make_shared<eval::enter_scope_instr>()); break;
		case 9: instrs.push_back(std::make_shared<eval::exit_scope_instr>()); break;
		case 10: /*exit as module*/ break;

		case 11: /*if*/ break;

		case 12: /*bop*/ break;
		case 13: instrs.push_back(std::make_shared<eval::log_not_instr>()); break;

		case 14: /*jmp*/ break;
		case 15: /*marker*/ break;
		case 16: /*jmp to marker*/ break;
		case 17: /*mk closure*/ break;
		case 18: /*call*/ break;
		case 19: instrs.push_back(std::make_shared<eval::ret_instr>()); break;

		case 30: instrs.push_back(std::make_shared<eval::get_index_instr>()); break;
		case 31: instrs.push_back(std::make_shared<eval::set_index_instr>()); break;
		case 32: instrs.push_back(std::make_shared<eval::get_key_instr>()); break;
		case 33: instrs.push_back(std::make_shared<eval::set_key_instr>()); break;
		case 50: instrs.push_back(std::make_shared<eval::append_list_instr>()); break;
		default: throw std::runtime_error("unknown opcode " + std::to_string(op));
		}
	}
}

void load_file(const std::filesystem::path& path, std::shared_ptr<eval::scope> cx) {
	std::ifstream input(path, std::ios::binary);
	std::vector<unsigned char> buf(std::istream_iterator<char>(input), {});
}

int main(int argc, char* argv[]) {
	std::vector<std::string> args;
	for (auto i = 1; i < argc; ++i) args.push_back(std::string(argv[i]));

	auto cx = create_global_std_scope();

	load_file(args[0], cx);

	auto vargs = std::vector<std::shared_ptr<eval::value>>();
	vargs.reserve(args.size());
	for (auto a : args) {
		vargs.push_back(std::make_shared<eval::str_value>(a));
	}

	auto code = std::vector<std::shared_ptr<eval::instr>>();
	code.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::list_value>(vargs)));
	code.push_back(std::make_shared<eval::get_binding_instr>("start"));
	code.push_back(std::make_shared<eval::call_instr>(1));

	eval::interpreter intp(cx, code);
	auto res = std::dynamic_pointer_cast<eval::int_value>(intp.run());
	if (res != nullptr) return res->value;
	else return 0;
}
