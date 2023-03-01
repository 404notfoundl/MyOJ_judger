#include <fstream>

#include "runner.h"

judger::runner::runner::runner(json& js) :exec_base(js)
{
    memset(argv, 0, sizeof(argv));
    scmp = nullptr;
    scmp_flag = true;
    //json_field temp(&js);
    //ForEachField(*this, set_field<json_field>(temp));
	ForEachField(*this, json_field(&js));
	usr_result = {
		{"uid",uid},{"pid",pid},
		{"uuid",uuid},{"status",""}
	};
    set_env_path();
}

judger::runner::runner::runner(cmdline::parser& cmd) :exec_base(cmd)
{
    memset(argv, 0, sizeof(argv));
    scmp = nullptr;
    scmp_flag = true;
    //cmdline_field temp(&cmd);
    //ForEachField(*this, set_field<cmdline_field>(temp));
    ForEachField(*this, cmdline_field(&cmd));
    usr_result = {
        {"uid",uid},{"pid",pid},
        {"uuid",uuid},{"status",""}
    };
    set_env_path();
}

judger::runner::runner::~runner()
{
    memset(argv, 0, sizeof(argv));
    scmp = nullptr;
}

judger::exec_base::state judger::runner::runner::start()
{
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
        result.child_state = UKE;
        LOGGER_ERR("fork failed, pid: {}, error: {}", pid, strerror(errno));
        exit(errors::FORK_FAILED);
    }
    else if (pid == 0) {
        TRACE("child:start child process");
		int resno = limits();
        switch (resno)
        {
        case errors::LOAD_CGROUP_ERROR:
            WARN("can't open cgroup!");
            break;
        case errors::LOAD_RLIMIT_ERROR:
            WARN("set fsizelimit failed!");
            break;
        case errors::SET_PERMISSION_FAILED:
            WARN("set uid failed!");
            break;
        default:
            break;
        }
        if (resno) {
            exit(-1);
        }
		redirect_fstream(prob_path, stdin, O_RDONLY, 0);
        redirect_fstream(des_path, stdout, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        redirect_fstream(err_path, stderr, O_WRONLY | O_TRUNC | O_CREAT, 0666);

#ifdef SYNC_CG_ON
        sync_flag->wait();
        shmdt(sync_flag);
#endif // SYNC_CG_ON
#ifndef SCMP_OFF
        //---添加新语言勿忘添加此处以免过滤未起作用,此段放在最后，避免其他调用的影响-------
        auto scmp_f = scmp_fliter::fliter_map.find(lang)->second;
        if (scmp_f(scmp, src_path.c_str(), false)) {
            exit(errors::LOAD_SCMP_ERROR);
        }
#endif // !SCMP_OFF 检测内存泄漏时需要关掉
        execle(&src_path[0], &src_path[0], NULL, argv);
		INFO("execv error: {}", strerror(errno));
        exit(errno);
    }
    else {
        exec_base::state res_code = exec_base::COMPLETE;
        rusage usage;

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
        int child_state = 1;
        timespec start_time, end_time, cpu_stime, cpu_etime;
        //监控用户进程
        std::thread killer(timeout_killer, pid, time_limit);
        killer.native_handle();
        killer.detach();
        //计时-实际用时
        clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_stime);
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

#ifndef WAIT4_ON
        //用户进程用时
        if (getrusage(RUSAGE_CHILDREN, &usage)) {
            result = state::UKE;
            //ERROR(logger, "uid:{0:d},pid:{1:d}/tget RusageError", info.uid, info.pid);
            WARN("uid:{0:d},pid:{1:d}/tget RusageError", uid, pid);
        }
#else
        if (rtn_wait <= 0 && errno != ECHILD) {
            res_code = state::UKE;
            LOGGER_ERR("wait4 error : {}", strerror(errno));
        }
#endif
        else {
            result = get_child_state(child_state, cal_time(start_time, end_time), usage);
        }
		if (COMPLETE != res_code) {
            result.child_state = res_code;
		}
    }
    return state(result.child_state);
}

judger::exec_base::errors judger::runner::start(cmdline::parser& cmd)
{
	runner instance(cmd);
    instance.scmp_init();
	auto rtn_code = _start(instance);
    seccomp_release(instance.scmp);
    instance.scmp = nullptr;
	std::ofstream output(instance.err_path);
	output << instance.usr_result;
	output.close();
	return rtn_code;
}

judger::exec_base::errors judger::runner::start(json & js, scmp_filter_ctx ctx)
{
	runner instance(js);
	instance.scmp = ctx;
    //INFO(js.dump());
	auto rtn_code = _start(instance);
	js = instance.usr_result;
	return rtn_code;
}

