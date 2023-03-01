#pragma once
#ifndef DISPATCH_TASK
#define DiSPATCH_TASK
#include <cstdio>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <map>
#include <unordered_map>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "RabbitmqClient.h"
#include "ThirdLib/nlohmann/json.hpp"
#include "task_runner.h"
#include "epoll_helper.h"
class dispatcher;
using nlohmann::json;
using std::string;
using recevie_msg = std::vector<string>;
using std::make_shared;
using callback = int (*)(string& input, string& output, int cnt);

const auto CORE_NUM = sysconf(_SC_NPROCESSORS_ONLN);// 核心数
constexpr int MB = 1024 * 1024;
/**
 * @brief 第一版
*/
class dispatcher :public CRabbitmqClient
{
public:
	struct names {
		string queue, exchange, routing_key;
	};
	/**
	 * @brief 应答结点
	*/
	struct ack_struct {
		std::atomic<bool> finished;
		amqp_envelope_t* envelope;
		task_runner* task; // 暂存task
		string uuid;
		ack_struct(bool finish, amqp_envelope_t* _envelope) :finished(finish), envelope(_envelope), task(nullptr), uuid() {
		}
		ack_struct() :ack_struct(false, nullptr) {
		}
	};

	dispatcher();
	~dispatcher();

	int Connect(cstr host, int port, cstr user, cstr password, int channel);

	int Disconnect();

	int OpenChannel();

	int CloseChannel();
	/**
	 * @brief 初始化，包括声明交换机和队列并绑定它们
	 * @param exchange_name 交换机名称
	 * @param queue_name 队列名称
	 * @param routing_key 映射键
	 * @param mode 交换机模式 direct fanout 等等，具体看rabbitmq
	 * @return 成功：0，失败其他
	*/
	int Init(cstr exchange_name, cstr queue_name, cstr routing_key, cstr mode);
	/**
	 * @brief
	 * @param msg
	 * @return
	*/
	int PublishMsg(string& msg);

	void setNames(cstr exchange, cstr queue, cstr routing_key, int is_publisher);

	void setThreadCnt(int id);

	int get_conn_fd();
public:
	names consumer, publisher;
	int thread_cnt; // 标识为 线程编号
protected:
	static bool close_flag; // 退出标记
};

/**
 * @brief V2
 *
*/
class dispatcherV2 :public dispatcher, public epoll_base
{
public:
	std::atomic<int> success_count, fail_compile; // 测试用，记录成功数
public:

	virtual int run_io(int fd) override;
	virtual int run_signal(int signal) override;
	// TODO 完成退出相关操作
	virtual int before_close(int fd) override;

	int get_recv_fd();
	/**
	 * @brief
	 * @param thread_num 线程池中线程数量，应当不大于核心数
	 * 应保留一个核心给主线程与rabbitmq io
	 * @param prefetch_count rabbitMq偏好获取消息数量
	*/
	dispatcherV2(int thread_num, int prefetch_count);
	~dispatcherV2();

	int send_msg();

	int publish_msg(string& msg, amqp_envelope_t* envelope);

	/**
	 * @brief 输出rabbitmq的错误信息
	 * @param ret
	 * @return
	*/
	int str_amqp_errors(amqp_rpc_reply_t& ret);

	int free_pointer();

	int consumer_init();

	int consume_msg(const timeval* timeout = nullptr);
	/**
	 * @brief 等待发送完所有消息
	*/
	void wait_for_send();

private:
	bool check_close();

	void consumer_cancel(amqp_bytes_t consumer_tag);

	int release_buffer();
	/**
	 * @brief 不同task_mode下的分配不同的任务
	 * @param task_mode
	 * @param task
	 * @return
	*/
	int append_task_run(bool task_mode, task_runner* task);
	/**
	 * @brief 合并任务并释放相关内存
	 * @param task
	 * @param envelope
	 * @return
	*/
	int merge_task(task_runner* task);
	/**
	 * @brief 任务信息初始化并对齐，加入对应映射中
	 * @param js
	 * @param envelope
	 * @param task_mode
	 * @return
	*/
	int add_to_map(pjson& js, amqp_envelope_t* envelope, bool task_mode);
	/**
	 * @brief 处理结果，写入相应的信息
	 * @param task_mode
	 * @param task
	 * @return
	*/
	int handle_task(bool task_mode, task_runner* task);

	int try_send_result(bool task_mode);

	dispatcherV2(const dispatcherV2&) = delete;
private:
	int m_pipefd[2]; // 用于通知线程发送结果
	int m_prefetch_count, m_thread_num; // rabbitmq期望每次获取的消息数量, 线程数
	bool task_mode;// 测试点模式尚未完成
	amqp_basic_consume_ok_t* m_consumer_rpy; // 记录相应队列的consumer tag
	std::unordered_map<std::string, pjson>m_result_map; // 以测试点为最小单位时，需要整合后再发送(uuid,res)
	std::unordered_map<std::string, ack_struct*> m_envelope_map;// 应答的映射，辅助修改对应应答
	block_queue<task_runner> m_free_queue; // 空闲的
	block_queue<task_runner> m_result_queue; // 中转用
	worker<task_runner> m_worker;// 待处理任务队列
	task_runner* m_runner;// 预分配任务
	block_queue<ack_struct> m_envelope_queue;	// 由于应答机制需要有序应答，故使用队列保存
	sem m_aval_ack_stat; // 可用应答
	scmp_filter_ctx m_scmp_ctx; // seccomp
};

#endif // DISPATCH_TASK
