#pragma once
#ifndef JUDGER_ARGS
#define JUDGER_ARGS
#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "thirdlib/nlohmann/json.hpp"

using std::string;
using std::unordered_map;
using std::unordered_set;
using nlohmann::json;
namespace judger {
	namespace compiler {
		class args {
		public:			
			static const string options; // 编译选项
			static json compile_options;
			static unordered_map<string, string> compiler_path; // 编译器路径
			static const unordered_map<string, unordered_set<string>> compiler_args; // 语言所需选项的映射
		};
	}
	namespace runner {
		class args {
		public:
			static string env_var; // 执行用户程序的默认环境变量
		};
	}
	namespace checker {
		class args {
		public:
			// spj 路径
			static const string spj_root;
			static const string spj_wcmp;
			static const string spj_ncmp;
		};
	}
}
#endif // !JUDGER_ARGS