judger::exec_base::errors judger::runner::_start(runner& instance)
{
    TRACE("start runner");
    exec_base::errors resno = exec_base::NONE;
    result_group rg;
    int tasks_num = instance.task_number, start = instance.task_start;

    json& usr_result = instance.usr_result;
    std::string _prob_path(instance.prob_path);
    if (tasks_num <= 0) {
        LOGGER_ERR("test's count <= 0, path: {}", instance.prob_path);
        return judger::exec_base::errors::TESTCASE_COUNT_ERROR;
    }

    for (int a = start; a < start + tasks_num; a++) {
        instance.result.clean();
		instance.prob_path = fmt::format(_prob_path, a);
        if (access(instance.prob_path.c_str(), F_OK) != 0) {
#ifndef ROTATE_LOGGER_OFF
            LOGGER_ERR("readin file not existed! path: {}", instance.prob_path);
#endif // !ROTATE_LOGGER_OFF
            resno = runner::errors::FILE_NOT_EXISTED;
            break;
        }

        TRACE("run usr code,task:{0:d}", a);
        instance.result.task_num = a;
		auto rtn_code = instance.start();
        if (exec_base::state::COMPLETE != rtn_code) {
            rg.update(instance.result);
            resno = exec_base::errors::RUN_CODE_ERROR;
			if (exec_base::UKE != rtn_code) {
                TRACE("task {} : UNACCEPT", a);
            }
            else {
                TRACE("task {} : UKE", a);
            }
			if (instance.pid == 0)usr_result["output"] = rg.details.back();
        }
        else {
            TRACE("complete runner");
            rg.update("", instance.result.time_usage, instance.result.memory_usage, "");
            if (instance.pid != 0) {
                // TODO
                //runner::comparer::start(jl, rg);
                checker::start(instance, rg);
                if (rg.status.back() == 'a') {
                    TRACE("task {} : AC", a);
                }
                else {
                    resno = exec_base::errors::RUN_CODE_ERROR;
                    TRACE("task {} : WA", a);
#ifdef OJ_DEBUG
                    INFO("comparer task {} reason : {}\n\texec p: {}", a, rg.details.back(), jl.argvs[0]);
#endif
                }
            }
            // 编辑器,需要获取程序输出
            else {
                rg.details.push_back(file_util::read_str(instance.des_path, 2048));
            }
        }
    }

    if (exec_base::FILE_NOT_EXISTED != resno) {
        rg.to_json(usr_result);
    }
    if (resno == exec_base::NONE)
        INFO("uid:{:<5} pid:{:<5} AC max time use:{:<4} max mem use:{:<6.2f}", instance.uid, instance.pid, rg.max_time_usage, rg.max_memory_usage);
    else
        INFO("uid:{:<5} pid:{:<5} WA max time use:{:<4} max mem use:{:<6.2f}", instance.uid, instance.pid, rg.max_time_usage, rg.max_memory_usage);
    return resno;
}

void judger::runner::set_cmdline(cmdline::parser& cmd, int argc, char* argv[])
{
	cmd.add<string>("des_path", 'a', "user answer path", true);
	cmd.add<string>("cg_name", 'c', "cgroup name", true);
	cmd.add<string>("err_path", 'e', "error path", true);
	cmd.add<string>("lang", 'l', "code language", true);
	cmd.add<int>("pid", 'i', "pid", true);
	cmd.add<int>("mem_limit", 'm', "memory limit", true);
	cmd.add<string>("src_path", 'o', "user program path", true);
	cmd.add<string>("prob_path", 'p', "problem path", true);
	cmd.add<int>("time_limit", 't', "time limit", true);
	cmd.add<int>("uid", 'u', "uid", true);

    cmd.add<string>("env_path", 'E', "env vars", false, "");
	cmd.add<int>("method", 'M', "judge method", true);
	cmd.add<string>("prob_root", 'P', "problem pattern path", true);
	cmd.add<int>("start", 'S', "first task index", false, 0);
	cmd.add<int>("task_number", 'T', "tasks count", true);
	cmd.add<string>("uuid", 'U', "uuid", true);

	cmd.parse_check(argc, argv);
}

int judger::runner::runner::scmp_init(uint32_t action)
{
	scmp = seccomp_init(action);
    return 0;
}

void judger::runner::runner::set_env_path()
{
    if (_env_path.length() == 0) {
        _env_path = args::env_var;
    }
    int cnt = 0;
    _env_path.resize(_env_path.length() + 1, '\0');
    for (long unsigned int i = 0; i < _env_path.length(); i++) {
        if (_env_path[i] == ';') {
            _env_path[i] = '\0';
            argv[cnt++] = &_env_path[++i];
        }
    }
}

void judger::runner::runner::set_default(int& field)
{
    field = 0;
}

void judger::runner::result_group::update(const string& _status, int _time_use, double _mem_use, const string& _details)
{
    max_time_usage = std::max(max_time_usage, _time_use);
    max_memory_usage = std::max(max_memory_usage, _mem_use);
	if (_status.length() != 0)
		status.push_back(_status[0]);
	if (_details.length() != 0)
		details.push_back(_details);
}

