#pragma once
#ifndef JUDGER
#define JUDGER
#include "ThirdLib/nlohmann/json.hpp"
#include <seccomp.h>
using nlohmann::json;
namespace judger {
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
			TESTCASE_COUNT_ERROR,
			RUN_CODE_ERROR,

			UNKNOW_ERROR
		};
	};
	namespace compiler {
		exec_base::state start(json& js);
	}
	namespace runner {
		exec_base::errors start(json& js, scmp_filter_ctx ctx);
	}
	namespace checker {
		/**
		* @brief 评测方式
		*/
		enum method {
			COMMON, SPJ, TEMP
		};
	}
}
#endif // !JUDGER
