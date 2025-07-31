#pragma once
#include <chrono>

#ifndef ArraySize(a)
#define ArraySize(a) (sizeof(a) / sizeof(*a))
#endif // !ArraySize(a)


void FatalError(const char* msg, int exitCode = 1);

void dbgprint(const wchar_t* str, ...);

void dbgprint(const char* str, ...);

// печатает последнюю ошибку, связанную с системой (полученную при помощи GetLastError())
void syserrprint(const char* msg);

struct Timer {
	std::chrono::steady_clock::time_point startTime, stopTime;
	void start();
	void stop();
	void printMS(const char* msg = NULL);
	void printS(const char* msg = NULL);
};