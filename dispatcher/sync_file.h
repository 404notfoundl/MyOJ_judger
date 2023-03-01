#pragma once
#ifndef SYNC_FILE
#define SYNC_FILE
#include "connection_pool.h"
#include "hi_redis_helper.h"
#include "judger.h"
#include "serializer.h"
#include "ThirdLib/nlohmann/json.hpp"
#include "epoll_helper.h"
#include <vector>
#include <string>
#include <pthread.h>
#include <sys/socket.h>
using std::string;
using std::vector;
/**
 * @brief 同步测试点及spj程序
 * @todo 同步文件逻辑存在问题，待修改
*/
class sync_file : public epoll_base
{
public:

	virtual int run_io(int fd) override;
	virtual int run_signal(int signal) override;
	static int sync();
	static sync_file& get_instance();
	int set_redis(hiredis_helper* _redis);
	int get_redis_fd();
	int start_sync();
	/**
	 * @brief 存在spj程序则编译
	 * @param js
	*/
	static int try_compile(json& js);
private:
	sync_file();
	~sync_file();
	sync_file(const sync_file&);
	sync_file& operator=(const sync_file&) = delete;
private:
	// 在对应文件开头
	const static std::string sync_cmd;
	const static std::string spj_file_list;
	const static std::string all_spj_list;
	const static std::string all_files;
	hiredis_helper* redis;
};
#endif // !SYNC_FILE
