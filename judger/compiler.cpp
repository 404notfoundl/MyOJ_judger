#include "compiler.h"

judger::compiler::compiler::compiler(json& js) :exec_base(js), arg_str("")
{
	arg_str.reserve(1 << 10);
	scmp_flag = false;
	if (args::compiler_path.find(lang) != args::compiler_path.end()) {
		argv[0] = &args::compiler_path.at(lang)[0]; // 编译器路径
		auto index = args::compiler_path.at(lang).length();
		for (; index > 0; index--) {
			if (argv[0][index - 1] == '/')break;
		}
		argv[1] = &argv[0][index]; // 编译器名称
	}
	else {
		throw std::runtime_error("compiler not support this language: " + lang);
	}
	time_limit = 1e4; // 编译10s时间
	mem_limit = NO_LIMIT; 
	argv_cnt = 2;
}

judger::compiler::compiler::compiler(cmdline::parser& cmd) :exec_base(cmd)
{
	arg_str.reserve(1 << 10);
	scmp_flag = false;
	if (args::compiler_path.find(lang) != args::compiler_path.end()) {
		argv[0] = &args::compiler_path.at(lang)[0]; // 编译器路径
		auto index = args::compiler_path.at(lang).length();
		for (; index > 0; index--) {
			if (argv[0][index - 1] == '/')break;
		}
		argv[1] = &argv[0][index]; // 编译器名称
	}
	else {
		throw std::runtime_error("compiler not support this language: " + lang);
	}
	time_limit = 1e4; // 编译10s时间
	mem_limit = NO_LIMIT;
	argv_cnt = 2;
}

judger::compiler::compiler::~compiler()
{
}

judger::exec_base::state judger::compiler::compiler::start()
{
	//TODO: 统计内存使用较小的情况下准确的记录
#ifdef SYNC_CG_ON
		// 确保cg先附加上去
	int shmid = shmget(IPC_PRIVATE, sizeof(sem), IPC_CREAT | 0666);
	if (shmid == -1) {
		LOGGER_ERR("open shm failed, {}", strerror(errno));
		exit(errors::OPEN_SHM_FAILED);
	}
	sem* sync_flag = (sem*)shmat(shmid, NULL, 0);
	sync_flag->init(1, 0);
#endif // SYNC_CG_ON
	pid_t pid = fork();
	if (pid < 0) {
		LOGGER_ERR("fork err: {}", strerror(errno));
		exit(errors::FORK_FAILED);
	}
	else if (pid == 0) {
#ifndef PERMISSION_OFF
		if (set_permission()) {
			perror("set uid failed");
			exit(errno);
		}
#endif // !PERMISSION_OFF
#ifdef SYNC_CG_ON
		sync_flag->wait();
		shmdt(sync_flag);
#endif // SYNC_CG_ON
		int old_mask = umask(0);
		redirect_fstream(err_path, stderr, O_WRONLY | O_TRUNC | O_CREAT, 0666);
		umask(old_mask);
		execv(argv[0], &argv[1]);
		INFO("execv error {}", strerror(errno));
		exit(errno);
	}
	else {
#ifndef CG_OFF
		if (attach_cgroup(pid)) {
			LOGGER_ERR("attach cgroup failed, errno: {} error: {}", cgroup_get_last_errno(), cgroup_strerror(cgroup_get_last_errno()));
			exit(errors::LOAD_CGROUP_ERROR);
		}
#endif // !CG_OFF
#ifdef SYNC_CG_ON
		sync_flag->post();
		shmdt(sync_flag);
#endif // SYNC_CG_ON
		TRACE("wait for child");
		rusage usage;
		int child_state = 0;
		timespec start_time, end_time, cpu_stime, cpu_etime;
		//监控用户进程
		std::thread killer(timeout_killer, pid, time_limit);
		killer.native_handle();
		killer.detach();
		
		clock_gettime(CLOCK_MONOTONIC_RAW, &start_time); //计时-实际用时
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_stime); // cpu
#ifndef WAIT4_ON
		waitpid(pid, &child_state, 0);
#else
		int rtn_wait = wait4(pid, &child_state, 0, &usage);
#endif // !WAIT4_FLAG
		clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_etime);
		clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

#ifdef SYNC_CG_ON
		shmctl(shmid, IPC_RMID, 0);
