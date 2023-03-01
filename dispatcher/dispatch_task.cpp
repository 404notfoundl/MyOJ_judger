#include "dispatch_task.h"
#include <stdlib.h>
bool dispatcher::close_flag = false;

dispatcher::dispatcher() :CRabbitmqClient()
{
	thread_cnt = 0;
}

dispatcher::~dispatcher() {
}

int dispatcher::Connect(cstr _host, int _port, cstr _user, cstr _password, int _channel) {
	auto res = CRabbitmqClient::Connect(_host, _port, _user, _password, _channel);
	return res;
}

int dispatcher::Disconnect() {
	if (nullptr != m_pConn) {
		CRabbitmqClient::Disconnect();
	}
	return 0;
}

int dispatcher::OpenChannel() {
	if (m_pConn == nullptr)
	{
		printf("can't get connection");
		return -1;
	}
	amqp_channel_open(m_pConn, m_iChannel);
	return 0;
}

int dispatcher::CloseChannel() {
	if (m_pConn == nullptr)
	{
		printf("can't get connection");
		return -1;
	}
	amqp_channel_close(m_pConn, m_iChannel, AMQP_REPLY_SUCCESS);
	if (0 != ErrorMsg(amqp_get_rpc_reply(m_pConn), "close channel")) {
		return -1;
	}
	return 0;
}

int dispatcher::Init(cstr exchange_name, cstr queue_name, cstr routing_key, cstr mode) {
	if (CRabbitmqClient::ExchangeDeclare(exchange_name, mode, 1))
		return -1;
	if (CRabbitmqClient::QueueDeclare(queue_name, 1))
		return -2;
	if (CRabbitmqClient::QueueBind(queue_name, exchange_name, routing_key))
		return -3;
	return 0;
}

int dispatcher::PublishMsg(string& msg)
{
	CRabbitmqClient::Publish(msg, publisher.exchange, publisher.routing_key);
	return 0;
}

void dispatcher::setNames(cstr exchange, cstr queue, cstr routing_key, int is_publisher)
{
	if (is_publisher) {
		publisher.exchange = exchange;
		publisher.queue = queue;
		publisher.routing_key = routing_key;
	}
	else {
		consumer.exchange = exchange;
		consumer.queue = queue;
		consumer.routing_key = routing_key;
	}
}

void dispatcher::setThreadCnt(int id)
{
	thread_cnt = id;
}

int dispatcher::get_conn_fd()
{
	return amqp_get_sockfd(m_pConn);;
}

//-----------------------------DispatcherV2--------------------------------------

int dispatcherV2::run_io(int fd)
{
	timeval timeout = { 0,1 };
	if (get_conn_fd() == fd) { // 接收
		//m_aval_ack_stat.wait();
		consume_msg(&timeout);
	}
	else if (get_recv_fd() == fd) { // 发送
		char ch = 0;
		while (true) {
			if (read(m_pipefd[0], &ch, sizeof(char)) < 0) {
				if (EAGAIN == errno)break;
			}
			send_msg();
		}
		check_close();
	}
	return 0;
}

int dispatcherV2::run_signal(int signal)
{
	return 0;
}

int dispatcherV2::before_close(int fd)
{
	close_flag = true;
	//consumer_cancel(m_consumer_rpy->consumer_tag); // TODO 关闭订阅
	check_close();
	return 0;
}

int dispatcherV2::get_recv_fd()
{
	return m_pipefd[0];
}

dispatcherV2::dispatcherV2(int thread_num, int prefetch_count) :
	dispatcher(), success_count(0), fail_compile(0), m_pipefd{ 0,0 },
	m_prefetch_count(prefetch_count), m_thread_num(thread_num), task_mode(true), m_consumer_rpy(nullptr),
	m_free_queue(thread_num* prefetch_count), m_result_queue(thread_num* prefetch_count),
	m_worker(thread_num, thread_num* prefetch_count/**/, thread_num, (1 << thread_num) - 1),
	m_runner(nullptr), m_envelope_queue(prefetch_count), m_aval_ack_stat(prefetch_count),
	m_scmp_ctx(nullptr)
{
	assert(thread_num > 0);
	assert(prefetch_count > 0);
	m_runner = new task_runner[thread_num * prefetch_count]();
	m_scmp_ctx = seccomp_init(SCMP_ACT_KILL);
	if (m_scmp_ctx == nullptr) {
		LOGGER_ERR("init scmp failed");
		throw std::exception();
	}
#ifdef JUDGE_DEBUGGER
	free_queue.set_addr({ m_runner,m_runner + thread_num * prefetch_count - 1 });
	result_queue.set_addr({ m_runner,m_runner + thread_num * prefetch_count - 1 });
	m_worker.set_addr({ m_runner,m_runner + thread_num * prefetch_count - 1 });
#endif // JUDGE_DEBUGGER
	if (pipe2(m_pipefd, O_NONBLOCK | O_CLOEXEC)) {
		LOGGER_ERR("set notice fd failed {}", strerror(errno));
		throw std::exception();
	}
	// task_runner 相关设置
	task_runner::notice_fd = m_pipefd[1];
	task_runner::resutl_queue = &m_result_queue;
	task_runner::success = &success_count;
	task_runner::m_ctx = m_scmp_ctx;
	for (int cnt = 0; cnt < thread_num * prefetch_count; cnt++) {
		m_free_queue.emplace(m_runner + cnt);
	}
}

