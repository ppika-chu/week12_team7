#include "EnginePCH.h"
#include "CrashHandler.h"

#include <DbgHelp.h>
#include <csignal>
#include <cstdlib>
#include <exception>

#include <cctype>
#include <cstdio>
#include <stdexcept>

#pragma comment(lib, "dbghelp.lib")

namespace
{
	constexpr DWORD CrashCode_PureCall			= 0xE0000001;
	constexpr DWORD CrashCode_InvalidParameter	= 0xE0000002;
	constexpr DWORD CrashCode_Abort				= 0xE0000003;
	constexpr DWORD CrashCode_Terminate			= 0xE0000004;

	void PrintError(const wchar_t* string)
	{
#ifdef ENGINE_DEBUG
		DWORD Error = ::GetLastError();
		wchar_t Message[256];
		swprintf_s(Message, L"%s failed. Error = %lu", string, Error);
		::MessageBoxW(nullptr, Message, L"CrashHandler", MB_OK | MB_ICONERROR);
#endif
	}

	// 순수 가상 함수 호출 재현
	struct FPureCallBase
	{
		FPureCallBase() { CallPure(this); }
		virtual void Pure() = 0;
		__declspec(noinline) static void CallPure(FPureCallBase* Object)
		{
			Object->Pure();
		}
	};

	struct FPureCallDerived : FPureCallBase
	{
		void Pure() override {}
	};

	//Terminate 재현(noexept안에서 except 발생)
	__declspec(noinline) void ThrowException()
	{
		throw std::runtime_error("CauseCrash");
	}
	__declspec(noinline) void ThrowThroughNoexcept() noexcept
	{
		ThrowException();
	}

	// 콘솔 입력은 대소문자를 구분하지 않는다. UE도 "STAT FPS"와 "stat fps"를 같게 다룬다.
	bool EqualsIgnoreCase(const FString& A, const char* B)
	{
		uint64 Index = 0;
		for (; Index < A.size() && B[Index] != '\0'; ++Index)
		{
			const int Left = std::tolower(static_cast<unsigned char>(A[Index]));
			const int Right = std::tolower(static_cast<unsigned char>(B[Index]));
			if (Left != Right)
			{
				return false;
			}
		}
		return Index == A.size() && B[Index] == '\0';
	}

	// 공백 구분 토큰으로 나눈다. 연속 공백과 앞뒤 공백은 무시한다.
	void Tokenize(const FString& Line, TArray<FString>& OutTokens)
	{
		OutTokens.Reset();
		uint64 Cursor = 0;
		while (Cursor < Line.size())
		{
			while (Cursor < Line.size() && std::isspace(static_cast<unsigned char>(Line[Cursor])))
			{
				++Cursor;
			}
			const uint64 Start = Cursor;
			while (Cursor < Line.size() && !std::isspace(static_cast<unsigned char>(Line[Cursor])))
			{
				++Cursor;
			}
			if (Cursor > Start)
			{
				OutTokens.Add(Line.substr(Start, Cursor - Start));
			}
		}
	}
	}

wchar_t FCrashHandler::DumpDirectory[MAX_PATH] = {};
volatile LONG FCrashHandler::bIsHandlingCrash = 0;


void FCrashHandler::Install()
{
	wchar_t ProcessPath[MAX_PATH];
	DWORD Result = GetModuleFileNameW(nullptr, ProcessPath, MAX_PATH);
	if (Result == 0)
	{
		PrintError(L"GetModuleFileNameW");
	}

	// "/Hitori.exe"는 삭제
	if (wchar_t* LastSlash = wcsrchr(ProcessPath, L'\\'))
	{
		*LastSlash = L'\0'; // "...\\Debug-windows-x86_64" 까지만 남김
	}

	wchar_t SavedDirectory[MAX_PATH];
	swprintf_s(SavedDirectory, L"%s\\Saved", ProcessPath);
	::CreateDirectoryW(SavedDirectory, nullptr);

	swprintf_s(DumpDirectory, L"%s\\Saved\\Crashes", ProcessPath);
	BOOL bRes = CreateDirectoryW(DumpDirectory, nullptr);
	if (bRes == FALSE && ::GetLastError() != ERROR_ALREADY_EXISTS)
	{
		PrintError(L"CreateDirectoryW");
	}

	::SetUnhandledExceptionFilter(&FCrashHandler::UnhandledExceptionFilter);
		
	_set_purecall_handler(&FCrashHandler::PureCallHandler);
	_set_invalid_parameter_handler(&FCrashHandler::InvalidParameterHandler);
	std::signal(SIGABRT, &FCrashHandler::AbortSignalHandler);
	std::set_terminate(&FCrashHandler::TerminateHandler);
}

void FCrashHandler::CauseCrash(ECrashType Type)
{
	switch (Type)
	{
		case ECrashType::AccessViolation:
		{
			volatile int* Null = nullptr;
			*Null = 0;
			break;
		}
		case ECrashType::Terminate:
		{
			ThrowThroughNoexcept();
			break;
		}
		case ECrashType::Purecall:
		{
			FPureCallDerived Object;
			break;
		}
		case ECrashType::InvalidParameter:
		{
			const char* volatile Format = nullptr;
			printf(Format);
			break;
		}
		case ECrashType::Abort:
		{
			abort();
			break;
		}
	}
}