#endif // SYNC_CG_ON

		TRACE("get child proc");
#ifndef WAIT4_ON
		//用户进程用时
		if (getrusage(RUSAGE_CHILDREN, &usage)) {
			result = judge_status::UKE;
			//ERROR(logger, "uid:{0:d},pid:{1:d}/tget RusageError", info.uid, info.pid);
			WARN("uid:{0:d},pid:{1:d}/tget RusageError", info.uid, info.pid);
		}
#else
		if (rtn_wait <= 0 && errno != ECHILD) {
			LOGGER_ERR("wait4 error : {}", strerror(errno));
		}
#endif
		else {
			state result = state::FAIL;
			auto child_res = get_child_state(child_state, cal_time(start_time, end_time), { 0 });
			switch (child_res.child_state) {
			case COMPLETE:
				if (child_res._state != 0) {
					result = state::FAIL;
					break;
				}
				result = state::AC;
				break;
			default:
				if (TLE == child_res.child_state) {
					file_util::write_data(err_path, "compile timeout!");
				}
				result = state::FAIL;
				break;
			}

			TRACE("end compile, result {0:d}", result);
			return result;
		}
	}
	return state::AC;
}

int judger::compiler::compiler::merge_argv(json& cfg)
{
	
	for (auto args = cfg.at("compile_argvs").begin(); args != cfg.at("compile_argvs").end(); args++) {
		auto des = args::compiler_args.at(lang);
		if (des.find(args->at("name")) != des.end()) {
			add_arg(args->at("argvs"));			
		}
	}
	int prev = 0;
	for (size_t i = 0; i < arg_str.length(); i++) {
		if (arg_str[i] == ';') {
			argv[argv_cnt++] = &arg_str[prev];
			arg_str[i] = '\0';
			prev = i + 1;
		}
	}
	argv[argv_cnt++] = &src_path[0];
	argv[argv_cnt++] = "-o"; // RDONLY
	argv[argv_cnt++] = &des_path[0];
	argv[argv_cnt++] = nullptr;
	return 0;
}

int judger::compiler::compiler::add_arg(json& cfg)
{
	if (!(cfg.is_object() || cfg.is_array())) {
		if (cfg.is_string()) {
			arg_str.append(cfg.get<string>()).append(";");
		}
		else if (cfg.is_boolean()) {
			// TODO 识别O2
			bool O2 = cfg.get<bool>();
			if (O2) {
				argv[argv_cnt++] = "-O2"; // RDONLY
			}
		}
		return 0;
	}
	for (auto item : cfg) {
		add_arg(item);
	}
	return 0;
}

judger::exec_base::state judger::compiler::start(json& js)
{
	if (args::compile_options.is_null()) {
		args::compile_options = json::parse(args::options);
	}
	compiler instance(js);
	instance.merge_argv(args::compile_options);
	auto rtn = instance.start();
	return rtn;
}

judger::exec_base::state judger::compiler::start(cmdline::parser& cmd)
{
	if (args::compile_options.is_null()) {
		args::compile_options = json::parse(args::options);
	}
	compiler instance(cmd);
	instance.merge_argv(args::compile_options);
	auto rtn = instance.start();
	return rtn;
}

int judger::compiler::add_cmd(cmdline::parser& cmd, int argc, char* argv[])
{
	cmd.add<string>("cg_name", 'c', "control group name", false, "compilecgroup");
	cmd.add<string>("err_path", 'e', "error path", true, "");
	//此处添加接受的语言类型
	cmd.add<string>("lang", 'l', "language", true, "", cmdline::oneof<string>("c", "cpp11"));
	cmd.add<int>("mem_limit", 'm', "memory limit", false, 128);
	cmd.add<string>("des_path", 'o', "program file path", true, "");
	cmd.add<int>("time_limit", 't', "time limit", false, 1000);
	cmd.add<string>("src_path", 'u', "usercode path", true, "");
	cmd.add<int>("uid", 'U', "uid", false, 0);
	cmd.add<bool>("O2", 'O', "O2", false, false);
	cmd.add<int>("pid", 'P', "pid", false, 0);
	cmd.set_program_name("./compiler");
	cmd.footer("compile user code");
	cmd.parse_check(argc, argv);
	return 0;
}