dispatcherV2::~dispatcherV2()
{
	free_pointer();
}

int dispatcherV2::send_msg()
{
	while (true) {
		task_runner* task = m_result_queue.try_front_and_pop();
		if (task == nullptr) {
			return 0;
		}
		if (task_runner::exit_flag) {
			return 0;
		}
		pjson t = task->get_task();
		bool compile_flag, compile, run_flag;
		compile_flag = t->at("compile_flag").get<bool>();
		compile = t->at("compile").get<bool>();
		run_flag = t->at("run_flag").get<bool>();
		// 已编译，增加运行任务
		if (compile && compile_flag && !run_flag) {
			append_task_run(task_mode, task);
			return 0;
		}
		handle_task(task_mode, task);
		if (!task_mode)
			m_free_queue.emplace(task);
	}

	return 0;
}

int dispatcherV2::publish_msg(string& msg, amqp_envelope_t* envelope)
{
	if (msg.length() > 0)
		PublishMsg(msg);
	amqp_basic_ack(m_pConn, m_iChannel, envelope->delivery_tag, 1);
	amqp_destroy_envelope(envelope);
	return 0;
}

// TDOO 测试点为单位时需要合并，待完成
int dispatcherV2::merge_task(task_runner* task)
{
	bool compile, runner, finished = false;
	pjson rtn = task->get_task();
	string uuid = rtn->at("uuid").get<string>();
	auto res_ito = m_result_map.find(uuid);
	compile = rtn->at("compile").get<bool>();
	if (compile) {
		int start = rtn->at("start").get<int>();
		int task_num = rtn->at("task_number").get<int>();
		int task_length = res_ito->second->at("task_length").get<int>();
		int finished_len = res_ito->second->at("finished_len").get<int>();
		res_ito->second->at("finished_len") = finished_len + task_num;
		runner = rtn->at("runner").get<bool>();
		if (!runner)
			res_ito->second->at("runner") = false;
		string status = res_ito->second->at("status"), status_sub = rtn->at("status");
		for (int a = 0; a < task_num; a++) {
			int dx = start + a;
			status[dx] = status_sub[a];
			res_ito->second->at("details")[dx] = rtn->at("details")[a];
		}
		res_ito->second->at("status") = status;
		if (finished_len + task_num == task_length)
			finished = true;
	}
	else {
		res_ito->second->at("compile") = false;
		res_ito->second->at("runner") = false;
		res_ito->second->at("status") = "c";
		res_ito->second->at("details") = vector<string>({ "CE" });
		finished = true;
	}

	task->reset();
	return finished;
}

int dispatcherV2::add_to_map(pjson& js, amqp_envelope_t* envelope, bool task_mode)
{
	pjson tmp = make_shared<json>();
	string uuid = js->at("uuid").get<string>();
	if (!task_mode) {
		*tmp = json::parse("{\"compile\":true,\"details\":[],\"pid\":-1,\"runner\":true,\"status\":\"\",\"uid\":-1,\"uuid\":\"\",\"task_length\":0}");
		int task_length = js->at("task_number").get<int>();
		string status;
		std::vector<string> details;
		details.resize(task_length);
		for (int a = 0; a < task_length; a++) status.push_back(' ');
		tmp->at("status") = status;
		tmp->at("details") = details;
		tmp->at("pid") = js->at("pid").get<int>();
		tmp->at("uid") = js->at("uid").get<int>();
		tmp->at("uuid") = uuid;
		tmp->at("task_length") = task_length;
		(*tmp)["finished_len"] = 0; // 已完成长度
		m_result_map.insert({ uuid,tmp });
	}
	ack_struct* ptmp = new ack_struct;
	ptmp->envelope = envelope;
	ptmp->uuid = uuid;
	m_envelope_queue.emplace(ptmp);
	m_envelope_map.insert({ uuid,ptmp });
	return 0;
}