bool FCrashHandler::ExecCommand(const FString& CommandLine, FString& OutMessage)
{
	TArray<FString> Tokens;
	Tokenize(CommandLine, Tokens);

	if (Tokens.Num() == 0 || !EqualsIgnoreCase(Tokens[0], "causecrash"))
	{
		return false;
	}

	if (Tokens.Num() < 2)
	{
		CauseCrash(ECrashType::AccessViolation);
		return true;
	}

	const FString& Arg = Tokens[1];
	if (EqualsIgnoreCase(Arg, "av")){ CauseCrash(ECrashType::AccessViolation); }
	else if (EqualsIgnoreCase(Arg, "terminate")) { CauseCrash(ECrashType::Terminate); }
	else if (EqualsIgnoreCase(Arg, "purecall")){ CauseCrash(ECrashType::Purecall); }
	else if (EqualsIgnoreCase(Arg, "invalid")){ CauseCrash(ECrashType::InvalidParameter); }
	else if (EqualsIgnoreCase(Arg, "abort")){ CauseCrash(ECrashType::Abort); }
	else
	{
		OutMessage = "Usage: CauseCrash [av|terminate|purecall|invalid|abort]";
	}

	return true;
}

LONG __stdcall FCrashHandler::UnhandledExceptionFilter(EXCEPTION_POINTERS* ExceptionInfo)
{
	//첫 진입만 통과, 다른 스레드가 동시에 죽으면 여기서 대기시킨다.
	if (::_InterlockedCompareExchange(&bIsHandlingCrash, 1, 0) != 0)
	{
		::Sleep(INFINITE);
	}

	WriteMiniDump(ExceptionInfo);

	// TODO(선택)  MessageBoxW로 덤프 경로 안내
	//::MessageBoxW(nullptr, L"")

	// TODO (선택) TerminateProcess(GetCurrentProcess(), ExceptionInfo->ExceptionRecord->ExceptionCode)
	TerminateProcess(GetCurrentProcess(), ExceptionInfo->ExceptionRecord->ExceptionCode);

	return EXCEPTION_EXECUTE_HANDLER;
}

bool FCrashHandler::WriteMiniDump(EXCEPTION_POINTERS* ExceptionInfo)
{
	wchar_t DumpPath[MAX_PATH];
	SYSTEMTIME Now;
	::GetLocalTime(&Now);

	DWORD Pid = ::GetCurrentProcessId();    // 프로세스 번호
	//DWORD Tid = ::GetCurrentThreadId();     // 지금 이 코드를 실행 중인 스레드 번호
	//HANDLE Process = ::GetCurrentProcess(); // 프로세스 핸들 (닫을 필요 없음

	//현재 시간 기준으로 덤프 경로 생성
	swprintf_s(DumpPath, L"%s\\Crash_%04d%02d%02d_%02d%02d%02d_%lu.dmp", DumpDirectory, Now.wYear, Now.wMonth, Now.wDay, Now.wHour, Now.wMinute, Now.wSecond, Pid);


	//Dump 파일 생성
	HANDLE File = CreateFileW(DumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (File == INVALID_HANDLE_VALUE)
	{
		PrintError(L"CreateFileW");
		return false;
	}

	//MINIDUMP_EXCEPTION_INFORMATION 채우기
	MINIDUMP_EXCEPTION_INFORMATION ExceptionParam;
	ExceptionParam.ThreadId = GetCurrentThreadId();
	ExceptionParam.ExceptionPointers = ExceptionInfo;
	ExceptionParam.ClientPointers = FALSE;

	//MINIDUMP_TYPE 결정
	//플래그를 |로 합치면 int가 되어서 static_cast<MINIDUMP_TYPE>이 필요
	const MINIDUMP_TYPE Type = static_cast<MINIDUMP_TYPE>(
		MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithDataSegs);

	//덤프 파일에 내용 작성
	const BOOL bOK = ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), File, Type, &ExceptionParam, nullptr, nullptr);

	// TODO 6. CloseHandle(File), 성공 여부 반환
	CloseHandle(File);
	return bOK != FALSE;
}

void __cdecl FCrashHandler::PureCallHandler()
{
	::RaiseException(CrashCode_PureCall, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}

void __cdecl FCrashHandler::InvalidParameterHandler(const wchar_t* Expression, const wchar_t* Function, const wchar_t* File, unsigned int Line, uintptr_t Reserved)
{
	::RaiseException(CrashCode_InvalidParameter, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}

void __cdecl FCrashHandler::AbortSignalHandler(int Signal)
{
	::RaiseException(CrashCode_Abort, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}

void __cdecl FCrashHandler::TerminateHandler()
{
	::RaiseException(CrashCode_Terminate, EXCEPTION_NONCONTINUABLE, 0, nullptr);
}
