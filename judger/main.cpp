#include <cstdio>

#ifdef SO
#include "compiler.h"
#include "runner.h"
#endif // SO
#ifdef COMPILE_CMD
#ifdef RUN_CMD
#error "only can define either COMPILE_CMD or RUN_CMD"
#endif // RUN_CMD
#include "compiler.h"
#endif // COMPILER_CMD
#ifdef RUN_CMD
#ifdef COMPILE_CMD
#error "only can define either COMPILE_CMD or RUN_CMD"
#endif // COMPILE_CMD
#include "runner.h"
#endif // RUN_CMD
// 所使用的参数定义
namespace judger {
	namespace compiler {
		const string args::options = R"(
            {
              "compile_argvs": [
                {
                  "name": "cpp11",
                  "argvs": [
                    { "cpp11": "-std=c++11" }
                  ]
                },
                {
                  "@name": "c/cpp common flags",
                  "name": "c/cpp",
                  "argvs": [
                    { "max-error": "-fmax-errors=5" },
                    { "OJ": "-DONLINE_JUDGE" },
                    { "wall": "-Wall" },
                    { "fno-asm": "-fno-asm" },
                    { "march": "-march=native" },
                    { "lm": "-lm" },
                    { "O2": true },
                    { "NULL": 0 }
                  ]
                }
              ]
            }        
        )";
		unordered_map<string, string> args::compiler_path = {
			{"c","/usr/bin/gcc"},
			{"cpp11","/usr/bin/g++"}
		};
		const unordered_map<string, unordered_set<string>> args::compiler_args = {
			{"c",{"c/cpp"}},
			{"cpp11",{"c/cpp","cpp11"}},
		};
        json args::compile_options;
	}
    namespace runner {
		string args::env_var = "HOSTNAME=a0d583140b72;TERM=xterm;PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin;_=/usr/bin/env;";
    }
    namespace checker {
        const string args::spj_root="/home/spj_checker/";
        const string args::spj_wcmp="wcmp";
        const string args::spj_ncmp = "ncmp";
    }
}
#ifndef SO
#ifdef COMPILE_CMD
int compile(int argc, char* argv[]) {
    judger::exec_base::cg_init();
    cmdline::parser cmd;
    judger::compiler::add_cmd(cmd, argc, argv);
    auto rtn = judger::compiler::start(cmd);
    int result = 1;
    switch (rtn)
    {
    case judger::exec_base::AC:
        INFO("compile success");
        result = 0;
        break;
    case judger::exec_base::FAIL:
        INFO("compile error");
        break;
    case judger::exec_base::ERROR:
        LOGGER_ERR("compile error");
        result = 2;
        break;
    default:
        break;
    }
    return result;
}
#endif // COMPILER_CMD

#ifdef RUN_CMD
int run(int argc, char* argv[]) {
    cmdline::parser cmd;
    judger::exec_base::cg_init();
    judger::runner::set_cmdline(cmd, argc, argv);
    auto rtn = judger::runner::start(cmd);
    return rtn;
}
#endif // RUN_CMD

int main(int argc, char* argv[])
{
#ifdef COMPILE_CMD
	return compile(argc, argv);
#endif // COMPILER_CMD
    
#ifdef RUN_CMD
    return run(argc, argv);
#endif // RUN_CMD
    return 0;
}
/* compile
* -c compilecgroup 
* -e /home/test_case/compiler/1.err 
* -l c 
* -u /home/test_case/compiler/compile_success.c 
* -o /home/test_case/compiler/test_compile 
* -O 1
*/
/* compile -c compilecgroup -e /home/test_case/compiler/1.err -l c -u /home/test_case/compiler/compile_success.c -o /home/test_case/compiler/test_compile -O 1
*/

/* run
* -a /home/test_case/runner/1.ans
* -c defaultcgroup
* -e /home/test_case/runner/1.err
* -l cpp11
* -i 1026
* -m 128
* -o /home/test_case/runner/sleep_500ms
* -p /home/test_case/runner/{}
* -t 1000
* -u 1
* -M 0
* -P /home/test_case/runner/
* -T 1
* -S 0
* -U 15b0685f868a46d48296ff4c31a60b9d
*/
/*
 -a /home/test_case/runner/1.ans -c defaultcgroup -e /home/test_case/runner/1.err -l cpp11 -i 1026 -m 128 -o /home/test_case/runner/sleep_500ms -p /home/test_case/runner/{} -t 1000 -u 1 -M 0 -P /home/test_case/runner/ -T 1 -S 0 -U 15b0685f868a46d48296ff4c31a60b9d
*/
#endif // SO
