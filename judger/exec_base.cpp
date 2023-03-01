#include "exec_base.h"

judger::exec_base::exec_base(json& js) :scmp_flag(false)
{
	memset(argv, 0, sizeof(argv));
	ForEachField(*this, json_field (&js));
}

judger::exec_base::exec_base(cmdline::parser& cmd) :scmp_flag(false)
{
	memset(argv, 0, sizeof(argv));
	ForEachField(*this, cmdline_field(&cmd));
}

judger::exec_base::~exec_base()
{
	memset(argv, 0, sizeof(argv));
}

const char* judger::exec_base::cg_last_str_err()
{
	return cgroup_strerror(cgroup_get_last_errno());
}

int judger::exec_base::cg_init()
{
	if (cgroup_init()) {
		LOGGER_ERR("init cgroup err: {}", cg_last_str_err());
		exit(-1);
	}
	return 0;
}

judger::exec_base::exec_base()
{
}

int judger::exec_base::cal_time(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec - start.tv_nsec) < 0)
	{
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1e9 + end.tv_nsec - start.tv_nsec;
	}
	else
	{
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp.tv_sec * 1e3 + temp.tv_nsec / 1e6;
}

double judger::exec_base::get_mem_usage(rusage& usage)
{
	return usage.ru_maxrss / (double)1024;
}

int judger::exec_base::attach_cgroup(pid_t pid)
{
	//cgroup限制内存,init 位于主线程
	cgroup* cg = cgroup_new_cgroup(cg_name.c_str());
	if (cg == NULL) {
		WARN("can't create cgroup, {}", cgroup_strerror(cgroup_get_last_errno()));
		return errors::LOAD_CGROUP_ERROR;
	}
	if (cgroup_get_cgroup(cg)) {
		WARN("cg name: {}, can't get cgroup, {}", cg_name, cgroup_strerror(cgroup_get_last_errno()));
		return errors::LOAD_CGROUP_ERROR;
	}
	if (cgroup_attach_task_pid(cg, pid)) {
		WARN("cg name: {}, can't attach cgroup, {}", cg_name, cgroup_strerror(cgroup_get_last_errno()));
		return errors::LOAD_CGROUP_ERROR;
	}
	cgroup_free(&cg);
	return 0;
}

judger::exec_base::errors judger::exec_base::limits()
{
#ifdef CG_OFF
	//rlimt限制内存
	if (jl.mem_limit != UNLIMITED) {
		rlim_t mlim_s, mlim_h;
		mlim_s = jl.mem_limit * MB, mlim_h = (jl.mem_limit + 10) * MB;
		rlimit mem_limit = { mlim_s,mlim_h };
		if (setrlimit(RLIMIT_AS, &mem_limit)) {
			fprintf(stderr, "set memlimit failed!\n");
			exit(-1);
		}
	}
#endif // CG_OFF
	//最大输出限制
	// 输出文件大小限制
	rlimit f_size{ 15 * MB,20 * MB };

	if (setrlimit(RLIMIT_FSIZE, &f_size)) {
		return LOAD_RLIMIT_ERROR;
	}
	// cpu使用时间限制
	f_size.rlim_cur = time_limit, f_size.rlim_max = time_limit + 200;
	if (setrlimit(RLIMIT_CPU, &f_size)) {
		return LOAD_RLIMIT_ERROR;
	}
#ifndef DEBUG_PERMISSION

	if (set_permission()) {
		return SET_PERMISSION_FAILED;
	}
#endif // !DEBUG_PERMISSION
	return NONE;
}

int judger::exec_base::set_permission()
{
	return setgid(JUDGE_USER) | setuid(JUDGE_USER);
}

int judger::exec_base::redirect_fstream(const string& src, FILE* des, int mode, int flags)
{
	int fd = open(src.c_str(), mode, flags);
	if (fd < 0) {
		WARN("open file failed! {}, path {}", strerror(errno), src);
		exit(OPEN_FAILED);
	}
	if (mode != O_RDONLY && strcmp(src.c_str(), "/dev/null")) {
		truncate_file(fd);
	}

	if (dup2(fd, fileno(des)) == -1) {
		WARN("redirect failed! errno {}", strerror(errno));
		close(fd);
		exit(DUP_FAILED);
	}
	close(fd);
	return 0;
}

