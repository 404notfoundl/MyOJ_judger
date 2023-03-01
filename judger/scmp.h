#pragma once
#ifndef SCMP
#define SCMP
#include <iostream>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <seccomp.h>
#include <fcntl.h>
#include <thread>
#include <stddef.h>
#include <unistd.h>
#include <functional>
#include <unordered_map>
using std::unordered_map;
using std::string;
namespace judger {
	namespace runner {
		constexpr const int LOAD_SECCOMP_FAILED = -1;
		using scmp_func = std::function<int(scmp_filter_ctx, const char*, bool)>;
		class scmp_fliter {
		public:
			/**
			 * @brief 见相应文件,对应的语言的映射
			*/
			static unordered_map<string, scmp_func> fliter_map;
			/**
			 * @brief c或c++的过滤器
			 * @param ctx scmp结构体
			 * @param cmd 允许执行的路径
			 * @param rwflag 是否允许读写
			 * @return
			*/
			static int c_cpp(scmp_filter_ctx ctx, const char* cmd, bool rwflag);

		};
	}
}
#endif // !SCMP
