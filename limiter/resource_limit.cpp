#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include "resource_limit.h"

namespace resource_limit {
	const char* str_errno() {
		return cgroup_strerror(cgroup_get_last_errno());
	}

	int modify_subsys(cgroup** cg, subsys& sub) {
		int new_ctrl = 0;
		cgroup_controller* ctrl = nullptr;
		if ((ctrl = cgroup_get_controller(*cg, sub.first.c_str())) == NULL) {
			cgroup_add_controller(*cg, sub.first.c_str());
			ctrl = cgroup_get_controller(*cg, sub.first.c_str());
			new_ctrl = 1;
		}

		for (auto arg : sub.second) {
			const char* name = arg.name.c_str();
			const char* val = arg.val.c_str();
			switch (arg.type) {
			case Value_Type::INT:
				if (cgroup_set_value_int64(ctrl, name, atoi(val)))
					printf("set int failed!\n");
				break;
			case Value_Type::STRING:
				if (cgroup_set_value_string(ctrl, name, val))
					printf("set string failed!\n");
				break;
			case Value_Type::BOOL:
				if (val[0] == 'y')
				{
					if (cgroup_set_value_bool(ctrl, name, true))
						printf("set bool failed\n!");
				}
				else if (cgroup_set_value_bool(ctrl, name, false))
						printf("set bool failed\n!");
				break;
			default:
				fprintf(stderr, "unidentified subsys param!\n");
				break;
			}
		}
		return new_ctrl;
	}

	int modify_cgroup(const rlconfig& r, cgroup** cg) {
		//子系统
		for (auto subs : r.subs) {
			modify_subsys(cg, subs);
		}
		if (!cgroup_modify_cgroup(*cg)) {
			printf("modify cg errno:%d err:%s\n", cgroup_get_last_errno(), cgroup_strerror(cgroup_get_last_errno()));
		}
		return 0;
	}

	cgroup* load_cgroup(const rlconfig& r) {
		cgroup* tem = cgroup_new_cgroup(r.name.c_str());

		int create_ctrl = 0;
		for (auto subs : r.subs) {
			create_ctrl |= modify_subsys(&tem, subs);
		}
		int res = 0;
		if (create_ctrl)
			res = cgroup_create_cgroup(tem, 0);
		else
			res = cgroup_modify_cgroup(tem);
		if (res) {
			printf("cgroup failed! errno:%d error:%s\n", res, str_errno());
		}

		return tem;
	}

	int remove_cgroup(cgroup* cg) {
		if (cgroup_get_cgroup(cg)) {
			fprintf(stderr, "cgroup not exist!\n");
			return 1;
		}
		if (cgroup_delete_cgroup(cg, 0)) {
			fprintf(stderr, "delete failed!\n");
			return 1;
		}
		return 0;
	}

	int from_json(subsys& sub, json& j) {
		//循环各个参数

		for (auto arg_iter = j.begin(); arg_iter != j.end(); arg_iter++) {
			if (arg_iter.key()[0] == '@')continue;
			auto val = arg_iter.value();
			if (val.is_string()) {
				subsys_arg par(arg_iter.key().c_str(), val.get<std::string>());
				sub.second.push_back(par);
			}
			else if (val.is_boolean()) {
				subsys_arg par(arg_iter.key().c_str(), val.get<bool>());
				sub.second.push_back(par);
			}
			else if (val.is_number_integer()) {
				subsys_arg par(arg_iter.key().c_str(), val.get<int>());
				sub.second.push_back(par);
			}
			else {
				fprintf(stderr, "unknow type\n");
				return -1;
			}
		}
		return 0;
	}

	int from_json(rlconfig& r, json& j) {
		for (auto sub_iter = j.begin(); sub_iter != j.end(); sub_iter++) {
			if (sub_iter.key()[0] == '@')continue;
			subsys sub;
			sub.first = sub_iter.key();
			from_json(sub, sub_iter.value());
			r.subs.push_back(sub);
		}
		return 0;
	}

	int read_json(json& cfg) {
		std::ifstream is;
		is.open(CGROUP_CONFIG_PATH, std::ios_base::in);
		if (!is.is_open()) {
			printf("open configure failed!\n");
			exit(0);
		}
		is >> cfg;
		return 0;
	}

	int init(json& j) {
		cgroup_init();
		//j.at("cg");
		read_json(j);
		return 0;
	}

	int init() {		
		return cgroup_init();
	}

	int load_cgroup_arg(cgroups_cfg& cgs, json& j) {
		for (auto cgs_iter = j.begin(); cgs_iter != j.end(); cgs_iter++) {
			if (cgs_iter.key()[0] == '@')continue;
			rlconfig r;
			r.name = cgs_iter.key();
			from_json(r, cgs_iter.value());
			cgs.push_back(r);
		}
		return 0;
	}

	int free_cgroup(cgroup** cg) {
		if (cg != NULL) {
			cgroup_free(cg);
			*cg = NULL;
		}
		return 0;
	}
#ifdef SO
	int start_from_json(json& j) {
		init();
		cgroups_cfg cgs;
		load_cgroup_arg(cgs, j);
		for (auto a : cgs) {
			auto res = resource_limit::load_cgroup(a);
			if (res == nullptr) {
				printf("load cgroup failed!\n");
				return 1;
			}
			resource_limit::free_cgroup(&res);
		}
		return 0;
	}
#endif // !SO
}
#ifndef SO

int main(int argc, char* argv[])
{
	json cfg;
	resource_limit::init(cfg);
	resource_limit::cgroups_cfg cgs;
	resource_limit::load_cgroup_arg(cgs, cfg.at("cgroups"));
	for (auto a : cgs) {
		auto res = resource_limit::load_cgroup(a);
		if (res == nullptr) {
			printf("load cgroup failed!\n");
		}

		resource_limit::free_cgroup(&res);
	}
	return 0;
}
#endif // !SO