void judger::runner::result_group::update(exec_result& res)
{
    max_time_usage = std::max(max_time_usage, res.time_usage);
    max_memory_usage = std::max(max_memory_usage, res.memory_usage);
    string _detail;
    switch (res.child_state)
    {
    case exec_base::AC:
        status.push_back('a');
        _detail = "AC";
        break;
    case exec_base::CE:
        status.push_back('c');
        _detail = "CE";
        break;
    case exec_base::MLE:
        status.push_back('m');
        _detail = fmt::format("MLE: {:.2f}", res.memory_usage);
        break;
    case exec_base::TLE:
        status.push_back('t');
        _detail = fmt::format("TLE: {}", res.time_usage);
        break;
    case exec_base::RE:
        status.push_back('r');
        _detail = fmt::format("RE SIG: {}", res.term_sig);
        break;
    case exec_base::WA:
        status.push_back('w');
        _detail = res.details;
        break;
    default:
        status.push_back('u');
		_detail = "UKE";
        break;
    }

    details.push_back(_detail);
}

void judger::runner::result_group::to_json(json& js)
{
    ForEachField(*this, [this, &js](auto&& field, auto&& name) {
        js[name] = field;
    });
}

judger::runner::result_group::result_group() :max_time_usage(0), max_memory_usage(0)
{
}

judger::runner::result_group::~result_group()
{
}

judger::exec_base::state judger::checker::start(runner::runner& run, runner::result_group& rg)
{
	checker check(run, rg);
    switch (run.checker_method)
    {
	case COMMON: //通常对比
        check.spj_path = args::spj_root + args::spj_wcmp;
        check.spj_name = args::spj_wcmp;
		break;
	case SPJ: //spj
        check.spj_path = run.prob_root + "checker";
        check.spj_name = "checker";
        break;
	default:
		break;
    }	
    return check.start();;
}

judger::checker::checker::checker(runner::runner& _run, runner::result_group& _rg) :
    run(_run), rg(_rg)
{
	time_limit = _run.time_limit * 3;
    // TODO cg对应的修改
    mem_limit = _run.mem_limit * 3;
    cg_name = _run.cg_name;
}

judger::checker::checker::~checker()
{
}

judger::exec_base::state judger::checker::checker::start()
{

#ifdef SYNC_CG_ON
    // 确保cg先附加上去
    int shmid = shmget(IPC_PRIVATE, sizeof(sem), IPC_CREAT | 0666);
    if (shmid == -1) {
        LOGGER_ERR("open shm failed {}", strerror(errno));
        exit(errors::OPEN_SHM_FAILED);
    }
    sem* sync_flag = (sem*)shmat(shmid, NULL, 0);
    sync_flag->init(1, 0);
#endif // SYNC_CG_ON			
    pid_t pid = fork();
    exec_result res;
    if (pid == 0) {
        string prob(run.prob_path); prob.append(".out");
        // 重定向标准流
		redirect_fstream("/dev/null", stdin, O_RDONLY, 0);
		redirect_fstream("/dev/null", stdout, O_WRONLY, 0);
		redirect_fstream("/dev/null", stderr, O_WRONLY, 0);
#ifdef SYNC_CG_ON
        sync_flag->wait();
        shmdt(sync_flag);
#endif // SYNC_CG_ON
        execl(spj_path.c_str(), spj_name.c_str(), &run.prob_path[0], &run.des_path[0], &prob[0], &run.err_path[0], NULL);
        perror("execv checker failed:");
    }
    else if (pid > 0) {
#ifndef CG_OFF
        // 为checker添加相应控制
        if (attach_cgroup(pid)) {
            LOGGER_ERR("at comparer attach cgroup error");
            exit(errors::LOAD_CGROUP_ERROR);
        }
#endif // !CG_OFF
#ifdef SYNC_CG_ON
        sync_flag->post();
        shmdt(sync_flag);
#endif // SYNC_CG_ON

        //监控用户进程
		std::thread killer(timeout_killer, pid, time_limit); // checker 时间更宽裕
        killer.native_handle();
        killer.detach();

        int status = 0;
        waitpid(pid, &status, 0);

#ifdef SYNC_CG_ON
        shmctl(shmid, IPC_RMID, 0);
#endif // SYNC_CG_ON
		res = get_child_state(status, -1, { 0 });
        switch (res.child_state)
        {
        case COMPLETE:
            if (res._state) {
                res.child_state = state::WA;
                res.details = file_util::read_str(run.err_path, 512);
                break;
            }
            res.child_state = AC;
            break;
        default:
            res.child_state = state::UKE;
            break;
        }
    }
    else {
        LOGGER_ERR("can't fork, errno: {}", errno);
        exit(errors::FORK_FAILED);
    }
    rg.update(res);
    return state(res.child_state);
}
