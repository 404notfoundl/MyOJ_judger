#include "hi_redis_helper.h"

hiredis_helper::hiredis_helper(redis_params info) :login(info), conn(NULL), conn_status(0)
{
}

hiredis_helper::hiredis_helper() : login(), conn(NULL), conn_status(0)
{
}

hiredis_helper::~hiredis_helper()
{
	if (conn) redisFree(conn), conn = nullptr;
}

int hiredis_helper::init(const redis_params& p)
{
	login = p;
	return 0;
}

int hiredis_helper::connect()
{
	if (login.timeout.tv_sec == 0 && login.timeout.tv_usec == 0) {
		if ((conn = redisConnect(login.hostname.c_str(), login.port)) == NULL || conn->err) {
			if (conn) WARN("connect redis failed, {}", conn->errstr), redisFree(conn), conn = NULL;
			else WARN("allocate redis context failed");
			return -1;
		}
	}
	else {
		if ((conn = redisConnectWithTimeout(login.hostname.c_str(), login.port, login.timeout)) == NULL || conn->err) {
			if (conn)WARN("connect redis failed, {}", conn->errstr), redisFree(conn), conn = NULL;
			else WARN("allocate redis context failed");
			return -1;
		}
	}
	if (login.password.length() > 0) {
		if (auth())
			return -1;
	}
	retryed = 0;
	return 0;
}

int hiredis_helper::disconnect()
{
	return 0;
}

int hiredis_helper::get_fd()
{
	return conn->fd;
}

int hiredis_helper::auth()
{
	if (login.password.length() > 0) {
		redisReply* reply = (redisReply*)redisCommand(conn, "auth %s", login.password.c_str());
		if (reply->type == REDIS_REPLY_ERROR)
		{
			WARN("auth failed");
			freeReplyObject(reply);
			return -1;
		}
		freeReplyObject(reply);
	}
	return 0;
}

int hiredis_helper::select(int db)
{
	exec_cmd("select %d", db);
	return 0;
}

int hiredis_helper::set(const string& key, const string& value)
{
	exec_cmd("set %s %s", key.c_str(), value.c_str());
	return 0;
}

int hiredis_helper::set(const string& key, const string& value, int seconds)
{
	int res = set(key, value);
	if (res)return res;
	res = expire(key, seconds);
	return res;
}

int hiredis_helper::get(const string& key)
{
	exec_cmd("get %s", key.c_str());
	return 0;
}

int hiredis_helper::get_reply(int(*callback)(json&))
{
	redisReply* reply = NULL;
	int exit_flag = 0;
	if (redisGetReply(conn, (void**)(&reply)) == REDIS_OK) {
		if (get_result(reply)) {
			exit_flag = callback(response);
		}
		free_reply(&reply);
	}
	else {
		exit_flag = 1;
	}
	return exit_flag;
}

int hiredis_helper::subscribe(const string& key)
{
	exec_cmd("subscribe %s", key.c_str());
	return 0;
}

int hiredis_helper::subscribe_loop(const string& key, int(*callback)(json&))
{
	exec_cmd("subscribe %s", key.c_str());
	int run_flag = 1;
	while (run_flag) {
		run_flag = !get_reply(callback);
	}
	return 0;
}

int hiredis_helper::publish(const string& key, const string& value)
{
	exec_cmd("publish %s %s", key.c_str(), value.c_str());
	return 0;
}

int hiredis_helper::expire(const string& key, int seconds)
{
	exec_cmd("expire %s %d", key.c_str(), seconds);
	return 0;
}

int hiredis_helper::incr(const string& key)
{
	exec_cmd("incr %s", key.c_str());
	return 0;
}

int hiredis_helper::decr(const string& key)
{
	exec_cmd("decr %s", key.c_str());
	return 0;
}

bool hiredis_helper::check_status()
{
	redisReply* reply = (redisReply*)redisCommand(conn, "ping");
	bool res = get_result(reply);
	while (login.max_retry > retryed && conn_status & REDIS_DISCONNECTING) {
		if (!redisReconnect(conn)) {
			auth();
			retryed = 0;
			conn_status &= ~REDIS_DISCONNECTING;// 复位
			break;
		}
		retryed++;
	}
	if (login.max_retry == retryed) {
		res = false;
		WARN("reconnect redis failed\n");
	}
	free_reply(&reply);
	if (retryed >= login.max_retry)res = false;
	return res;
}

bool hiredis_helper::exec_cmd(std::string pattern, ...)
{
	response.clear();
	if (!check_status())
		return false;
	va_list params;
	va_start(params, pattern);
	redisReply* reply = (redisReply*)redisvCommand(conn, pattern.c_str(), params);
	va_end(params);
	auto res = get_result(reply);
	free_reply(&reply);
	return res;
}

void hiredis_helper::append_res(json& src, const char* key, json& value)
{
	if (!src.contains(key)) {
		src[key] = std::vector<json>();
	}
	src[key].push_back(value);
}
bool hiredis_helper::get_result(redisReply* reply)
{
	bool res = true;
	response.clear();
	if (reply != NULL) {
		switch (reply->type)
		{
		case REDIS_REPLY_INTEGER:
		case REDIS_REPLY_STRING:
		case REDIS_REPLY_ARRAY:
			response = parse_result(reply);
			break;
		case REDIS_REPLY_NIL:
			INFO("key is not exist");
			res = false;
			break;
		case REDIS_REPLY_STATUS: // OK
			break;
		case REDIS_REPLY_ERROR:
			response["err"] = std::string(reply->str, reply->len);
			LOGGER_ERR("reply error: {}", response.at("err").get<std::string>());
			res = false;
			break;
		default:
			WARN("can't read this type");
			res = false;
			break;
		}
		//freeReplyObject(reply);
	}
	else if (conn->err == REDIS_ERR_IO || conn->err == REDIS_ERR_EOF) {
		conn_status |= REDIS_DISCONNECTING; // 需要重连
		INFO("%s", conn->errstr);
	}
	else { // 其他错误
		res = false;
	}
	return res;
}
json hiredis_helper::parse_result(redisReply* reply)
{
	json js;
	switch (reply->type)
	{
	case REDIS_REPLY_STRING:
		try
		{
			js = json::parse(std::string(reply->str, reply->len));
		}
		catch (const std::exception&)
		{
			js = std::string(reply->str, reply->len);
		}
		break;
	case REDIS_REPLY_INTEGER:
		js = reply->integer;
		break;
	case REDIS_REPLY_ARRAY:
	{
		for (size_t a = 0; a < reply->elements; a++) {
			json tem = parse_result(reply->element[a]);
			if (tem.is_string())append_res(js, "str", tem);
			else if (tem.is_array())append_res(js, "arr", tem);
			else if (tem.is_number_integer())append_res(js, "int", tem);
			else if (tem.is_object())append_res(js, "json", tem);
		}
		break;
	}
	default:
		WARN("can't read this type\n");
		break;
	}
	return js;
}

void hiredis_helper::free_reply(redisReply** reply)
{
	if (*reply != nullptr)freeReplyObject(*reply), * reply = nullptr;
}