int dispatcherV2::str_amqp_errors(amqp_rpc_reply_t& ret)
{
	amqp_frame_t frame;
	if (AMQP_RESPONSE_LIBRARY_EXCEPTION == ret.reply_type) {
		switch (ret.library_error)
		{
		case AMQP_STATUS_UNEXPECTED_STATE:
			if (AMQP_STATUS_OK != amqp_simple_wait_frame(m_pConn, &frame)) {
				return AMQP_STATUS_UNEXPECTED_STATE;
			}
			if (AMQP_FRAME_METHOD == frame.frame_type) {
				switch (frame.payload.method.id) {
				case AMQP_BASIC_ACK_METHOD:
					/* if we've turned publisher confirms on, and we've published a
					 * message here is a message being confirmed.
					 */
					break;
				case AMQP_BASIC_RETURN_METHOD:
					/* if a published message couldn't be routed and the mandatory
					 * flag was set this is what would be returned. The message then
					 * needs to be read.
					 */
				{
					amqp_message_t message;
					ret = amqp_read_message(m_pConn, frame.channel, &message, 0);
					if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
						return dispatcher_errors::MsgRouteFailed;
					}

					amqp_destroy_message(&message);
				}
				break;
				case AMQP_CHANNEL_CLOSE_METHOD:
					/* a channel.close method happens when a channel exception occurs,
					 * this can happen by publishing to an exchange that doesn't exist
					 * for example.
					 *
					 * In this case you would need to open another channel redeclare
					 * any queues that were declared auto-delete, and restart any
					 * consumers that were attached to the previous channel.
					 */
				{
					amqp_channel_close_t* details = (amqp_channel_close_t*)frame.payload.method.decoded;
					string str((char*)details->reply_text.bytes, details->reply_text.len);
					LOGGER_ERR("channel close err: {}", str.c_str());
					return dispatcher_errors::ChannelException;
				}
				case AMQP_CONNECTION_CLOSE_METHOD:
					/* a connection.close method happens when a connection exception
					 * occurs, this can happen by trying to use a channel that isn't
					 * open for example.
					 *
					 * In this case the whole connection must be restarted.
					 */
				{
					amqp_connection_close_t* details = (amqp_connection_close_t*)frame.payload.method.decoded;
					string str((char*)details->reply_text.bytes, details->reply_text.len);
					LOGGER_ERR("connection close err: {}\n", str.c_str());
					return dispatcher_errors::ConnectionException;
				}

				default:
					LOGGER_ERR("An unexpected method was received {}",
						frame.payload.method.id);
					return dispatcher_errors::UnexpectError;
				}
			}
			else if (AMQP_FRAME_BODY == frame.frame_type) {
			}
			else if (AMQP_FRAME_HEADER == frame.frame_type) {
			}
		case AMQP_STATUS_TIMEOUT: // 等待消费超时
			return dispatcher_errors::None;
		case AMQP_STATUS_HEARTBEAT_TIMEOUT: // 心跳包超时
			return dispatcher_errors::HeartBeatTimeOut;
		default:
			LOGGER_ERR("amqp unknow error, reply type: {}", int(ret.reply_type));
			return ret.reply_type;
		}
	}
	else if (AMQP_RESPONSE_SERVER_EXCEPTION == ret.reply_type) {
		LOGGER_ERR("rabbitMq server error");
		ErrorMsg(ret, "server err");
		return dispatcher_errors::ServerException;
	}
	else {
		// 其他的情况
		return ret.reply_type;
	}
	return 0;
}

int dispatcherV2::free_pointer()
{
	if (m_runner) {
		delete[] m_runner;
		m_runner = nullptr;
		m_free_queue.clear();
	}

	if (m_pipefd[0] != 0) {
		close(m_pipefd[0]);
		m_pipefd[0] = 0;
	}
	if (m_pipefd[1] != 0) {
		close(m_pipefd[1]);
		m_pipefd[1] = 0;
	}
	if (m_scmp_ctx) {
		seccomp_release(m_scmp_ctx);
		m_scmp_ctx = nullptr;
	}
	return 0;
}

int dispatcherV2::append_task_run(bool task_mode, task_runner* task)
{
	auto runner_task = task;
	pjson data = runner_task->get_task();
	if (task_mode) {
		m_worker.append(runner_task);
	}
	else {
		int task_length = data->at("task_number").get<int>();
		int d_len = task_length / m_thread_num;
		d_len = d_len == 0 ? 1 : d_len;
		for (int start = 0; start < task_length; start += d_len) {
			int len = std::min(d_len, task_length - start);
			// 添加
			pjson sub_data = make_shared<json>(*data);
			sub_data->at("task_number") = len;
			(*sub_data)["start"] = start;
			runner_task->set_task(sub_data);
			m_worker.append(runner_task);
			if (start + d_len < task_length) {
				runner_task = m_free_queue.front_and_pop();
			}
		}
	}
	return 0;
}

void dispatcherV2::wait_for_send()
{
	int cnt = 0;
	while (true) {
		m_aval_ack_stat.wait();
		cnt++;
		if (cnt == m_prefetch_count)break;
	}
	return;
}

