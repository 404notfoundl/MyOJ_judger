#pragma once
#ifndef RESOURCE_LIMIT
#define RESOUCE_LIMIT
#include <libcgroup.h>
#include "ThirdLib/nlohmann/json.hpp"
// resouce_limit 模块
using nlohmann::json;

namespace resource_limit {
	//参数的类型
	enum class Value_Type { Empty, INT, UINT, BOOL, STRING };

	int start_from_json(json& j);
}

#endif // !RESOURCE_LIMIT
