
#include <set>
#include <fstream>
#include "eval.h"
#include "intrp_std.h"

/*
	code
	uint64 num_instrs
	.. instructions ..
*/

std::vector<std::shared_ptr<eval::instr>> load_file(const std::filesystem::path& path);

std::string load_str(char*& buf) {
	std::string data(buf);
	buf += data.length() + 1;
	return data;
}

std::vector<std::filesystem::path> load_module_table(char*& buf) {
	uint32_t num_modules = *((uint32_t*)buf); buf += sizeof(uint32_t);
	std::vector<std::filesystem::path> paths;
	for (auto i = 0; i < num_modules; ++i)
		paths.push_back(load_str(buf));
	return paths;
}

std::vector<std::shared_ptr<eval::instr>> load_code(char*& buf, std::filesystem::path root_path) {
	uint64_t num_instrs = *((uint64_t*)buf); buf += sizeof(uint64_t);
	std::vector<std::shared_ptr<eval::instr>> instrs;
	for (auto i = 0; i < num_instrs; ++i) {
		auto op = *buf; buf += 1;
		switch (op) {
		case 0: /*nop*/ break;
		case 1: instrs.push_back(std::make_shared<eval::discard_instr>()); break;
		case 2: instrs.push_back(std::make_shared<eval::duplicate_instr>()); break;
		case 3: {
			auto type = *buf; buf += 1;
			switch (type) {
			case 0: instrs.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::nil_value>())); break;
			case 1: instrs.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::int_value>(*((uint32_t*)buf)))); buf += sizeof(uint32_t); break;
			case 2: instrs.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::str_value>(load_str(buf)))); break;
			case 3: instrs.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::bool_value>(*buf))); buf += 1; break;
			case 4: instrs.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::list_value>())); break;
			case 5: instrs.push_back(std::make_shared<eval::literal_instr>(std::make_shared<eval::map_value>())); break;
			}
		} break;

		case 4: instrs.push_back(std::make_shared<eval::get_binding_instr>(load_str(buf))); break;
		case 5: {
			std::vector<std::string> path;
			auto size = *buf; buf += 1;
			for (auto i = 0; i < size; ++i)
				path.push_back(load_str(buf));
			instrs.push_back(std::make_shared<eval::get_qualified_binding_instr>(path));
		} break;
		case 6: instrs.push_back(std::make_shared<eval::set_binding_instr>(load_str(buf))); break;
		case 7: instrs.push_back(std::make_shared<eval::bind_instr>(load_str(buf))); break;

		case 8: instrs.push_back(std::make_shared<eval::enter_scope_instr>()); break;
		case 9: instrs.push_back(std::make_shared<eval::exit_scope_instr>()); break;
		case 10: instrs.push_back(std::make_shared<eval::exit_scope_as_new_module_instr>(load_str(buf))); break;

		case 11:
			instrs.push_back(std::make_shared<eval::if_instr>(*((uint32_t*)buf), *(1 + (uint32_t*)buf)));
			buf += 2 * sizeof(uint32_t);
			break;

		case 12: instrs.push_back(std::make_shared<eval::bin_op_instr>((op_type)*buf)); buf += 1; break;
		case 13: instrs.push_back(std::make_shared<eval::log_not_instr>()); break;

		case 14: instrs.push_back(std::make_shared<eval::jump_instr>(*((uint32_t*)buf))); buf += sizeof(uint32_t); break;
		case 15: instrs.push_back(std::make_shared<eval::marker_instr>(*((uint32_t*)buf))); buf += sizeof(uint32_t); break;
		case 16: instrs.push_back(std::make_shared<eval::jump_to_marker_instr>(*((uint32_t*)buf))); buf += sizeof(uint32_t); break;
		case 17: {
			auto anc = *buf; buf += 1;
			std::optional<std::string> name = std::nullopt;
			if ((anc & 0x80) == 0x80) {
				anc ^= 0x80;
				name = load_str(buf);
			}
			std::vector<std::string> arg_names;
			for (auto i = 0; i < anc; ++i) {
				arg_names.push_back(load_str(buf));
			}
			instrs.push_back(std::make_shared<eval::make_closure_instr>(arg_names, load_code(buf, root_path), name));
		} break;
		case 18: instrs.push_back(std::make_shared<eval::call_instr>(*((uint32_t*)buf))); buf += sizeof(uint32_t); break;
		case 19: instrs.push_back(std::make_shared<eval::ret_instr>()); break;

		case 30: instrs.push_back(std::make_shared<eval::get_index_instr>()); break;
		case 31: instrs.push_back(std::make_shared<eval::set_index_instr>()); break;
		case 32: instrs.push_back(std::make_shared<eval::get_key_instr>()); break;
		case 33: instrs.push_back(std::make_shared<eval::set_key_instr>()); break;
		case 50: instrs.push_back(std::make_shared<eval::append_list_instr>()); break;

		case 64: {
			auto inner_import = *buf; buf += 1;
			auto name = load_str(buf);
			auto code = load_file(root_path / (name + ".bcc"));
			if (!inner_import) instrs.push_back(std::make_shared<eval::enter_scope_instr>());
			instrs.insert(instrs.end(),
				std::make_move_iterator(code.begin()),
				std::make_move_iterator(code.end()));
			if (!inner_import) instrs.push_back(std::make_shared<eval::exit_scope_as_new_module_instr>(name));
		} break;
		default: throw std::runtime_error("unknown opcode " + std::to_string(op));
		}
	}
	return instrs;
}

std::vector<std::shared_ptr<eval::instr>> load_file(const std::filesystem::path& path) {
	try {
		std::ifstream input(path, std::ios::binary);
		std::vector<char> buf(std::istream_iterator<char>(input), {});
		char* pbuf = buf.data();
		auto code = load_code(pbuf, path.parent_path());
		return code;
	}
	catch (const std::runtime_error& e) {
		std::cout << "error: " << e.what() << " in file " << path << std::endl;
		exit(-1);
	}
}

int main(int argc, char* argv[]) {
	if (argc < 2) {
		std::cout << "require input bytecode";
		return -1;
	}
	std::vector<std::string> args;
	for (auto i = 1; i < argc; ++i) args.push_back(std::string(argv[i]));

	auto cx = create_global_std_scope();

	auto code = load_file(args[0]);

	auto vargs = std::vector<std::shared_ptr<eval::value>>();
	vargs.reserve(args.size());
	for (auto a : args) {
		vargs.push_back(std::make_shared<eval::str_value>(a));
	}

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
