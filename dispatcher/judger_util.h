#pragma once
#ifndef JUDGER_UTIL
#define JUDGER_UTIL
#include <iostream>
#include <cstdio>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include "logger.h"
#include "dispatcher_errors.h"
using std::string;
namespace file_util {
	int write_data(const string& des, const string& data);

	string read_str(const char* src, int max_length);
}
#endif // !UTIL
