#include "serializer.h"
#include<fstream>
#include<iostream>

inline int is_path_exist(const char* path) {
	return !access(path, F_OK);
}

inline int write_to_code(string& path, string& code) {
	int rtn = file_util::write_data(path, code);
#ifndef DEBUG_PERMISSION
	if (chown(path.c_str(), JUDGE_USER, JUDGE_USER)) {
		LOGGER_ERR("chown failed, path {}, err: {}", path, strerror(errno));
		return dispatcher_errors::ChownFailed;
	}
#endif // !DEBUG_PERMISSION

	return rtn;
}
/**
 * @brief 获取 __len__ 中的长度
 * @param pattern_path 父路径，最后的'/'不要忘
 * @return 测试点数量
*/
int get_file_list_num(const char* pattern_path) {
	string path(pattern_path);
	path.append("__len__");
	std::ifstream fin(path.c_str(), std::ios::in);
	if (!fin.is_open()) {
		LOGGER_ERR("can't open __len__ file, path {}", pattern_path);
		return -1;
	}
	int len = 0;
	fin >> len;
	fin.close();
	return len;
}
/**
 * @brief 获取测试点数量
 * @param js
 * @return
*/
int get_tests_count(json& js) {
	if (js.at("pid").get<int>() == 0)
		return 1;
	return get_file_list_num(get_problem_path(js).c_str());
}
inline string get_problem_path(json& js)
{
	int cid = -1, pid = 0;
	if (js.contains("cid")) {
		cid = js.at("cid").get<int>();
	}
	pid = js.at("pid").get<int>();
	return get_problem_path(pid, cid);
}
inline string get_problem_path(int pid, int cid)
{
	string root(ROOT_PATH_PROBLEMS);
	if (cid != -1) {
		root.append("C").append(std::to_string(cid)).append("/");
	}
	root.append("P").append(std::to_string(pid)).append("/");
	return root;
}
// 形如xxx/Pxxx/Pxxx_%d,做默认模式串用
// 比赛路径，使用 Cxx/Pxx/Pxx_%d
string get_problem_path(oj_serializer& info, string& root) {
	string pattern_path;
	pattern_path.append(ROOT_PATH_PROBLEMS);
	if (info.cid >= 0) {
		pattern_path.append("C").append(std::to_string(info.cid)).append("/");
	}
	string problem("P");
	problem.append(std::to_string(info.pid));
	pattern_path.append(problem).append("/");
	root = pattern_path;
	pattern_path.append(problem).append("_{}");
	return pattern_path;
}
// 默认/tmp/Uxxx/uuid
string get_user_code_path(oj_serializer& info, string& pattern) {
	string pattern_path(ROOT_PATH_USERS);
	string user("U");
	user.append(std::to_string(info.uid));
	pattern_path.append(user).append("/");
	pattern.append(pattern_path);
	pattern_path.append(info.uuid);

	return pattern_path;
}

const char* get_language(const char* str) {
	if (!strcmp(str, "cpp") || !strcmp(str, "cpp11")) {
		return "cpp";
	}
	else if (!strcmp(str, "c")) {
		return "c";
	}
	return NULL;
}

int serializer_compile(json& js) {
	dispatcher_errors::Code rtn = dispatcher_errors::None;
	oj_serializer serial(js);
	string pattern;
	string out_path = get_user_code_path(serial, pattern);
	string code_path = out_path + ".";
	code_path.append(get_language(js.at("lang").get<string>().c_str()));
	if (!is_path_exist(pattern.c_str())) {
		umask(0);
		if (mkdir(pattern.c_str(), S_IRWXU | S_IRWXG | S_IRWXO)) { // 目录权限需给其他用户使用 777
			if (errno != 17) {
				LOGGER_ERR("can't mkdir, path: {} errno: {} err: {}", pattern.c_str(), errno, strerror(errno));
				return dispatcher_errors::MkDirFailed;
			}
		}
	}
	// 输出及错误路径
	string err_path;
	err_path.append(out_path).append(".error");

	string code = js.at("code").get<string>();
	js["src_path"] = code_path;
	rtn = dispatcher_errors::Code(write_to_code(code_path, code));
	if (dispatcher_errors::None != rtn) {
		LOGGER_ERR("write user code failed! code: {}", (int)rtn);
		return rtn;
	}
	// -------------------------编译部分的参数-----------------------------
	js["des_path"] = out_path;
	js["err_path"] = err_path;
	js["cg_name"] = "compilecgroup";

	return 0;
}

int serializer_run(json& js) {
	oj_serializer serial(js);
	string problem_root;
	string problem_path;
	string out_path = js.at("des_path").get<string>();

	string ans_path;
	ans_path.append(out_path).append(".ans");
	if (serial.pid != 0) {
		problem_path = get_problem_path(serial, problem_root);// xx/Pxxx/Pxxx_{}
	}
	else if (serial.pid == 0) {
		problem_path = out_path;
		string input = js.at("input").get<string>();
		string prob_des = problem_path + "_0";
		file_util::write_data(prob_des, input);
		problem_path = out_path + "_{}";
	}	// 通过编辑器提交的

	if (serial.pid != 0 && !is_path_exist(problem_root.c_str())) {
		LOGGER_ERR("problem not exist");
		return dispatcher_errors::PathNotExist;
	}
	js["src_path"] = js["des_path"];
	js["prob_root"] = problem_root;
	js["des_path"] = ans_path;
	js["prob_path"] = problem_path;
	int len = 0;
	if (serial.pid != 0) {
		// 记录数量
		len = get_file_list_num(problem_root.c_str());
		if (len <= 0) {
			return dispatcher_errors::TaskNumUnexpect;
		}
	}
	// 编辑器只有一个测试点
	else if (serial.pid == 0) {
		len = 1;
	}
	js["task_number"] = len;

	return 0;
}

int serializer(json& js) {
	int rtn = serializer_compile(js);
	if (rtn)return rtn;
	rtn = serializer_run(js);
	INFO(js.dump());
	return rtn;
}