#include "scmp.h"

namespace judger {
	namespace runner {
		int scmp_fliter::c_cpp(scmp_filter_ctx ctx, const char* cmd, bool rwflag)
			{
				int syscalls_whitelist[] = {
					SCMP_SYS(read), SCMP_SYS(fstat),
					SCMP_SYS(mmap), SCMP_SYS(mprotect),
					SCMP_SYS(munmap), SCMP_SYS(uname),
					SCMP_SYS(arch_prctl), SCMP_SYS(brk),
					SCMP_SYS(access), SCMP_SYS(exit_group),
					SCMP_SYS(close), SCMP_SYS(readlink),
					SCMP_SYS(sysinfo), SCMP_SYS(write),
					SCMP_SYS(writev), SCMP_SYS(lseek),
					SCMP_SYS(clock_gettime),SCMP_SYS(pread64),
					SCMP_SYS(time),SCMP_SYS(exit_group),SCMP_SYS(stat),
					SCMP_SYS(nanosleep),SCMP_SYS(times)
				};
				int syscalls_whitelist_length = sizeof(syscalls_whitelist) / sizeof(int);
				if (ctx == NULL) {
					fprintf(stderr, "error: scmp ctx is NULL!\n");
					return LOAD_SECCOMP_FAILED;
				}
				seccomp_reset(ctx, SCMP_ACT_KILL);
				// load seccomp rules 载入
				if (!ctx) {
					return LOAD_SECCOMP_FAILED;
				}
				for (int i = 0; i < syscalls_whitelist_length; i++) {
					if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscalls_whitelist[i], 0) != 0) {
						return LOAD_SECCOMP_FAILED;
					}
				}

				//允许特定地址执行
				if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(execve), 1, SCMP_A0(SCMP_CMP_EQ, (scmp_datum_t)(cmd))) != 0) {
					return LOAD_SECCOMP_FAILED;
				}

				if (!rwflag) {
					// 禁止写
					if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 1, SCMP_CMP(1, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) != 0) {
						return LOAD_SECCOMP_FAILED;
					}
					if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 1, SCMP_CMP(2, SCMP_CMP_MASKED_EQ, O_WRONLY | O_RDWR, 0)) != 0) {
						return LOAD_SECCOMP_FAILED;
					}
				}
				else {
					int io_whitelist[] = {
						SCMP_SYS(open),SCMP_SYS(dup),
						SCMP_SYS(dup2),SCMP_SYS(dup3)
					};
					for (size_t a = 0; a < sizeof(io_whitelist) / sizeof(int); a++) {
						if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, io_whitelist[a], 0) != 0) {
							return LOAD_SECCOMP_FAILED;
						}
					}
					// checker 用
					if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(ioctl), 1, SCMP_A0(SCMP_CMP_LE, 2)) != 0) {
						return LOAD_SECCOMP_FAILED;
					}

				}
				if (seccomp_load(ctx) != 0) {
					return LOAD_SECCOMP_FAILED;
				}
				return 0;
			}

		unordered_map<string, scmp_func> scmp_fliter::fliter_map = {
			{"c",scmp_func(scmp_fliter::c_cpp)},
			{"cpp11",scmp_func(scmp_fliter::c_cpp)}
		};
	}
}