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
			static const string options; // ����ѡ��
			static json compile_options;
			static unordered_map<string, string> compiler_path; // ������·��
			static const unordered_map<string, unordered_set<string>> compiler_args; // ��������ѡ���ӳ��
		};
	}
	namespace runner {
		class args {
		public:
			static string env_var; // ִ���û������Ĭ�ϻ�������
		};
	}
	namespace checker {
		class args {
		public:
			// spj ·��
			static const string spj_root;
			static const string spj_wcmp;
			static const string spj_ncmp;
		};
	}
}
#endif // !JUDGER_ARGS

