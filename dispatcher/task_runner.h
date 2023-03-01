#pragma once
#ifndef TASK_RUNNER
#define TASK_RUNNER
#include <iostream>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <atomic>
#include "serializer.h"
#include "resource_limit.h"
#include "worker.h"
#include "ThirdLib/nlohmann/json.hpp"
#include "judger.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE   //设置亲和性
#endif // !_GNU_SOURCE
using std::string;
using pjson = std::shared_ptr<nlohmann::json>;
class task_runner
{
private:
	static int serializer_common(json& src, json& des);
	int run_task(int task_mode);
	int queue_append(task_runner* p);
	judger::exec_base::state compile(json& info, json& js);
	judger::exec_base::errors run(json& info, json& js);
	task_runner(const task_runner&) = delete;
private:
	pjson task;
	int thread_id;// 当前执行线程编号
public:
	task_runner();
	task_runner(pjson& t);
	~task_runner();
	static int set_limits(int thread_id, json& args);
	static int serializer_compile_res(json& src, json& des);
	static int serializer_run_res(json& src, json& des);
	/**
	 * @brief 设置线程亲和性
	 * @param id
	 * @return
	*/
	static int thread_configure(int id);
	void start();
	int set_thread_id(int id);
	int set_task(pjson& t);
	int set_task(string str);
	int set_task(json& js);
	int reset();
	pjson get_task();
public:
	static block_queue<task_runner>* resutl_queue;
	static std::atomic<int>* success; // 测试用
	static scmp_filter_ctx m_ctx;
	static bool exit_flag; // 退出用标志
	static int notice_fd;
};
#endif // !TASK_RUNNER
