#pragma once
#ifndef EPOLL_HELPER
#define EPOLL_HELPER
#include <sys/epoll.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <unordered_map>
#include <cstring>
#include <signal.h>
#include <sys/signalfd.h>
#include "logger.h"
using std::unordered_map;
class epoll_base
{
public:
	virtual int run_io(int fd) = 0;
	virtual int run_signal(int signal) = 0;
	// TODO 退出前通知各个fd只出不进
	virtual int before_close(int fd);
};

class epoll_helper
{
public:
	epoll_helper(int _max_fd_num);
	~epoll_helper();
	/**
	 * @brief io fd 管理
	 * @param fd
	 * @param ev
	 * @param instance
	 * @return
	*/
	int add(int fd, epoll_event* ev, epoll_base* instance);
	/**
	 * @brief signal fd 管理
	 * @param signal 待注册sig
	 * @param instance
	 * @return
	*/
	int add(int signal, epoll_base* instance);
	int del(int fd);
	int ctl(int op, int fd, epoll_event* ev);

	int loop();

	void free();

private:
	void handle_sig();
	int sig_set_ctl(sigset_t& sig_mask, int ctl, int signal);
	int epoll_fd;
	int sig_fd;
	int max_fd_num;
	int close_flag, wait_flag;
	epoll_event* events;
	unordered_map<int, epoll_base*> fd_map;
	unordered_map<int, epoll_base*> sig_map;
};

#endif // !EPOLL_HELPER
