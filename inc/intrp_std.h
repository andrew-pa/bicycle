#pragma once
#include "eval.h"

std::shared_ptr<eval::value> mk_sys_fn(std::initializer_list<std::string>&& args, std::function<void(eval::interpreter* intrp)> f);

std::shared_ptr<eval::scope> create_global_std_scope();
