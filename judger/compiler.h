#pragma once
#ifndef COMPILER
#define COMPILER
#include "exec_base.h"
#include "judger_args.h"
namespace judger {
	namespace compiler {
		/**
		 * @brief �����û�����
		 * exec_base: ����ֶ��ڴ˴�����
		 *	src_path: Դ��·��
		 * 	des_path: �������·��
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


