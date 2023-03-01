#pragma once
#ifndef HI_REDIS_HELPER
#define HI_REDIS_HELPER
#include <iostream>
#include "ThirdLib/hiredis/hiredis.h"
#include "ThirdLib/nlohmann/json.hpp"
#include "connection_pool.h"
#include "epoll_helper.h"
using nlohmann::json;
using std::string;

class redis_params {
public:
	string hostname, password;
	int port, max_retry;
	timeval timeout;
	redis_params(string host = "127.0.0.1", int _port = 6379, string pwd = "", timeval _timeout = { 0,0 }, int _max_retry = 10) {
		hostname = host;
		password = pwd;
		port = _port;
		timeout = _timeout;
		max_retry = _max_retry;
	}
};

class hiredis_helper :connection_raw<redis_params>
{
public:

	hiredis_helper(redis_params info);
	hiredis_helper();
	~hiredis_helper();

	int init(const redis_params& p) override;
	int connect() override;
	int disconnect() override;
	int get_fd();

	int auth();
	int select(int db_num);
	int set(const string& key, const string& value);
	int set(const string& key, const string& value, int seconds);
	int get(const string& key);
	int get_reply(int(*callback)(json&));
	int subscribe(const string& key);
	int subscribe_loop(const string& key, int (*callback)(json&));
	int publish(const string& key, const string& value);
	int expire(const string& key, int seconds);
	int incr(const string& key);
	int decr(const string& key);

	bool check_status();
	bool exec_cmd(string pattern, ...);

public:
	json response;
private:
	void append_res(json& src, const char* key, json& value);
	bool get_result(redisReply* reply);
	json parse_result(redisReply* reply);
	void free_reply(redisReply** reply);
	hiredis_helper(const hiredis_helper&) = delete;
private:
	redis_params login;
	redisContext* conn;
	long long conn_status;// 连接状态标记
	int retryed;// 已重连次数
};
#endif // !HI_REDIS_HELPER
