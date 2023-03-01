#include "epoll_helper.h"

epoll_helper::epoll_helper(int _max_fd_num) :
	epoll_fd(-1), sig_fd(-1), max_fd_num(_max_fd_num), close_flag(0),
	wait_flag(0), events(nullptr), fd_map()
{
	// epoll
	epoll_fd = epoll_create1(EPOLL_CLOEXEC);
	if (epoll_fd < 0) {
	}
	// signal
	sigset_t sig_mask;
	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, SIGINT);
	sigaddset(&sig_mask, SIGUSR1);// TODO 计划通知任务已完成
	sigprocmask(SIG_BLOCK, &sig_mask, nullptr);
	sig_fd = signalfd(-1, &sig_mask, SFD_NONBLOCK | SFD_CLOEXEC);
	if (sig_fd < 0) {
		close(epoll_fd);
	}
	epoll_event ev;
	ev.data.fd = sig_fd;
	ev.events = EPOLLIN | EPOLLET;
	ctl(EPOLL_CTL_ADD, sig_fd, &ev);
	// epoll_events
	events = new epoll_event[max_fd_num];
	memset(events, 0, sizeof(epoll_event) * max_fd_num);
}

epoll_helper::~epoll_helper()
{
	free();
}

int epoll_helper::add(int fd, epoll_event* ev, epoll_base* instance)
{
	fd_map.insert({ fd,instance });
	return ctl(EPOLL_CTL_ADD, fd, ev);
}

int epoll_helper::add(int signal, epoll_base* instance)
{
	sigset_t sig_mask, old_set;
	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, signal);
	sigprocmask(SIG_BLOCK, &sig_mask, &old_set);
	sigaddset(&old_set, signal);
	signalfd(sig_fd, &old_set, SFD_NONBLOCK);
	sig_map.insert({ signal,instance });
	return 0;
}

int epoll_helper::del(int fd)
{
	return ctl(EPOLL_CTL_DEL, fd, nullptr);
}

int epoll_helper::ctl(int op, int fd, epoll_event* ev)
{
	return epoll_ctl(epoll_fd, op, fd, ev);
}

int epoll_helper::loop()
{
	while (!close_flag) {
		int n = epoll_wait(epoll_fd, events, max_fd_num, -1);
		if (n < 0) {
			if (EINTR != errno) {
				LOGGER_ERR("epoll error: {}", strerror(errno));
				return -1;
			}
			else {
				continue;
			}
		}
		for (int i = 0; i < n; i++) {
			if (events[i].events & (/*EPOLLRDHUP |*/ EPOLLERR | EPOLLHUP)) {
				//auto code = events[i].events;
				//LOGGER_ERR("events error {}", code);
			}
			else {
				int fd = events[i].data.fd;
				if (fd == sig_fd && (events[i].events & EPOLLIN)) {
					handle_sig();
				}
				else {
					if (fd_map.find(fd) != fd_map.end()) {
						auto instance = fd_map.find(fd)->second;
						instance->run_io(fd);
					}
					else {
					}
				}
			}
		}
	}
	return 0;
}

void epoll_helper::handle_sig()
{
	signalfd_siginfo info;

	if (read(sig_fd, &info, sizeof(signalfd_siginfo)) < 0)
	{
		LOGGER_ERR("read sig fd failed: {}", strerror(errno));
		return;
	}
	switch (info.ssi_signo)
	{
	case SIGINT:
	{
		INFO("get SIGINT, exiting");
		if (wait_flag) // 多次发送此信号则强退
			close_flag = 1;
		wait_flag = 1;
		// TODO 通知结束
		for (auto begin = fd_map.begin(); begin != fd_map.end(); begin++) {
			begin->second->before_close(begin->first);
		}
		for (auto begin = sig_map.begin(); begin != sig_map.end(); begin++) {
			begin->second->before_close(begin->first);
		}
		break;
	}
	case SIGUSR1: // 表示任务已完成 TODO
	{
		if (wait_flag)
			close_flag = 1;
		break;
	}
	default:
	{
		if (sig_map.find(info.ssi_signo) == sig_map.end()) {
			WARN("unkonw sig: {}", info.ssi_signo);
		}
		else {
			auto instance = sig_map.find(info.ssi_signo)->second;
			instance->run_signal(info.ssi_signo);
		}
		break;
	}
	}
}

int epoll_helper::sig_set_ctl(sigset_t& sig_mask, int ctl, int signal)
{
	sigset_t old_set;
	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, signal);
	sigprocmask(ctl, &sig_mask, &old_set);
	sigaddset(&old_set, signal);
	std::swap(sig_mask, old_set);
	return 0;
}

void epoll_helper::free()
{
	if (epoll_fd == -1 || events == nullptr || sig_fd == -1) {
		return;
	}
	auto begin = fd_map.begin();
	for (; begin != fd_map.end(); begin++) {
		if (del(begin->first)) {
		}
	}
	close(epoll_fd);
	epoll_fd = -1;
	close(sig_fd);
	sig_fd = -1;
	delete[] events;
	events = nullptr;
	fd_map.clear();
	sig_map.clear();
}

int epoll_base::before_close(int fd)
{
	return 0;
}