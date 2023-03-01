#pragma once
#ifndef OJ_SERIALIZER
#define OJ_SERIALIZER
#include<sys/stat.h>
#include<unistd.h>
#include<string>
#include<dirent.h>
#include<vector>
#include<cstring>
#include "ThirdLib/nlohmann/json.hpp"
#include "judger_util.h"
#include "logger.h"
#include "dispatcher_errors.h"
using nlohmann::json;
using cstr = const char*;
using std::string;
using str_array = std::vector<std::string>;
#ifdef TEST_RABBITMQ
constexpr cstr ROOT_PATH_PROBLEMS = "/home/test_problems/"; // "/root/projects/problems/"
#else
constexpr cstr ROOT_PATH_PROBLEMS = "/home/problems/";
#endif // TEST_RABBITMQ
// TODO 待重构
constexpr cstr ROOT_PATH_USERS = "/tmp/";
constexpr int JUDGE_USER = 1000;
struct oj_serializer {
	int uid, pid, cid;
	string uuid;
	oj_serializer(json& js) {
		js.at("uid").get_to(uid);
		uuid = js.at("uuid").get<string>();
		pid = js.at("pid").get<int>();
		if (js.contains("cid")) {
			cid = js.at("cid").get<int>();
		}
		else cid = -1;
	}
	oj_serializer() {
	}
};
/**
 * @brief 编译参数赋值
 * @param js
 * @return
*/
int serializer_compile(json& js);
/**
 * @brief 运行参数赋值
 * @param js
 * @return
*/
int serializer_run(json& js);

int serializer(json& js);
/**
 * @brief 获取测试点数量
 * @param js
 * @return
*/
int get_tests_count(json& js);
/**
 * @brief
 * @param js
 * @return 测试点所在文件夹
*/
string get_problem_path(json& js);

string get_problem_path(int pid, int cid = -1);

int is_path_exist(const char* path);
/**
 * @brief 写入代码
 * @param path
 * @param code
 * @return
*/
int write_to_code(string& path, string& code);

#endif // !OJ_SERIALIZER
