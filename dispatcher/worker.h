#pragma once
#ifndef WORKER
#define WORKER
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif // !_GNU_SOURCE
#include <list>
#include <iostream>
#include <vector>
#include <queue>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <memory>
#include <signal.h>
#include <atomic>
#include "lock/locker.h"
#include "ThirdLib/nlohmann/json.hpp"
#include "logger.h"
using json = nlohmann::json;
using pjson = std::shared_ptr<json>;
using std::cout;
using std::endl;

template<typename T>
class block_list {
public:
	block_list(int max_size);
	~block_list();

	/**
	* @brief 堵塞式
	* @return 无返回空
	*/
	T* front_and_pop();
	/**
	 * @brief 非堵塞式
	 * @return 无返回空
	*/
	T* try_front_and_pop();
	/**
	 * @brief 堵塞式
	 * @return
	*/
	T* back_and_pop();
	/**
	 * @brief 首部
	 * @param data
	 * @return
	*/
	bool emplace_front(T* data);
	/**
	 * @brief 尾部
	 * @param data
	 * @return
	*/
	bool emplace(T* data);
private:
	std::list<T*> m_list; // 请求链表
	locker m_list_locker;       //保护队列的互斥锁
	sem m_list_stat;            //是否有任务需要处理，值标记待处理任务数量
	int max_size;
};

template<typename T>
class block_queue
{
public:
	block_queue(size_t max_size);
	~block_queue();
	/**
	 * @brief
	 * @return
	*/
	const T* front();
	/**
	 * @brief
	 * @return 无返回nullptr
	*/
	T* front_and_pop();
	/**
	 * @brief 非堵塞式
	 * @return 无返回nullptr
	*/
	T* try_front_and_pop();
	/**
	 * @brief 入队
	 * @param data
	 * @return
	*/
	bool emplace(T* data);
	/**
	 * @brief
	 * @return
	*/
	size_t size();
	/**
	 * @brief
	 * @return
	*/
	size_t get_max_size();
	/**
	 * @brief debug
	 * @param addr
	*/
	void set_addr(std::pair<T*, T*> addr);

	void clear();
private:
	std::queue<T*> m_queue;
	locker m_queue_locker;
	sem m_queue_stat;
	size_t max_size;
	std::pair<T*, T*> valid_addr; // debug 用
};

template<typename T>
class worker
{
public:
	worker(int thread_number, int max_size);
	/**
	 * @brief 设置相应的线程亲和性
	 * @param thread_number
	 * @param max_size
	 * @param core_num core_stat的长度
	 * @param core_stat 为1的位表示启用亲和性
	*/
	worker(int thread_number, int max_size, int core_num, int core_stat);
	~worker();
	/**
	 * @brief debug用
	 * @param addr
	*/
	void set_addr(std::pair<T*, T*> addr);
	bool append(T* task);
	int exit_all();
private:
	static void* work(void* ptr);
	void run(int thread_id);
	static void exit_thread(int sig);
private:
	int m_thread_number;        //线程池中的线程数
	int m_core_num, m_core_stat;	//若设置亲和性则需要这些
	pthread_t* m_threads;       //描述线程池的数组，其大小为m_thread_number
	block_queue<T> m_queue;		//任务的队列
	std::atomic<int> used_cnt; // 测试用，使用的数量
};
//----------------------------------------BLOCK_QUEUE---------------------------------------
template<typename T>
inline block_queue<T>::block_queue(size_t _max_size) :max_size(_max_size)// 0表示不限制
{
}

template<typename T>
inline block_queue<T>::~block_queue()
{
}

template<typename T>
inline const T* block_queue<T>::front()
{
	m_queue_locker.lock();
	if (m_queue.empty()) {
		m_queue_locker.unlock();
		return nullptr;
	}
	T* rtn = m_queue.front();
	m_queue_locker.unlock();
	return rtn;
}

template<typename T>
inline T* block_queue<T>::front_and_pop()
{
	m_queue_stat.wait();
	m_queue_locker.lock();
	if (m_queue.empty()) {
		m_queue_locker.unlock();
		return nullptr;
	}
	T* rtn = m_queue.front();
	m_queue.pop();
	m_queue_locker.unlock();
	return rtn;
}

template<typename T>
inline T* block_queue<T>::try_front_and_pop()
{
	if (!m_queue_stat.trywait())
		return nullptr;
	m_queue_locker.lock();
	if (m_queue.empty()) {
		m_queue_locker.unlock();
		return nullptr;
	}
	T* rtn = m_queue.front();
	m_queue.pop();
	m_queue_locker.unlock();
	return rtn;
}

template<typename T>
inline bool block_queue<T>::emplace(T* data)
{
	m_queue_locker.lock();
	if (!(max_size == 0 || m_queue.size() <= max_size))
	{
		m_queue_locker.unlock();
		return false;
	}
	if (valid_addr.first > data || valid_addr.second < data) {
		if (!(valid_addr.first == nullptr && valid_addr.second == nullptr)) {
			LOGGER_ERR("invalid addr");
			m_queue_locker.unlock();
			return false;
		}
	}
	m_queue.emplace(data);
	m_queue_locker.unlock();
	m_queue_stat.post();
	return true;
}

template<typename T>
inline size_t block_queue<T>::size()
{
	size_t size = 0;
	m_queue_locker.lock();
	if (!(max_size == 0 || m_queue.size() <= max_size))
	{
		m_queue_locker.unlock();
		return 0;
	}
	size = m_queue.size();
	m_queue_locker.unlock();
	return size;
}

