// NOTE: Apologies for the quality of this code, this is really from pre-opensource Dolphin - that is, 2003.

#include "base/threadutil.h"
#include "Log.h"
#include "../Globals.h"
#include "EmuThread.h"
#include "../Core/MemMap.h"
#include "../Core/Core.h"
#include "../Core/Host.h"
#include "../Core/System.h"
#include "../Core/Config.h"

char fileToStart[MAX_PATH];

static HANDLE emuThread;


HANDLE EmuThread_GetThreadHandle()
{
	return emuThread;
}


DWORD TheThread(LPVOID x);

void EmuThread_Start(const char *filename)
{
	// _dbg_clear_();
	_tcscpy(fileToStart, filename);

	unsigned int i;
	emuThread = (HANDLE)_beginthreadex(0,0,(unsigned int (__stdcall *)(void *))TheThread,(LPVOID)0,0,&i);
}

void EmuThread_Stop()
{
//	DSound_UpdateSound();
	Core_Stop();
	if (WAIT_TIMEOUT == WaitForSingleObject(EmuThread_GetThreadHandle(),300))
	{
		//MessageBox(0,"Wait for emuthread timed out, please alert the developer to possible deadlock or infinite loop in emuthread :(.",0,0);
	}
	host->UpdateUI();
}


char *GetCurrentFilename()
{
	return fileToStart;
}

DWORD TheThread(LPVOID x)
{
	setCurrentThreadName("EmuThread");

	g_State.bEmuThreadStarted = true;

	host->UpdateUI();
	host->InitGL();

	INFO_LOG(BOOT, "Starting up hardware.");

  CoreParameter coreParameter;
  coreParameter.fileToStart = fileToStart;
  coreParameter.enableSound = true;
  coreParameter.gpuCore = GPU_GLES;
  coreParameter.cpuCore = g_Config.bJIT ? CPU_JIT : CPU_INTERPRETER;
  coreParameter.enableDebugging = true;
  coreParameter.printfEmuLog = false;
  coreParameter.headLess = false; //true;

	std::string error_string;
	if (!PSP_Init(coreParameter, &error_string))
	{
		ERROR_LOG(BOOT, "Error loading: %s", error_string.c_str());
		goto shutdown;
	}

	INFO_LOG(BOOT, "Done.");
	_dbg_update_();

	if (g_Config.bAutoRun)
	{
#ifdef _DEBUG
		host->UpdateDisassembly();
#endif
		Core_EnableStepping(FALSE);
	}
	else
	{
#ifdef _DEBUG
		host->UpdateDisassembly();
#endif
		Core_EnableStepping(TRUE);
	}

	g_State.bBooted = true;
#ifdef _DEBUG
	host->UpdateMemView();
#endif

	host->BootDone();
	Core_Run();

	host->PrepareShutdown();

shutdown:

	PSP_Shutdown();

	host->ShutdownGL();
	
	//The CPU should return when a game is stopped and cleanup should be done here, 
	//so we can restart the plugins (or load new ones) for the next game
	g_State.bEmuThreadStarted = false;
	_endthreadex(0);
	return 0;
}


