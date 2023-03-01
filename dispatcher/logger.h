#pragma once
#ifndef LOGGER
#define LOGGER

#include "ThirdLib/spdlog/sinks/rotating_file_sink.h"
#include "ThirdLib/spdlog/spdlog.h"
#include <iostream>
#define LOGS_PATH "logs/err.log"
#define ERR_LOGGER_NAME "err_logger"

using std::shared_ptr;
using std::string;
class logger
{
public:
	static shared_ptr<spdlog::logger> get_rotate_logger(const string& name) {
		auto logger = spdlog::get(name);
		if (!logger) {
			try {
				logger = spdlog::rotating_logger_mt(name, LOGS_PATH, 1024 * 1024 * 5, 10);
			}
			catch (const spdlog::spdlog_ex& ex) {
				printf("get logger failed, logger: %s, err: %s", name.c_str(), ex.what());
			}
		}
		return logger;
	}

	static int init() {
		auto err = get_rotate_logger(ERR_LOGGER_NAME);
		spdlog::set_pattern("[%t] [%C-%m-%d %H:%M:%S] [%L] %v");

		err->set_pattern("[%t] [%C-%m-%d %H:%M:%S %s:%#] %v");
		err->set_level(spdlog::level::err);
		return 0;
	}

	static int drop(const string& name) {
		spdlog::drop(name);
		return 0;
	}

	static void drop_all() {
		spdlog::drop_all();
	}
};

#define TRACE(...) spdlog::trace(__VA_ARGS__)
#define INFO(...) spdlog::info(__VA_ARGS__)
#define WARN(...) spdlog::warn(__VA_ARGS__)
#define LOGGER_ERR(...) SPDLOG_LOGGER_ERROR(logger::get_rotate_logger(ERR_LOGGER_NAME), __VA_ARGS__)

#endif // !LOGGER
