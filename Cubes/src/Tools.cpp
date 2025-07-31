#include <windows.h>
#include <iostream>
#include "Tools.h"

void FatalError(const char* msg, int exitCode) {
	OutputDebugStringA("\n\n[!] FATAL ERROR: ");
	OutputDebugStringA(msg);
	OutputDebugStringA("\n\n");
	exit(exitCode);
}

void dbgprint(const wchar_t* str, ...) {
	va_list argp;
	va_start(argp, str);
	wchar_t dbg_out[512];
	vswprintf_s(dbg_out, str, argp);
	va_end(argp);
	OutputDebugString(dbg_out);
}

void dbgprint(const char* str, ...) {
	va_list argp;
	va_start(argp, str);
	char dbg_out[512];
	vsprintf_s(dbg_out, str, argp);
	va_end(argp);
	OutputDebugStringA(dbg_out);
}

// печатает последнюю ошибку, связанную с системой (полученную при помощи GetLastError())
void syserrprint(const char* msg) {
	LPSTR dbg_out;
	auto err = GetLastError();
	WORD maxLen = 0xFF;
	FormatMessageA(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&dbg_out, 0, NULL);
	OutputDebugStringA(msg);
	OutputDebugStringA(dbg_out);
	LocalFree(dbg_out);
}

void Timer::start() {
	startTime = std::chrono::high_resolution_clock::now();
}

void Timer::stop() {
	stopTime = std::chrono::high_resolution_clock::now();
}

void Timer::printMS(const char* msg) {
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime);
	if (!msg) msg = "Timer:";
	dbgprint("%s %dms\n", msg, duration);
}

void Timer::printS(const char* msg) {
	auto duration = std::chrono::duration_cast<std::chrono::seconds>(stopTime - startTime);
	if (!msg) msg = "Timer:";
	dbgprint("%s %dms\n", msg, duration);
}