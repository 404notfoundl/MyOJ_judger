#pragma once
#ifndef DISPATCHER_ERRORS
#define DISPATCHER_ERRORS
namespace dispatcher_errors {
	enum Code
	{
		None,

		ConnectFailed, // 连接失败
		DeclareError, // 声明队列，交换机出错
		BasicConsumeError, // basic_consume 出错
		ConnectionException, // 连接发生错误
		HeartBeatTimeOut, // 心跳超时
		ChannelException, // 信道发生错误
		MsgRouteFailed, // 消息路由失败
		ServerException, // rabbitmq服务器出错

		SetAffinityError, // 设置线程优先级出错
		TaskOutOfRangeError, // task指向非法地址
		EnvelopeNotFoundError, // 出现了未在映射中的envelope
		InitCgroupFailed, // cgroup初始化失败

		CompileTaskException, // 编译出现问题
		RunTaskException, // 运行任务出现问题
		SerializeTaskException, // 序列化/反序列化出现问题

		TaskNumUnexpect, // 测试点数量小于1
		PathNotExist, // 路径不存在

		MkDirFailed, // mkdir调用失败
		OpenFailed, // open调用失败/无法打开文件
		WriteFailed, // write调用失败
		ChownFailed, // chown调用失败

		UnexpectError // 意外错误
	};
}
#endif // !DISPATCHER_ERRORS
