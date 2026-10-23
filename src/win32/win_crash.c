// Development aid: writes a symbolized stack trace to crash.txt on an unhandled
// exception. Registered through a CRT initializer, so no engine code changes.
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>

void GLimp_RestoreGamma(void);	// sdl_glimp.c

static char crashPath[MAX_PATH];	// crash.txt next to the executable

// true if 'address' is in a module loaded from the executable's folder (or below)
static BOOL IsOurModule(void *address)
{
	HMODULE	hm;
	char	path[MAX_PATH], exeDir[MAX_PATH], *slash;

	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)address, &hm))
		return TRUE;	// not in any module: our own corrupted code pointer
	GetModuleFileNameA(hm, path, sizeof(path));
	GetModuleFileNameA(NULL, exeDir, sizeof(exeDir));
	slash = strrchr(exeDir, '\\');
	if (slash)
		slash[1] = 0;
	return _strnicmp(path, exeDir, strlen(exeDir)) == 0;
}

static LONG WINAPI CrashFilter(EXCEPTION_POINTERS *ep)
{
	static LONG reported;
	DWORD code = ep->ExceptionRecord->ExceptionCode;
	// only fatal NTSTATUS codes (0xC...); C++ and debugger exceptions are not crashes
	if ((code & 0xF0000000) != 0xC0000000 || code == 0xE06D7363)
		return EXCEPTION_CONTINUE_SEARCH;
	// drivers raise and handle their own exceptions; only faults in our
	// modules (the exe, game dlls, SDL2) are crashes
	if (!IsOurModule(ep->ExceptionRecord->ExceptionAddress))
		return EXCEPTION_CONTINUE_SEARCH;
	if (InterlockedExchange(&reported, 1))
		return EXCEPTION_CONTINUE_SEARCH;

	// a dead process never gets to restore the desktop gamma ramp
	GLimp_RestoreGamma();
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

	f = fopen(crashPath, "w");
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

	{
		char msg[MAX_PATH + 128];

		_snprintf(msg, sizeof(msg), "Q3Doom crashed.\n\nA report was written to:\n%s", crashPath);
		msg[sizeof(msg) - 1] = 0;
		MessageBoxA(NULL, msg, "Q3Doom", MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND);
	}
	return EXCEPTION_CONTINUE_SEARCH;
}

static int __cdecl InstallCrashFilter(void)
{
	char *slash;

	GetModuleFileNameA(NULL, crashPath, sizeof(crashPath) - 16);
	slash = strrchr(crashPath, '\\');
	strcpy(slash ? slash + 1 : crashPath, "crash.txt");

	AddVectoredExceptionHandler(1, CrashFilter);
	return 0;
}

#pragma section(".CRT$XIU", long, read)
__declspec(allocate(".CRT$XIU")) static int (__cdecl *crashFilterInit)(void) = InstallCrashFilter;