int dispatcherV2::handle_task(bool task_mode, task_runner* task)
{
	string uuid = task->get_task()->at("uuid").get<string>();
	auto map_ito = m_envelope_map.find(uuid);

	if (map_ito == m_envelope_map.end()) {
		LOGGER_ERR("can't get envelope");
		exit(dispatcher_errors::EnvelopeNotFoundError);
	}
	ack_struct* node = map_ito->second;
	if (task_mode) {
		node->finished = true;
		node->task = task;
	}
	else {
		if (merge_task(task)) {
			node->finished = true;
		}
	}

	try_send_result(task_mode);
	return 0;
}

int dispatcherV2::try_send_result(bool task_mode)
{
	while (true) {
		const ack_struct* front = m_envelope_queue.front();
		if (front != nullptr && front->finished) {
			auto map_ite = m_envelope_map.find(front->uuid);
			amqp_envelope_t* envelope = map_ite->second->envelope;
			string res;
			if (task_mode) {
				res = front->task->get_task()->dump();
				publish_msg(res, envelope);
			}
			else {
				auto map_index = m_result_map.find(front->uuid);
				res = map_index->second->dump();
				m_result_map.erase(map_index);
				publish_msg(res, envelope);
			}
			m_envelope_map.erase(map_ite);
			delete envelope;
			m_envelope_queue.front_and_pop();
			if (front->task != nullptr) {
				front->task->reset();
				if (task_mode)
					m_free_queue.emplace(front->task);
			}
			delete front;
		}
		else {
			break;
		}
	}
	return 0;
}

int dispatcherV2::consumer_init()
{
	cpu_set_t mask;
	CPU_ZERO(&mask);
	CPU_SET(m_thread_num, &mask);
	if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
		return -1;
	}

	amqp_basic_qos(m_pConn, m_iChannel, 0, m_prefetch_count, 0);
	int ack = 0; // no_ack    是否自动确认消息
	amqp_bytes_t queuename = amqp_cstring_bytes(consumer.queue.c_str());
	m_consumer_rpy = amqp_basic_consume(m_pConn, m_iChannel, queuename, amqp_empty_bytes, 0, ack, 0, amqp_empty_table);
	if (0 != ErrorMsg(amqp_get_rpc_reply(m_pConn), "Consuming")) {
		amqp_channel_close(m_pConn, m_iChannel, AMQP_REPLY_SUCCESS);
		LOGGER_ERR("basic consume error");
		return dispatcher_errors::BasicConsumeError;
	}
	fcntl(get_conn_fd(), F_SETFL, fcntl(get_conn_fd(), F_GETFL, 0) | SOCK_CLOEXEC);
	timeval timeout = { 1,0 };
	consume_msg(&timeout);
	return 0;
}

bool dispatcherV2::check_close()
{
	// 可用队列是满的
	if (close_flag && m_free_queue.get_max_size() == m_free_queue.size()) {
		kill(getpid(), SIGUSR1);// 暂时这样，通知已完成
		return true;
	}
	return false;
}

void dispatcherV2::consumer_cancel(amqp_bytes_t consumer_tag)
{
	amqp_basic_cancel(m_pConn, m_iChannel, consumer_tag);
}

int dispatcherV2::release_buffer()
{
	amqp_maybe_release_buffers(m_pConn);
	return 0;
}

int dispatcherV2::consume_msg(const timeval* timeout)
{
	while (true) {
		release_buffer();
		task_runner* runner_task = m_free_queue.try_front_and_pop();
		if (runner_task == nullptr)
			break;
		// TODO 此处改为shared_ptr
		amqp_envelope_t* envelope = new amqp_envelope_t;
		auto ret = amqp_consume_message(m_pConn, envelope, timeout, 0);
		if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
			int rtn = str_amqp_errors(ret);
			amqp_destroy_envelope(envelope);
			delete envelope;
			m_free_queue.emplace(runner_task);
			if (rtn)
				return rtn;
			else {
				break; // 无待处理内容
			}
		}
		else {
			string input((char*)envelope->message.body.bytes, (char*)envelope->message.body.bytes + envelope->message.body.len);
			//INFO(input);
			pjson data = make_shared<json>();
			*data = json::parse(input);
			(*data)["task_number"] = get_tests_count(*data);
			if ((*data)["task_number"] < 0) {
				return dispatcher_errors::TaskNumUnexpect;
			}
			add_to_map(data, envelope, task_mode);
			(*data)["task_mode"] = task_mode; // 任务为单位
			(*data)["compile_flag"] = false;
			(*data)["run_flag"] = false;
			runner_task->set_task(data);
			m_worker.append(runner_task);
		}
	}
	return 0;
}