#pragma once
#ifndef COMPILER
#define COMPILER
#include "exec_base.h"
#include "judger_args.h"
namespace judger {
	namespace compiler {
		/**
		 * @brief 编译用户代码
		 * exec_base: 相关字段在此处作用
		 *	src_path: 源码路径
		 * 	des_path: 输出程序路径
		*/
		class compiler : exec_base
		{
		public:

			compiler(json& js);
			compiler(cmdline::parser& cmd);
			virtual ~compiler();
			state start() override;
			int merge_argv(json& cfg);
			int add_arg(json& cfg);
		private:
			int argv_cnt;	
			string arg_str;
		};
		exec_base::state start(json& js);

		exec_base::state start(cmdline::parser& cmd);

		int add_cmd(cmdline::parser& cmd, int argc, char* argv[]);
	}
}
#endif // !COMPILER


