#include "task_runner.h"
int task_runner::notice_fd = 0;
bool task_runner::exit_flag = false;
block_queue<task_runner>* task_runner::resutl_queue = nullptr;
std::atomic<int>* task_runner::success = nullptr;
scmp_filter_ctx task_runner::m_ctx = nullptr;

int task_runner::serializer_common(json& src, json& des) {
	des["uuid"] = src.at("uuid").get<string>();
	des["pid"] = src.at("pid").get<int>();
	des["uid"] = src.at("uid").get<int>();
	//if (src.contains("cid"))
	//	des["cid"] = src.at("cid").get<int>();
	return 0;
}

int task_runner::run_task(int task_mode)
{
	json info(*task), js;
	bool compile_flag = task->at("compile_flag").get<bool>();
	bool run_flag = task->at("run_flag").get<bool>();
	if (info.contains("cid"))
		js["cid"] = info.at("cid").get<int>();
	if (info.contains("start"))
		js["start"] = info.at("start").get<int>();
	if (info.contains("task_number"))
		js["task_number"] = info.at("task_number").get<int>();
	if (!compile_flag) {
		dispatcher_errors::Code rtn = dispatcher_errors::None;
		rtn = dispatcher_errors::Code(serializer_compile(info));
		//INFO(info.dump());
		if (rtn) {
			//TODO 此部分出现的问题应当终止执行
			LOGGER_ERR("serialize compile failed");
			return rtn;
		}
		// 编译失败或是测试点模式下需直接返回
		auto code = compile(info, js);
		if (code >= judger::exec_base::FAIL || !task_mode)
			return 0;
	}
	if (!run_flag) {
		auto rtn_code = dispatcher_errors::Code(serializer_run(info));
		if (rtn_code) {
			LOGGER_ERR("serialize run failed");
			return rtn_code;
		}
		//INFO(info.dump());
		auto rtn = run(info, js);
		(*task)["compile_flag"] = true;
		(*task)["compile"] = true;
		(*task)["run_flag"] = true;
		if (rtn > judger::exec_base::NONE) {
			LOGGER_ERR("run usr code failed");
			return rtn;
		}
	}
	return 0;
}

int task_runner::queue_append(task_runner* p)
{
	resutl_queue->emplace(p);
	return 0;
}

judger::exec_base::state task_runner::compile(json& info, json& js)
{
	auto compile_res = judger::compiler::start(info);
	//INFO("compile finish: {}", (int)compile_res);
	if (compile_res != judger::exec_base::AC) {
		// 编译是通用的
		serializer_compile_res(info, js);
		set_task(js);
		return compile_res;
	}
	else {
		(*task)["compile_flag"] = true;
		(*task)["compile"] = true;
	}

	return judger::exec_base::state::AC;
}

judger::exec_base::errors task_runner::run(json& info, json& js)
{
	// 执行运行部分
	if (set_limits(thread_id, info)) {
		return judger::exec_base::errors::LOAD_CGROUP_ERROR;
	}

	info["cg_name"] = std::to_string(thread_id);
	bool res = true;
	auto run_res = judger::runner::start(info, m_ctx);
	//INFO("run finish: {}", (int)run_res);

	if (run_res != judger::exec_base::errors::NONE) {
		if (run_res == judger::exec_base::errors::RUN_CODE_ERROR)
			res = false;
		else {
			LOGGER_ERR("run user code error occured! return val: {}", int(run_res));
			return run_res;
		}
	}
	(*success)++;
	serializer_run_res(info, js);
	js["runner"] = res;
	set_task(js);
	return judger::exec_base::errors::NONE;
}

int task_runner::serializer_compile_res(json& src, json& des) {
	serializer_common(src, des);
	des["compile"] = false;
	des["compile_flag"] = true;
	des["run_flag"] = false;
	des["runner"] = false;
	des["status"] = "c";
	auto tem = std::vector<string>({ file_util::read_str(src.at("err_path").get<string>().c_str(),512) });// 获取编译错误详情
	des["details"] = tem;
	//if (src["pid"] == 0)des["output"] = err_msg; // 编译器编译出错
	return 0;
}

int task_runner::serializer_run_res(json& src, json& des) {
	serializer_common(src, des);
	des["compile"] = true;
	des["status"] = src["status"];
	des["details"] = src["details"];
	des["time_usage"] = src["time_usage"];
	des["memory_usage"] = src["memory_usage"];
	if (src.contains("output"))
		des["output"] = src["output"];
	return 0;
}

int task_runner::thread_configure(int id)
{
	pthread_t thread = pthread_self();
	//pthread_attr_t attr;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(id, &cpuset);
	int rtn = 0;
	rtn = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset); //设置亲和性
	if (rtn)
		return rtn;
	return 0;
}

int task_runner::set_task(pjson& t)
{
	task = t;
	return 0;
}

int task_runner::set_task(string str)
{
	pjson tmp = std::make_shared<json>();
	(*tmp) = json::parse(str);
	task.reset();
	task = tmp;
	return 0;
}

int task_runner::set_task(json& js)
{
	pjson t(new json(js));
	task.reset();
	task = t;
	return 0;
}

int task_runner::reset()
{
	task.reset();
	return 0;
}

pjson task_runner::get_task()
{
	if (!task)
		return nullptr;
	return task;
}

task_runner::task_runner() :task(), thread_id(-1)
{
}

task_runner::task_runner(pjson& t) : task(t), thread_id(-1)
{
}

task_runner::~task_runner()
{
}

int task_runner::set_limits(int thread_id, json& args) {
	if (!args.contains("mem_limit"))
	{
		LOGGER_ERR("key is not exist!\n");
		return -1;
	}
	int mem_limit = args.at("mem_limit").get<int>();
	mem_limit += 5;
	mem_limit *= 1024 * 1024;
	json limits;
	limits[std::to_string(thread_id)]["memory"] = {
		{"memory.limit_in_bytes",mem_limit},
		{"memory.swappiness",0}
	};
	// TODO 禁止相关设备访问，尽量避免容器逃逸
	if (resource_limit::start_from_json(limits)) {
		LOGGER_ERR("set cgroup failed");
		return -1;
	}
	return 0;
}

void task_runner::start()
{
	bool task_mode = false;
	if (task->contains("task_mode")) task_mode = task->at("task_mode").get<bool>();
	if (!task_mode) {
		// 输出添加线程编号
		string str;
		str = task->at("ans_path").get<string>();
		str.append(std::to_string(thread_id));
		task->at("ans_path") = str;

		str = task->at("err_path").get<string>();
		str.append(std::to_string(thread_id));
		task->at("err_path") = str;
	}
	auto rtn = run_task(task_mode);
	//INFO("thread {} finish, rtn {}", thread_id, rtn);
	if (rtn)
		exit(rtn);
	queue_append(this);
	// 通知主线程发送
	char ch = 1;
	write(notice_fd, &ch, sizeof(char));
}

int task_runner::set_thread_id(int id)
{
	thread_id = id;
	return 0;
}