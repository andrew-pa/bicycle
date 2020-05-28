#include "eval.h"

void eval::analyzer::visit(ast::seq_stmt* s) {
	s->first->visit(this);
	if (s->second != nullptr) s->second->visit(this);
}

void eval::analyzer::visit(ast::block_stmt* s) {
	if (s->body == nullptr) return;
	instrs.push_back(std::make_shared<enter_scope_instr>());
	s->body->visit(this);
	instrs.push_back(std::make_shared<exit_scope_instr>());
}

void eval::analyzer::visit(ast::let_stmt* s) {
	s->value->visit(this);
	instrs.push_back(std::make_shared<bind_instr>(ids->at(s->identifer)));
}

void eval::analyzer::visit(ast::expr_stmt* s) {
	s->expr->visit(this);
	instrs.push_back(std::make_shared<discard_instr>());
}

void eval::analyzer::visit(ast::if_stmt* s) {
	// compute condition
	// if true
	//  stuff
	//  go to end
	// if false
	//  other stuff
	// end
	s->condition->visit(this);
	auto true_b_mark = new_marker();
	auto false_b_mark = new_marker();
	instrs.push_back(std::make_shared<if_instr>(true_b_mark, false_b_mark));
	instrs.push_back(std::make_shared<marker_instr>(true_b_mark));
	s->if_true->visit(this);
	if (s->if_false != nullptr) {
		auto end_mark = new_marker();
		instrs.push_back(std::make_shared<jump_to_marker_instr>(end_mark));
		instrs.push_back(std::make_shared<marker_instr>(false_b_mark));
		s->if_false->visit(this);
		instrs.push_back(std::make_shared<marker_instr>(end_mark));
	} else {
		instrs.push_back(std::make_shared<marker_instr>(false_b_mark));
	}
}

void eval::analyzer::visit(ast::continue_stmt* s) {
	auto start = 0;
	if (s->name.has_value()) {
		for (size_t i = loop_marker_stack.size() - 1; i >= 0; --i) {
			auto loop = loop_marker_stack[i];
			if (std::get<0>(loop).has_value() && std::get<0>(loop).value() == s->name.value()) {
				start = std::get<1>(loop);
				break;
			}
		}
	}
	else {
		auto loop = loop_marker_stack[loop_marker_stack.size() - 1];
		start = std::get<1>(loop);
	}
	instrs.push_back(std::make_shared<jump_instr>(start));
}

void eval::analyzer::visit(ast::break_stmt* s) {
	auto end_mark = 0;
	if (s->name.has_value()) {
		for (int i = loop_marker_stack.size() - 1; i >= 0; --i) {
			auto loop = loop_marker_stack[i];
			if (std::get<0>(loop).has_value() && std::get<0>(loop).value() == s->name.value()) {
				end_mark = std::get<2>(loop);
				break;
			}
		}
	}
	else {
		auto loop = loop_marker_stack[loop_marker_stack.size() - 1];
		end_mark = std::get<2>(loop);
	}
	instrs.push_back(std::make_shared<jump_to_marker_instr>(end_mark));
}

void eval::analyzer::visit(ast::loop_stmt* s) {
	auto start = instrs.size();
	auto endm = new_marker();
	loop_marker_stack.push_back(std::tuple(s->name, start, endm));
	s->body->visit(this);
	instrs.push_back(std::make_shared<jump_instr>(start));
	instrs.push_back(std::make_shared<marker_instr>(endm));
	loop_marker_stack.pop_back();
}

void eval::analyzer::visit(ast::return_stmt* s) {
	if (s->expr != nullptr) s->expr->visit(this);
	instrs.push_back(std::make_shared<ret_instr>());
}

void eval::analyzer::visit(ast::named_value* x) {
	instrs.push_back(std::make_shared<get_binding_instr>(ids->at(x->identifier)));
}

void eval::analyzer::visit(ast::qualified_value* x) {
	std::vector<std::string> path;
	for (auto i : x->path) path.push_back(ids->at(i));
	instrs.push_back(std::make_shared<get_qualified_binding_instr>(path));
}

void eval::analyzer::visit(ast::integer_value* x) {
	auto v = std::make_shared<int_value>(x->value);
	instrs.push_back(std::make_shared<literal_instr>(v));
}

void eval::analyzer::visit(ast::str_value* x) {
	auto v = std::make_shared<str_value>(x->value);
	instrs.push_back(std::make_shared<literal_instr>(v));
}

void eval::analyzer::visit(ast::bool_value* x) {
	auto v = std::make_shared<bool_value>(x->value);
	instrs.push_back(std::make_shared<literal_instr>(v));
}

void eval::analyzer::visit(ast::list_value* x) {
	auto v = std::make_shared<list_value>();
	instrs.push_back(std::make_shared<literal_instr>(v));
	for (auto v : x->values) {
		v->visit(this);
		instrs.push_back(std::make_shared<append_list_instr>());
	}
}

