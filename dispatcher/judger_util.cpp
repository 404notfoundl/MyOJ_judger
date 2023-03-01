#include "judger_util.h"

int file_util::write_data(const string& des, const string& data) {
	umask(0);
	int fd = open(des.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd < 0) {
		LOGGER_ERR("open file error {}", strerror(errno));
		return dispatcher_errors::OpenFailed;
	}
	if (write(fd, data.c_str(), data.length()) < 0) {
		LOGGER_ERR("write file error {}", strerror(errno));
		return dispatcher_errors::WriteFailed;
	}
	close(fd);
	return 0;
}

string file_util::read_str(const char* src, int max_length) {
	int fd = open(src, O_RDONLY);
	if (fd < 0) {
		LOGGER_ERR("open file error {}", strerror(errno));
		return "";
	}
	char* buffer = new char[max_length + 10];
	int flen = read(fd, buffer, max_length);
	string rtn(buffer, flen);
	delete[] buffer;
	close(fd);
	return rtn;
}