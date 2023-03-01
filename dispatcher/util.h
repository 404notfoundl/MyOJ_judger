#pragma once
#ifndef JUDGER_UTIL
#define JUDGER_UTIL
#include <iostream>
#include <cstdio>
#include <pthread.h>
using std::string;
namespace file_util {
	int write_data(string& des, string& data) {
		FILE* fp = NULL;
		fp = fopen(des.c_str(), "w");
		if (fp == NULL) {
			fprintf(stderr, "open file error\n");
			return -1;
		}
		if (fputs(data.c_str(), fp) == EOF) {
			fprintf(stderr, "write file error\n");
			return -1;
		}
		fclose(fp);
		return 0;
	}

	string read_str(const char* src, int max_length) {
		FILE* fp = NULL;
		fp = fopen(src, "r");
		if (fp == NULL) {
			fprintf(stderr, "open file error");
			return "";
		}
		char* buffer = new char[max_length + 10];
		int flen = fread(buffer, sizeof(char), max_length, fp);
		string rtn(buffer, flen);
		delete buffer;
		fclose(fp);
		return rtn;
	}
}
#endif // !UTIL
