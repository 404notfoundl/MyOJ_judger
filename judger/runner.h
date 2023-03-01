#pragma once
#ifndef RUNNER
#define RUNNER
#include <vector>

#include "exec_base.h"
#include "judger_args.h"
#include "scmp.h"


using judge_details = std::vector<string>; //评测的详情

namespace judger {
	namespace checker {
		/**
		* @brief 评测方式
		*/
		enum method {
			COMMON, SPJ, TEMP
		};		
	}
    namespace runner {

		class result_group {
		public:
			string status;
			judge_details details;
			int max_time_usage;
			double max_memory_usage;
			void update(const string& _status, int _time_use, double _mem_use, const string& _details);
			void update(exec_result& res);
			void to_json(json& js);
			result_group();
			~result_group();
		};

		/** 
		 * @brief 运行程序
		 * exec_base: 相关字段在此处作用
		 *	src_path: 源程序路径
		 *	des_path: 程序输出路径
		 *	argv: 环境变量
		*/
		class runner :public exec_base
		{
		public:
			runner(json& js);
			runner(cmdline::parser& cmd);
			virtual ~runner();

			int scmp_init(uint32_t action = SCMP_ACT_KILL);
			// 通过 exec_base 继承
			virtual state start() override;
			string prob_path;
			string _env_path;
			string uuid;
			string prob_root; 
			checker::method checker_method;
			scmp_filter_ctx scmp;
			int task_number;
			int task_start;
			exec_result result;
			json usr_result;
		private:
			void set_env_path();
			template <typename T>
			void set_default(T& field){}
			void set_default(int& field);
		};
		exec_base::errors start(cmdline::parser& cmd);
		exec_base::errors start(json& js, scmp_filter_ctx ctx);
		exec_base::errors _start(runner& instance);
		void set_cmdline(cmdline::parser& cmd, int argc, char* argv[]);
	}
	namespace checker {
		class checker : public exec_base
		{
		public:
			checker(runner::runner& _run, runner::result_group& rg);
			~checker();
			virtual state start() override;
			runner::runner& run;
			runner::result_group& rg;
			string spj_path, spj_name;
		private:
		};
		exec_base::state start(runner::runner& run, runner::result_group& rg);
	}
}
DEFINE_STRUCT_SCHEMA(judger::runner::runner,
	DEFINE_STRUCT_FIELD(uuid, "uuid"),
	DEFINE_STRUCT_FIELD(prob_path, "prob_path"),
	DEFINE_STRUCT_FIELD(_env_path, "env_path"),
	DEFINE_STRUCT_FIELD(prob_root, "prob_root"),  
	DEFINE_STRUCT_FIELD(task_number, "task_number"),
	DEFINE_STRUCT_FIELD(task_start, "start"),
	DEFINE_STRUCT_FIELD(checker_method, "method")
);

DEFINE_STRUCT_SCHEMA(judger::runner::result_group,
	DEFINE_STRUCT_FIELD(status, "status"),
	DEFINE_STRUCT_FIELD(details, "details"),
	DEFINE_STRUCT_FIELD(max_time_usage, "time_usage"),
	DEFINE_STRUCT_FIELD(max_memory_usage, "memory_usage")
);
#endif // !RUNNER