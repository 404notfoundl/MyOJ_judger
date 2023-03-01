#pragma once
#ifndef CONNECTION_POOL
#define CONNECTION_POOL
#include "worker.h"
template<typename T, typename P>
class connection_pool;

/**
 * @brief 连接类
 * @tparam T 值类型
 * @tparam P 连接参数类型
*/
template<typename T, typename P>
class connection {
public:
	connection(T* t, connection_pool<T, P>* c);
	connection(connection<T, P>&& move)noexcept;
	~connection();
	connection<T, P>& operator=(connection<T, P>&& move)noexcept;
	T* operator ->()const noexcept;
	T& operator *()const noexcept;
	explicit operator bool() const noexcept;

	int emplace();
	void swap(connection<T, P>& p)noexcept;

	connection(const connection&)noexcept = delete;
	connection& operator=(const connection&)noexcept = delete;
protected:
	T* value;
	connection_pool<T, P>* pool;
};
/**
 * @brief 原生连接类
 * @tparam P
*/
template<typename P>
class connection_raw
{
public:
	// 需要实现以下函数
	virtual int connect() = 0;
	virtual int init(const P& params) = 0;
	virtual int disconnect() = 0;
};
/**
 * @brief 连接池
 * @tparam T
 * @tparam P
*/
template<typename T, typename P>
class connection_pool
{
public:
	/**
	 * @brief
	 * @param size 池大小
	 * @param params 初始参数
	*/
	connection_pool(size_t size, P params);
	~connection_pool();

	bool emplace(T* conn);
	T* pop();
	T* pop_raw();
	connection<T, P> pop_conn();

	//TODO 实现检测连接是否存活

	int destory_all();
private:
	T* connections;
	size_t pool_size;
	block_queue<T> pool;
};

template<typename T, typename P>
inline connection_pool<T, P>::connection_pool(size_t size, P params) : connections(nullptr), pool_size(size),
pool(pool_size)
{
	connections = new T[size];
	for (size_t a = 0; a < size; a++) {
		connections[a].init(params);
		if (connections[a].connect()) {
			fprintf(stderr, "connect failed!\n");
			throw std::exception();
		}
		pool.emplace(&connections[a]);
	}
}

template<typename T, typename P>
inline connection_pool<T, P>::~connection_pool()
{
	destory_all();
}

template<typename T, typename P>
inline bool connection_pool<T, P>::emplace(T* conn)
{
	pool.emplace(conn);
	return true;
}

template<typename T, typename P>
inline T* connection_pool<T, P>::pop()
{
	return pop_raw();
}

template<typename T, typename P>
inline T* connection_pool<T, P>::pop_raw()
{
	return pool.front_and_pop();
}

template<typename T, typename P>
inline connection<T, P> connection_pool<T, P>::pop_conn()
{
	return connection<T, P>(pop(), this);
}

template<typename T, typename P>
inline int connection_pool<T, P>::destory_all()
{
	if (connections == nullptr)return 0;
	for (size_t a = 0; a < pool_size; a++) {
		connections[a].disconnect();
	}
	pool.clear();
	delete[] connections;
	connections = nullptr;
	return 0;
}

//-------------------------------connection-----------------------------------------------
template<typename T, typename P>
inline connection<T, P>::connection(T* t, connection_pool<T, P>* c) :value(t), pool(c)
{
}

template<typename T, typename P>
inline connection<T, P>::connection(connection<T, P>&& move) noexcept :value(move.value), pool(move.pool)
{
	move.value = nullptr;
	move.pool = nullptr;
}

template<typename T, typename P>
inline connection<T, P>::~connection()
{
	emplace();
}

template<typename T, typename P>
inline connection<T, P>& connection<T, P>::operator=(connection<T, P>&& move) noexcept
{
	swap(move);
	return *this;
}

template<typename T, typename P>
inline T* connection<T, P>::operator->() const noexcept
{
	return value;
}

template<typename T, typename P>
inline T& connection<T, P>::operator*() const noexcept
{
	return *value;
}

template<typename T, typename P>
inline connection<T, P>::operator bool() const noexcept
{
	return value && pool;
}

template<typename T, typename P>
inline int connection<T, P>::emplace()
{
	if (value != nullptr && pool != nullptr) {
		pool->emplace(value);
		value = nullptr;
		pool = nullptr;
	}
	return 0;
}

template<typename T, typename P>
inline void connection<T, P>::swap(connection<T, P>& p) noexcept
{
	using std::swap;
	swap(value, p.value);
	swap(pool, p.pool);
}

#endif
