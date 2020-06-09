#include "..\inc\intrp_std.h"
#include "intrp_std.h"

std::shared_ptr<eval::value> mk_sys_fn(std::initializer_list<std::string>&& args, std::function<void(eval::interpreter* intrp)> f) {
	return std::make_shared<eval::fn_value>(std::vector<std::string>(args),
		std::vector<std::shared_ptr<eval::instr>> {
		std::make_shared<eval::system_instr>(f)
	}, nullptr);
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
	mod->bind("current_position", mk_sys_fn({"file"}, [](eval::interpreter* intrp) {
		auto f = std::dynamic_pointer_cast<ios_value>(intrp->current_scope->binding("file"));
		intrp->stack.push(std::make_shared<eval::int_value>(ftell(f->f)));
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

std::shared_ptr<eval::scope> create_global_std_scope() {
	auto cx = std::make_shared<eval::scope>(nullptr);

	cx->bind("nil", std::make_shared<eval::nil_value>());

	cx->bind("print", mk_sys_fn({ "str" }, std::function([](eval::interpreter* intrp) {
		auto v = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("str"));
		std::cout << v->value;
	})));
	
	cx->bind("println", mk_sys_fn({ "str" }, std::function([](eval::interpreter* intrp) {
		auto v = std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("str"));
		std::cout << v->value << std::endl;
	})));


	cx->bind("printv", mk_sys_fn({ "val" }, std::function([](eval::interpreter* intrp) {
		auto v = intrp->current_scope->binding("val");
		v->print(std::cout);
	})));

	cx->bind("error", mk_sys_fn({ "msg" }, [](eval::interpreter* intrp) {
		throw std::runtime_error(std::dynamic_pointer_cast<eval::str_value>(intrp->current_scope->binding("msg"))->value);
	}));

	cx->modules["file"] = build_file_api();
	cx->modules["str"] = build_str_api();
	cx->modules["list"] = build_list_api();

	return cx;
}
