#pragma once
#ifndef DISPATCHER_ERRORS
#define DISPATCHER_ERRORS
namespace dispatcher_errors {
	enum Code
	{
		None,

		ConnectFailed, // ����ʧ��
		DeclareError, // �������У�����������
		BasicConsumeError, // basic_consume ����
		ConnectionException, // ���ӷ�������
		HeartBeatTimeOut, // ������ʱ
		ChannelException, // �ŵ���������
		MsgRouteFailed, // ��Ϣ·��ʧ��
		ServerException, // rabbitmq����������

		SetAffinityError, // �����߳����ȼ�����
		TaskOutOfRangeError, // taskָ��Ƿ���ַ
		EnvelopeNotFoundError, // ������δ��ӳ���е�envelope
		InitCgroupFailed, // cgroup��ʼ��ʧ��

		CompileTaskException, // �����������
		RunTaskException, // ���������������
		SerializeTaskException, // ���л�/�����л���������

		TaskNumUnexpect, // ���Ե�����С��1
		PathNotExist, // ·��������

		MkDirFailed, // mkdir����ʧ��
		OpenFailed, // open����ʧ��/�޷����ļ�
		WriteFailed, // write����ʧ��
		ChownFailed, // chown����ʧ��

		UnexpectError // �������
	};
}
#endif // !DISPATCHER_ERRORS
