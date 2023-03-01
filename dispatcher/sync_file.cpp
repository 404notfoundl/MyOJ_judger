#include "sync_file.h"

const std::string sync_file::sync_cmd = "rsnapshot backup";
const std::string sync_file::spj_file_list = "spj_list"; // 新的spj列表
const std::string sync_file::all_spj_list = "all_spj_list";// 全部spj列表，后端带了此前缀
const std::string sync_file::all_files = "all_files"; // 全部的spj文件仅启动时编译，新的接收时编译

sync_file::sync_file() : redis(nullptr)
{
}

sync_file::~sync_file()
{
}

int sync_file::run_io(int fd)
{
	sync();
	redis->get_reply(try_compile);
	return 0;
}

int sync_file::run_signal(int signal)
{
	return 0;
}

int sync_file::sync()
{
	system(sync_cmd.c_str());
	return 0;
}

sync_file& sync_file::get_instance()
{
	static sync_file instance;
	return instance;
}

int sync_file::set_redis(hiredis_helper* _redis)
{
	redis = _redis;
	fcntl(redis->get_fd(), F_SETFL, fcntl(redis->get_fd(), F_GETFL, 0) | SOCK_CLOEXEC);
	return 0;
}

int sync_file::get_redis_fd()
{
	return redis->get_fd();
}

int sync_file::start_sync()
{
	if (redis == nullptr) {
		WARN("redis is null");
		return 1;
	}
	INFO("sync file");
#ifdef TEST_RABBITMQ
	redis->select(0);
#else
	redis->select(1);
#endif // TEST_RABBITMQ
	redis->get(all_spj_list);
	sync();
	try_compile(redis->response);
	redis->subscribe(spj_file_list);
	return 0;
}

int sync_file::try_compile(json& js)
{
	/*
	{
		`spj_file_list`:[{pid:0,cid:0},{...}]
	}
	*/
	json data;
	bool all_files_flag = false;

	vector<json> spj_list;
	//INFO("test sync {}", js.dump());
	if (js.contains("json"))data = js["json"][0]; // 新文件
	else data = js; // 全部文件

	if (!data.contains(spj_file_list))
		return 1;
	if (data.contains(all_files))
		data.at(all_files).get_to(all_files_flag);
	spj_list = data.at(spj_file_list).get<vector<json>>();
	for (auto item : spj_list) {
		int pid = item.at("pid").get<int>();
		// TODO 原因同下
		string out_file(ROOT_PATH_PROBLEMS);
		out_file.append("P").append(std::to_string(pid)).append("/");
		out_file.append("checker");
		// 原本想用 serializer.h is_path_exist() 但是开启 O2 找不到，待查明原因
		if (access(out_file.c_str(), F_OK) || !all_files_flag) {
			// checker编译相关信息
			item["lang"] = "cpp11";
			item["uid"] = 1;
			item["O2"] = true;
			item["des_path"] = out_file;
			// checker.cpp 已经同步至本地
			item["src_path"] = out_file + ".cpp";
			item["err_path"] = out_file + ".err";
			item["cg_name"] = "compilecgroup";
			// 服务器编译通过才会通知同步文件，一般不会出现问题
			if (judger::compiler::start(item) != judger::exec_base::AC) {
				LOGGER_ERR("compile checker error, pid: {}", item.at("pid").get<int>());
				LOGGER_ERR("compile args: {}", item.dump().c_str());
				exit(-1);
			}
			INFO("spj file :P{} compile success", item.at("pid").get<int>());
			item.clear();
		}
	}
	return 0;
}