template<typename T>
inline size_t block_queue<T>::get_max_size()
{
	return max_size;
}

template<typename T>
inline void block_queue<T>::set_addr(std::pair<T*, T*> addr)
{
	valid_addr = addr;
}

template<typename T>
inline void block_queue<T>::clear()
{
	while (!m_queue.empty())m_queue.pop();
	m_queue_stat.init(0);
}

// -----------------------------------BLOCK_LIST---------------------------------------
template<typename T>
inline block_list<T>::block_list(int max_size) :max_size(max_size)
{
}

template<typename T>
inline block_list<T>::~block_list()
{
}

template<typename T>
inline T* block_list<T>::front_and_pop()
{
	m_list_stat.wait();
	m_list_locker.lock();
	if (m_list.empty()) {
		m_list_locker.unlock();
		return nullptr;
	}
	T* rtn = m_list.front();
	m_list.pop_front();
	m_list_locker.unlock();
	return rtn;
}

template<typename T>
inline T* block_list<T>::try_front_and_pop()
{
	if (!m_list_stat.trywait())
		return nullptr;
	m_list_locker.lock();
	if (m_list.empty()) {
		m_list_locker.unlock();
		return nullptr;
	}
	T* rtn = m_list.front();
	m_list.pop_front();
	m_list_locker.unlock();
	return rtn;
}

template<typename T>
inline T* block_list<T>::back_and_pop()
{
	m_list_stat.wait();
	m_list_locker.lock();
	if (m_list.empty()) {
		m_list_locker.unlock();
		return nullptr;
	}
	T* rtn = m_list.back();
	m_list.pop_back();
	m_list_locker.unlock();
	return rtn;
}

template<typename T>
inline bool block_list<T>::emplace_front(T* data)
{
	if (data == nullptr)
		return false;
	m_list_locker.lock();
	if (!(max_size == 0 || m_list.size() <= max_size))
	{
		m_list_locker.unlock();
		return false;
	}
	m_list.emplace_front(data);
	m_list_locker.unlock();
	m_list_stat.post();
	return true;
}

template<typename T>
inline bool block_list<T>::emplace(T* data)
{
	if (data == nullptr)
		return false;
	m_list_locker.lock();
	if (!(max_size == 0 || m_list.size() <= max_size))
	{
		m_list_locker.unlock();
		return false;
	}
	m_list.emplace_back(data);
	m_list_locker.unlock();
	m_list_stat.post();
	return true;
}
// --------------------------------WORKER----------------------------------------
template<typename T>
inline worker<T>::worker(int thread_number, int max_size) :m_thread_number(thread_number), m_queue(max_size)
{
	assert(thread_number > 0);
	assert(max_size > -1);
	m_threads = new pthread_t[thread_number];
	assert(m_threads);
	for (int i = 0; i < thread_number; ++i)
	{
		std::pair<worker*, int>* arg = new std::pair<worker*, int>({ this,i });
		if (pthread_create(m_threads + i, nullptr, work, arg) != 0)
		{
			delete arg;
			delete[] m_threads;
			throw std::exception();
		}
	}
}

template<typename T>
inline worker<T>::worker(int thread_number, int max_size, int core_num, int core_stat) :worker(thread_number, max_size)
{
	assert(core_num > 0);
	m_core_num = core_num;
	m_core_stat = core_stat;
	int b = 0;
	// 各个线程绑定一个核心
	for (int a = 0; a < thread_number; a++) {
		cpu_set_t mask;
		CPU_ZERO(&mask);

		for (; b < core_num; b++) {
			if ((1 << b) & core_stat) {
				CPU_SET(b, &mask);
				b++;
				break;
			}
		}

		if (pthread_setaffinity_np(m_threads[a], sizeof(mask), &mask) < 0) {
			delete[] m_threads;
			throw std::exception();
		}
	}
}

template<typename T>
inline worker<T>::~worker()
{
	exit_all();
	delete[] m_threads;
}

template<typename T>
inline void worker<T>::set_addr(std::pair<T*, T*> addr)
{
	m_queue.set_addr(addr);
}

template<typename T>
inline bool worker<T>::append(T* task)
{
	used_cnt++;
	return m_queue.emplace(task);
}

template<typename T>
inline int worker<T>::exit_all()
{
	T tem[m_thread_number];
	for (int i = 0; i < m_thread_number; i++) {
		T::exit_flag = true;
		append(&tem[i]);
	}

	for (int i = 0; i < m_thread_number; ++i)
		pthread_join(m_threads[i], nullptr);
	delete[] m_threads;
	m_threads = nullptr;
	return 0;
}

template<typename T>
inline void* worker<T>::work(void* ptr)
{
	std::pair<worker*, int>* p = (std::pair<worker*, int>*)ptr;
	int thread_id = p->second;
	worker* w = p->first;
	delete p;
	//设置调度策略，FIFO
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
	//设置优先级
	sched_param param;
	param.__sched_priority = sched_get_priority_max(SCHED_FIFO);
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
	pthread_attr_destroy(&attr);

	w->run(thread_id);
	return nullptr;
}

template<typename T>
inline void worker<T>::run(int thread_id)
{
	while (true) {
		T* p = m_queue.front_and_pop();
		used_cnt--;
		if (p == nullptr) {
			break;
		}
		if (T::exit_flag) {
			break;
		}
		p->set_thread_id(thread_id);
		p->start();
	}
	INFO("worker thread {} exit", thread_id);
	return;
}

template<typename T>
inline void worker<T>::exit_thread(int sig)
{
	if (sig == SIGINT)
		pthread_exit(0);
}
#endif // !WORKER
