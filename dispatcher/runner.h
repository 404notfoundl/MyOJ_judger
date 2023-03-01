#pragma once
#ifndef RUNNER
#define RUNNER
#include"ThirdLib/nlohmann/json.hpp"
#include <seccomp.h>
// runner 模块
using nlohmann::json;
namespace runner {
	enum class judge_status {
		AC, WA, CE,
		RE, TLE, MLE,
		UKE, Complete
	};

	enum class language {
		Empty, C, CPP,
		CPP11
	};

	enum errors {
		None, Not_AC, Load_RLimit_Error,
		Load_Scmp_Error, Load_AnsFile_Error,
		Load_Cgroup_Error, Redirect_Failed,
		Get_Logger_Failed, File_Not_Existed,
		Testcase_Count_Error, Fork_Failed,
		Set_Uid_Failed, Load_Shm_Error,
		Get_Prob_Path_Failed, Open_Shm_Failed,
		Attach_Cgroup_Failed,
		Unknow_Error
	};
	errors start_from_str(std::string& str);
	errors start_from_json(json& js);
	errors start_from_json(json& js, scmp_filter_ctx ctx);
}
extern "C" {
	typedef struct return_str {
		char* pstr;
		char* str;
	}*prstr;
	prstr start_from_json(char* str);
}
int start_from_json_cpp(char* str);

#endif // !RUNNER