int judger::exec_base::truncate_file(int fd) {
	if (ftruncate(fd, 0)) {
		INFO("ftruncate failed: {}", strerror(errno));
		exit(TRUNCATE_FAILED);
	}
	lseek(fd, 0, SEEK_SET);
	return 0;
}

int judger::exec_base::timeout_killer(pid_t pid, int time_limit)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	usleep((time_limit + 200) * MILLSECOND);
	//进程还存在
	if (!kill(pid, 0)) {
		kill(pid, SIGKILL);
	}
	pthread_exit(nullptr);
	return 0;
}

judger::exec_result judger::exec_base::get_child_state(int child_state, int proc_time_use, const rusage & usage)
{
	exec_result result;
	state res_code = COMPLETE;
	int _term_sig;
	if (WIFEXITED(child_state)) {
		result._state = WEXITSTATUS(child_state);
		//略超时
		if (NO_LIMIT != time_limit && proc_time_use > time_limit) {
			INFO("TLE uid:{0:d}", uid);
			res_code = state::TLE;
		}
		//略超内存
		else if (NO_LIMIT != mem_limit && usage.ru_maxrss / 1024 > mem_limit) {
			INFO("MLE uid:{0:d}", uid);
			res_code = state::MLE;
		}
		else {
			if (WEXITSTATUS(child_state) != 0)
				TRACE("user code return val is not 0");
			res_code = state::COMPLETE;
			proc_time_use = (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * MILLSECOND + (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / MILLSECOND;
		}
	}
	else if (WIFSIGNALED(child_state)) {
		_term_sig = WTERMSIG(child_state);
		switch (WTERMSIG(child_state)) {
		case SIGILL:// 非法指令
		case SIGBUS:// 总线错误
		case SIGSEGV:// 非法内存访问
		case SIGFPE:// 算术运算错误
		case SIGXFSZ:// 输出文件超限制
			res_code = state::RE;
			INFO("child proc killed by SIG:{}", _term_sig);
			break;
		case SIGKILL:
			if (proc_time_use > time_limit) {
				INFO("TLE time use:{}", proc_time_use);
				res_code = state::TLE;
			}
			else if (usage.ru_maxrss / MB > mem_limit) {
				INFO("MLE time use:{}", usage.ru_maxrss / MB);
				res_code = state::MLE;
			}
			else {
				INFO("child proc killed by SIGKILL");
				res_code = state::RE;
			}
			break;
		case SIGSYS:
			//被屏蔽的系统调用
			res_code = state::RE;
			INFO("bad syscalls uid:{0:d}", uid);
			break;
		case SIGHUP: // TODO 低概率会有此信号，目前先忽略，待查明原因
			LOGGER_ERR("child process term, signal: sighup ignored pid:{}", pid);
			res_code = state::COMPLETE;
			break;
		default:
			INFO("UKE, pid:{}, SIG:{}", pid, _term_sig);
			res_code = state::UKE;
		}
	}
	else {
		res_code = FAIL;
		LOGGER_ERR("exec user code UKE");
	}
	result.term_sig = _term_sig;
	result.time_usage = proc_time_use;
	result.memory_usage = usage.ru_maxrss / (double)1024;
	result.child_state = res_code;
	return result;
}

void judger::exec_result::clean() {
	time_usage = 0;
	memory_usage = 0;
	task_num = 0;
	details.clear();
	_state = -1;
	child_state = 2;
	term_sig = -1;
}

judger::exec_result::exec_result(int _time_use, int _mem_use, int _term_sig, int _state, const string& _details, int _task_num) :
	time_usage(_time_use), memory_usage(_mem_use), task_num(_task_num),
	details(_details), child_state(_state), _state(-1), term_sig(_term_sig)
{
}

judger::exec_result::~exec_result()
{
}

judger::json_field::json_field(json* _js) :js(_js)
{
}

judger::json_field::~json_field()
{
	js = nullptr;
}

judger::cmdline_field::cmdline_field(cmdline::parser* _cmd) :cmd(_cmd)
{
}

judger::cmdline_field::~cmdline_field()
{
	cmd = nullptr;
}

