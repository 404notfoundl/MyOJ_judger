#ifndef COMPILER
#define COMPILER
#include"ThirdLib/nlohmann/json.hpp"
// compiler 模块
using nlohmann::json;
namespace compiler {
	enum class Language { Empty, C_CPP_COMMON, C, CPP, CPP11 };
	enum State { Success, Failed, Error };
	enum Errors {
		Truncate_Failed,
		Open_Failed,
		Dup_Failed,
		Open_Shm_Failed,
		Set_Limits_Failed,
		Fork_Error,
		Compile_Timeout_Error // 可能出现意料之外的原因导致编译超时
	};
	State start_from_json(json& js);
}
#endif // !COMPILER
