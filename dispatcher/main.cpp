#include "dispatch_task.h"
#include "sync_file.h"
#include "epoll_helper.h"
#include <cstdio>
#include <pthread.h>
#include <thread>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <wait.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sched.h>
#include <map>
#include <libcgroup.h>
#include <sys/socket.h>
#include <sys/syscall.h>
// 记录各个进程pid
constexpr cstr syncer_name = "syncer.pid";
constexpr cstr judger_name = "judger.pid";
constexpr cstr shm_sem = "/home/shm/judger_shm";
constexpr cstr exchange_name = "default";

#ifdef TEST_RABBITMQ // 本地连这个
constexpr cstr queue_name_task = "test_queue_task";
constexpr cstr routing_key_task = "test_queue_task";
constexpr cstr queue_name_result = "test_queue_result";
constexpr cstr routing_key_result = "test_queue_result";
#else
constexpr cstr queue_name_task = "queue_task";
constexpr cstr routing_key_task = "queue_task";
constexpr cstr queue_name_result = "queue_result";
constexpr cstr routing_key_result = "queue_result";
#endif // TEST_RABBITMQ

class config
{
public:
	static string rabbitMq_host;
	static int rabbitMq_port;
	static string rabbitMq_user;
	static string rabbitMq_password;

	static string redis_host;
	static int redis_port;
	static string redis_password;
};
string config::rabbitMq_host;
int config::rabbitMq_port = 0;
string config::rabbitMq_user;
string config::rabbitMq_password;
string config::redis_host;
int config::redis_port = 0;
string config::redis_password;
void init_config() {
#ifdef SSH // 没有环境变量的时候
	config::rabbitMq_host = "rabbitmq";
	config::rabbitMq_port = 5672;
	config::rabbitMq_user = "admin";
	config::rabbitMq_password = "xy123456";
	config::redis_host = "redis";
	config::redis_port = 6379;
	config::redis_password = "xy123456";
#else
	config::rabbitMq_host = getenv("RABBITMQ_HOST");
	config::rabbitMq_port = atoi(getenv("RABBITMQ_PORT"));
	config::rabbitMq_user = getenv("RABBITMQ_USER");
	config::rabbitMq_password = getenv("RABBITMQ_PASSWORD");
	config::redis_host = getenv("REDIS_HOST");
	config::redis_port = atoi(getenv("REDIS_PORT"));
	config::redis_password = getenv("REDIS_PASSWORD");
#endif // SSH 
}

int init_cg() {
	if (cgroup_init()) {
		LOGGER_ERR("cgroup init failed, {}", cgroup_strerror(cgroup_get_last_errno()));
		exit(dispatcher_errors::InitCgroupFailed);
	}
	return 0;
}

void write_pid(const char* file_name) {
	pid_t pid = getpid();
	file_util::write_data(file_name, std::to_string(pid));
}

int epoll_start(dispatcherV2* dis, epoll_helper* ep) {
	INFO("pid: {}", getpid());
	epoll_event ev;
	dis->consumer_init();
	ev.data.fd = dis->get_conn_fd();
	ev.events = EPOLLIN | EPOLLERR;
	ep->add(dis->get_conn_fd(), &ev, dynamic_cast<epoll_base*>(dis));
	ev.data.fd = dis->get_recv_fd();
	ep->add(dis->get_recv_fd(), &ev, dynamic_cast<epoll_base*>(dis));
	redis_params p(config::redis_host.c_str(), config::redis_port, config::redis_password.c_str(), { 10,0 });
	hiredis_helper helper(p);
	helper.connect();
	sync_file::get_instance().set_redis(&helper);
	sync_file::get_instance().start_sync();
	ev.data.fd = helper.get_fd();
	ep->add(helper.get_fd(), &ev, dynamic_cast<epoll_base*>(&sync_file::get_instance()));

	ep->loop();

	return 0;
}

int dispatcher_init(dispatcherV2& client) {
	if (client.Connect(config::rabbitMq_host.c_str(), config::rabbitMq_port, config::rabbitMq_user.c_str(), config::rabbitMq_password.c_str(), 1)) {
		LOGGER_ERR("connect rabbitMq failed!");
		return dispatcher_errors::ConnectFailed;
	}
	// 接收任务队列
	if (client.Init(exchange_name, queue_name_task, routing_key_task, "direct")) {
		LOGGER_ERR("init task exchange and queue failed");
		return 0;
	}
	// 返回结果队列
	if (client.Init(exchange_name, queue_name_result, routing_key_result, "direct")) {
		LOGGER_ERR("init result exchange and queue failed");
		return 0;
	}
	client.setNames(exchange_name, queue_name_task, routing_key_task, 0);
	client.setNames(exchange_name, queue_name_result, routing_key_result, 1);
	client.OpenChannel();
	return 0;
}

// TODO 细分此容器可用的权限 man page: capabilities
#define CLOC //clock 计时
int main()
{
	epoll_helper ep(20);
	logger::init();
	init_cg();
	init_config();

	write_pid(judger_name);
	INFO("start judger");
#ifdef CLOCK
	timeval start, end;
	gettimeofday(&start, NULL);
#endif // CLOCK
	int cores = CORE_NUM > 1 ? CORE_NUM - 1 : 1;
	//cores = 1;
	dispatcherV2 client(cores, cores * 2);
	dispatcher_init(client);
	int rtn = epoll_start(&client, &ep);
	client.CloseChannel();
	client.Disconnect();
#ifdef CLOCK
	gettimeofday(&end, NULL);
	INFO("total time use: {}", 1000 * (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000);
#endif // CLOCK
	client.free_pointer();
	if (rtn)LOGGER_ERR("run dispatcher err, code:{}", rtn);

	logger::drop(ERR_LOGGER_NAME);
	return 0;
}