#pragma once
#ifndef EXEC_BASE
#define EXEC_BASE
#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <unordered_map>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <libcgroup.h>
#include <libgen.h>
#include <tuple>

#include "thirdlib/nlohmann/json.hpp"
#include "thirdlib/cmdline.h"
#include "thirdlib/spdlog/spdlog.h"
#include "thirdlib/static_reflection.h"
#include "lock/locker.h"
#include "logger.h"
#include "judger_args.h"
#include "judger_util.h"

using std::string;
using std::unordered_map;
using nlohmann::json;
using std::enable_if_t;
using std::decay_t;

template< class T >
constexpr bool is_enum_v = std::is_enum<T>::value;

namespace judger {
	constexpr int JUDGE_USER = 1000; // uid
	constexpr int MB = 1024 * 1024;
	constexpr int MILLSECOND = 1e3;
	/**
	* @brief 子进程执行结果
	*/
	class exec_result {
	public:
		int time_usage;
		double memory_usage, task_num;
		string details;
		int child_state; // 默认 WA
		int _state; // 原返回值，默认 -1
		int term_sig;
		void clean();
		exec_result(int _time_use = 0, int _mem_use = 0, int _term_sig = -1, int _state = 2, const string& _details = "", int _task_num = 0);
		~exec_result();
	};
	/**
	 * @brief 执行相关基类
	*/
	class exec_base
	{
	public:
		enum state {
			AC, COMPLETE,
			WA, CE, // 评测的结果
			RE, TLE, MLE, UKE, // 评测的结果
			FAIL, ERROR
		};
		enum errors {
			NONE,
			TRUNCATE_FAILED,
			OPEN_FAILED,
			DUP_FAILED,
			FORK_FAILED,
			REDIRECT_FAILED,
			LOAD_RLIMIT_ERROR,
			LOAD_SCMP_ERROR,
			LOAD_CGROUP_ERROR,
			FILE_NOT_EXISTED,
			SET_PERMISSION_FAILED,
			OPEN_SHM_FAILED,
			// runner errors
			TESTCASE_COUNT_ERROR, // 测试点数量出错
			RUN_CODE_ERROR, // 运行未通过

			UNKNOW_ERROR
		};

		exec_base(json &js);
		exec_base(cmdline::parser& cmd);
		virtual ~exec_base();
		int uid, pid;
		//时空限制
		int time_limit, mem_limit;
		char* argv[128];// exec调用的参数
		string src_path, des_path, err_path, cg_name, lang;
		bool scmp_flag; // 启用scmp
	public:
		virtual state start() = 0;
		static const char* cg_last_str_err();
		static int cg_init();    
	protected:
		enum limit_flag {
			NO_LIMIT = -1
		};

		exec_base();
		int cal_time(timespec start, timespec end);
		double get_mem_usage(rusage& usage);
		int attach_cgroup(pid_t pid);
		errors limits();
		int set_permission();
		int redirect_fstream(const string& src, FILE* des, int mode, int flags);
		int truncate_file(int fd);
		static int timeout_killer(pid_t pid, int time_limit);
		exec_result get_child_state(int child_state, int proc_time_use, const rusage& usage);
	};
	/**
	 * @brief 获取json的值
	*/
	struct json_field {
	public:
		json_field(json* _js);
		~json_field();

		template <typename Field, typename Name>
		void operator()(Field&& field, Name&& name) {
			if (js->contains(name)) {
				js->at(name).get_to(field);
			}
			else {
				TRACE("instance not have field {}", name);
				this->set_default(field);
			}
		}

	private:
		template <typename T>
		void set_default(T& field) {
			field = T{};
		}
		json* js;
	};
	/**
	 * @brief 获取cmdline的值
	*/
	struct cmdline_field {
	public:
		cmdline_field(cmdline::parser* _cmd);
		~cmdline_field();
		/**
		 * @brief 枚举特判
		 * @tparam Field 
		 * @tparam Name 
		 * @tparam  
		 * @param field 
		 * @param name 
		*/
		template < typename Field, typename Name,
			enable_if_t<is_enum_v<decay_t<Field>>, int > = 0
		>
		void operator()(Field&& field, Name&& name) {
			if (cmd->exist(name)) {
				using type = decay_t<decltype(field)>;
				field = static_cast<type>(cmd->get<int>(name));
			}
			else {
				this->set_default(field);
			}
		}
		/**
		 * @brief 其他的基本类型
		 * @tparam Field 
		 * @tparam Name 
		 * @tparam  
		 * @param field 
		 * @param name 
		*/
		template < typename Field, typename Name,
			enable_if_t<!is_enum_v<decay_t<Field>>, int > = 0
		>
		void operator()(Field&& field, Name&& name) {
			if (cmd->exist(name)) {
				using type = decay_t<decltype(field)>;
				field = cmd->get<type>(name);
			}
			else {
				this->set_default(field);
			}
		}
	private:
		template <typename T>
		void set_default(T& field) {
			field = T{};
		}
		cmdline::parser* cmd;
	};
}

DEFINE_STRUCT_SCHEMA(judger::exec_base,
	DEFINE_STRUCT_FIELD(uid, "uid"),
	DEFINE_STRUCT_FIELD(pid, "pid"),
	DEFINE_STRUCT_FIELD(time_limit, "time_limit"),
	DEFINE_STRUCT_FIELD(mem_limit, "mem_limit"),
	DEFINE_STRUCT_FIELD(src_path, "src_path"),
	DEFINE_STRUCT_FIELD(des_path, "des_path"),
	DEFINE_STRUCT_FIELD(err_path, "err_path"),
	DEFINE_STRUCT_FIELD(cg_name, "cg_name"),
	DEFINE_STRUCT_FIELD(lang, "lang")
);
#endif // !EXEC_BASE