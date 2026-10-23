// Development aid: writes a symbolized stack trace to crash.txt on an unhandled
// exception. Registered through a CRT initializer, so no engine code changes.
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS *ep)
{
	static LONG reported;
	DWORD code = ep->ExceptionRecord->ExceptionCode;
	if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_STACK_OVERFLOW &&
		code != EXCEPTION_ILLEGAL_INSTRUCTION && code != EXCEPTION_INT_DIVIDE_BY_ZERO)
		return EXCEPTION_CONTINUE_SEARCH;
	if (InterlockedExchange(&reported, 1))
		return EXCEPTION_CONTINUE_SEARCH;
	{
	HANDLE process = GetCurrentProcess();
	HANDLE thread = GetCurrentThread();
	CONTEXT ctx = *ep->ContextRecord;
	STACKFRAME64 frame = {0};
	char symbuf[sizeof(SYMBOL_INFO) + 256];
	SYMBOL_INFO *sym = (SYMBOL_INFO *)symbuf;
	IMAGEHLP_LINE64 line;
	DWORD64 disp;
	DWORD lineDisp;
	FILE *f;
	int i;

	f = fopen("crash.txt", "w");
	if (!f)
		return EXCEPTION_CONTINUE_SEARCH;

	fprintf(f, "Exception 0x%08lX at %p\n", ep->ExceptionRecord->ExceptionCode,
		ep->ExceptionRecord->ExceptionAddress);
	if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
		fprintf(f, "%s address %p\n",
			ep->ExceptionRecord->ExceptionInformation[0] == 8 ? "Executing (DEP)" :
			ep->ExceptionRecord->ExceptionInformation[0] ? "Writing" : "Reading",
			(void *)ep->ExceptionRecord->ExceptionInformation[1]);

	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
	SymInitialize(process, NULL, TRUE);

#ifdef _M_IX86
	frame.AddrPC.Offset = ctx.Eip;
	frame.AddrFrame.Offset = ctx.Ebp;
	frame.AddrStack.Offset = ctx.Esp;
#define MACHINE IMAGE_FILE_MACHINE_I386
#else
	frame.AddrPC.Offset = ctx.Rip;
	frame.AddrFrame.Offset = ctx.Rbp;
	frame.AddrStack.Offset = ctx.Rsp;
#define MACHINE IMAGE_FILE_MACHINE_AMD64
#endif
	frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;

	for (i = 0; i < 64; i++) {
		if (!StackWalk64(MACHINE, process, thread, &frame, &ctx, NULL,
				SymFunctionTableAccess64, SymGetModuleBase64, NULL) || !frame.AddrPC.Offset)
			break;
		sym->SizeOfStruct = sizeof(SYMBOL_INFO);
		sym->MaxNameLen = 255;
		line.SizeOfStruct = sizeof(line);
		if (SymFromAddr(process, frame.AddrPC.Offset, &disp, sym)) {
			if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &lineDisp, &line))
				fprintf(f, "  %s  (%s:%lu)\n", sym->Name, line.FileName, line.LineNumber);
			else
				fprintf(f, "  %s +0x%llx\n", sym->Name, disp);
		} else {
			fprintf(f, "  %p\n", (void *)frame.AddrPC.Offset);
		}
	}
	fclose(f);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

static int __cdecl InstallCrashFilter(void)
{
	AddVectoredExceptionHandler(1, CrashFilter);
	OutputDebugStringA("crashreport: handler installed\n");
	return 0;
}

#pragma section(".CRT$XIU", long, read)
__declspec(allocate(".CRT$XIU")) static int (__cdecl *crashFilterInit)(void) = InstallCrashFilter;
