#pragma once

#include <Windows.h>

//크래시 나는 경우, 이 함수를 통해 핸들링
class FCrashHandler
{
public:
	static void Install();

	FCrashHandler() = delete;

	enum class ECrashType
	{
		AccessViolation,
		Terminate,
		Purecall,
		InvalidParameter,
		Abort,
		Recurse
	};

	static void CauseCrash(ECrashType Type);

	// 콘솔 명령: CauseCrash [av|terminate|purecall|invalid|abort]
	static bool ExecCommand(const FString& CommandLine, FString& OutMessage);


private:
	static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* ExceoptionInfo);

	static DWORD WINAPI DumpThreadProc(LPVOID Param);
	static bool WriteMiniDump(EXCEPTION_POINTERS* ExceptionInfo, DWORD ThreadId); // ThreadId 인자 추가

	static HANDLE CrashEvent;    // 크래시 스레드 → 덤프 스레드: "일어나"
	static HANDLE DumpDoneEvent; // 덤프 스레드 → 크래시 스레드: "끝났어"
	static HANDLE DumpThread;
	static EXCEPTION_POINTERS* PendingException;
	static DWORD CrashedThreadId;

	static void __cdecl PureCallHandler();
	static void __cdecl InvalidParameterHandler(const wchar_t* Expression, const wchar_t* Function, const wchar_t* File, unsigned int Line, uintptr_t Reserved);
	static void __cdecl AbortSignalHandler(int Signal);
	static void __cdecl TerminateHandler();

	static wchar_t DumpDirectory[MAX_PATH];
	static volatile LONG bIsHandlingCrash;
};