void eval::analyzer::visit(ast::map_value* x) {
	auto v = std::make_shared<map_value>();
	instrs.push_back(std::make_shared<literal_instr>(v));
	for (auto v : x->values) {
		auto n = std::make_shared<str_value>(ids->at(v.first));
		instrs.push_back(std::make_shared<literal_instr>(n));
		v.second->visit(this);
		instrs.push_back(std::make_shared<set_key_instr>());
	}
}

void eval::analyzer::visit(ast::binary_op* x) {
	if (x->op == op_type::assign) {
		auto path = std::dynamic_pointer_cast<ast::binary_op>(x->left);
		if (path != nullptr && path->op == op_type::dot) {
			path->left->visit(this);
			auto name = ids->at(std::dynamic_pointer_cast<ast::named_value>(path->right)->identifier);
			instrs.push_back(std::make_shared<literal_instr>(std::make_shared<str_value>(name)));
			x->right->visit(this);
			instrs.push_back(std::make_shared<set_key_instr>());
			return;
		}
		auto index = std::dynamic_pointer_cast<ast::index_into>(x->left);
		if (index != nullptr) {
			index->collection->visit(this);
			index->index->visit(this);
			x->right->visit(this);
			instrs.push_back(std::make_shared<set_index_instr>());
			return;
		}
		auto name = std::dynamic_pointer_cast<ast::named_value>(x->left)->identifier;
		x->right->visit(this);
		instrs.push_back(std::make_shared<set_binding_instr>(ids->at(name)));
		return;
	}
	else if (x->op == op_type::dot) {
		x->left->visit(this);
		auto name = ids->at(std::dynamic_pointer_cast<ast::named_value>(x->right)->identifier);
		instrs.push_back(std::make_shared<literal_instr>(std::make_shared<str_value>(name)));
		instrs.push_back(std::make_shared<get_key_instr>());
		return;
	}
	x->left->visit(this);
	x->right->visit(this);
	instrs.push_back(std::make_shared<bin_op_instr>(x->op));
}

void eval::analyzer::visit(ast::logical_negation* x) {
	x->value->visit(this);
	instrs.push_back(std::make_shared<log_not_instr>());
}

void eval::analyzer::visit(ast::index_into* x) {
	x->collection->visit(this);
	x->index->visit(this);
	instrs.push_back(std::make_shared<get_index_instr>());
}

void eval::analyzer::visit(ast::fn_call* x) {
	if (x->args.size() > 0) {
		for (int i = x->args.size() - 1; i >= 0; --i) {
			x->args[i]->visit(this);
		}
	}
	x->fn->visit(this);
	instrs.push_back(std::make_shared<call_instr>());
}

void eval::analyzer::visit(ast::fn_value* x) {
	std::vector<std::string> arg_names;
	for (auto an : x->args) arg_names.push_back(ids->at(an));
	eval::analyzer anl(ids, this->root_path);
	auto fn = std::make_shared<fn_value>(arg_names, anl.analyze(x->body));
	instrs.push_back(std::make_shared<literal_instr>(fn));
}

void eval::analyzer::visit(ast::module_stmt* s) {
	if(!s->inner_import) instrs.push_back(std::make_shared<enter_scope_instr>());
	if (s->body != nullptr) {
		s->body->visit(this);
	} else {
		auto modcode = eval::load_and_assemble(root_path / (ids->at(s->name)+".bcy"));
		instrs.insert(instrs.end(),
			std::make_move_iterator(modcode.begin()),
			std::make_move_iterator(modcode.end()));
	}
	if(!s->inner_import) instrs.push_back(std::make_shared<exit_scope_as_new_module_instr>(ids->at(s->name)));
}

#include <fstream>
#include "parse.h"

std::vector<std::shared_ptr<eval::instr>> eval::load_and_assemble(const std::filesystem::path& path) {
	std::ifstream input_stream(path);
	tokenizer tok(&input_stream);
	parser par(&tok);

	std::vector<std::shared_ptr<eval::instr>> code;

	while (!tok.peek().is_eof()) {
		try {
			auto stmt = par.next_stmt();
			std::cout << std::endl;
			eval::analyzer anl(&tok.identifiers, path.parent_path());
			auto part = anl.analyze(stmt);
			code.insert(code.end(), std::make_move_iterator(part.begin()), std::make_move_iterator(part.end()));
		}
		catch (const parse_error& pe) {
			std::cout << "parse error: " << pe.what()
				<< " [file= " << path << "line= " << tok.line_number
				<< " token type=" << pe.irritant.type << " data=" << pe.irritant.data << "]";
		}
		catch (const std::runtime_error& e) {
			std::cout << "error: " << e.what() << " in file " << path << std::endl;
		}
	}

	return code;
}


