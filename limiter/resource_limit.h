#pragma once
#ifndef RESOURCE_LIMIT
#define RESOUCE_LIMIT
#include <libcgroup.h>
#include "thirdlib/nlohmann/json.hpp"
#define CGROUP_CONFIG_PATH "/root/projects/limiter/cgroup.json"
using json = nlohmann::json;

namespace resource_limit {

	struct rlconfig;
	struct subsys_arg;	
	//子系统 名称，参数 如：memory 
	using subsys = std::pair<std::string, std::vector<subsys_arg>>;
	using cgroups_cfg = std::vector<rlconfig>;
	//参数的类型
	enum class Value_Type { Empty, INT, UINT, BOOL, STRING };
	//各个子系统中参数的名称和对应的值
	struct subsys_arg {
		std::string name, val;
		Value_Type type;
		subsys_arg(std::string _name, int _val) {
			name = _name, val = std::to_string(_val);
			type = Value_Type::INT;
		}
		subsys_arg(std::string _name, std::string _val) {
			name = _name, val = _val;
			type = Value_Type::STRING;
		}
		subsys_arg(std::string _name, bool b) {
			name = _name;
			if (b)val = 't';
			else val = 'f';
			type = Value_Type::BOOL;
		}
		subsys_arg() {
			type = Value_Type::Empty;
		}
	};

	//资源限制
	struct rlconfig {
		//组名称
		std::string name;
		//子系统 名称,参数
		std::vector<subsys> subs;
	};

#ifdef SO
	int start_from_json(json& j);
#endif // SO

}



#endif // !RESOURCE_LIMIT

