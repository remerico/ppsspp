// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Setup.h"
#include "Thread.h"
#include "Common.h"

#ifdef __APPLE__
#include <mach/mach.h>
#elif defined BSD4_4
#include <pthread_np.h>
#endif

#ifdef USE_BEGINTHREADEX
#include <process.h>
#endif

namespace Common
{

int CurrentThreadId()
{
#ifdef _WIN32
	return GetCurrentThreadId();
#elif defined __APPLE__
	return mach_thread_self();
#else
	return 0;
#endif
}
	
#ifdef _WIN32

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask)
{
	SetThreadAffinityMask(thread, mask);
}

void SetCurrentThreadAffinity(u32 mask)
{
	SetThreadAffinityMask(GetCurrentThread(), mask);
}

// Supporting functions
void SleepCurrentThread(int ms)
{
	Sleep(ms);
}
	
void SwitchCurrentThread()
{
	SwitchToThread();
}

// Sets the debugger-visible name of the current thread.
// Uses undocumented (actually, it is now documented) trick.
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/vsdebug/html/vxtsksettingthreadname.asp
	
// This is implemented much nicer in upcoming msvc++, see:
// http://msdn.microsoft.com/en-us/library/xcb2z8hs(VS.100).aspx
void SetCurrentThreadName(const char* szThreadName)
{
	static const DWORD MS_VC_EXCEPTION = 0x406D1388;

	#pragma pack(push,8)
	struct THREADNAME_INFO
	{
		DWORD dwType; // must be 0x1000
		LPCSTR szName; // pointer to name (in user addr space)
		DWORD dwThreadID; // thread ID (-1=caller thread)
		DWORD dwFlags; // reserved for future use, must be zero
	} info;
	#pragma pack(pop)

	info.dwType = 0x1000;
	info.szName = szThreadName;
	info.dwThreadID = -1; //dwThreadID;
	info.dwFlags = 0;

	__try
	{
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except(EXCEPTION_CONTINUE_EXECUTION)
	{}
}

void EnableCrashingOnCrashes() 
{ 
  typedef BOOL (WINAPI *tGetPolicy)(LPDWORD lpFlags); 
  typedef BOOL (WINAPI *tSetPolicy)(DWORD dwFlags); 
  const DWORD EXCEPTION_SWALLOWING = 0x1;

  HMODULE kernel32 = LoadLibraryA("kernel32.dll"); 
  tGetPolicy pGetPolicy = (tGetPolicy)GetProcAddress(kernel32, 
    "GetProcessUserModeExceptionPolicy"); 
  tSetPolicy pSetPolicy = (tSetPolicy)GetProcAddress(kernel32, 
    "SetProcessUserModeExceptionPolicy"); 
  if (pGetPolicy && pSetPolicy) 
  { 
    DWORD dwFlags; 
    if (pGetPolicy(&dwFlags)) 
    { 
      // Turn off the filter 
      pSetPolicy(dwFlags & ~EXCEPTION_SWALLOWING); 
    } 
  } 
}


#else // !WIN32, so must be POSIX threads

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask)
{                
#ifdef __APPLE__
	thread_policy_set(pthread_mach_thread_np(thread),
		THREAD_AFFINITY_POLICY, (integer_t *)&mask, 1);
#elif defined __linux__ || defined BSD4_4
	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
                
	for (int i = 0; i != sizeof(mask) * 8; ++i)
		if ((mask >> i) & 1)
			CPU_SET(i, &cpu_set);
                
	pthread_setaffinity_np(thread, sizeof(cpu_set), &cpu_set);
#endif
}

void SetCurrentThreadAffinity(u32 mask)
{
	SetThreadAffinity(pthread_self(), mask);
}

static pthread_key_t threadname_key;
static pthread_once_t threadname_key_once = PTHREAD_ONCE_INIT;

void SleepCurrentThread(int ms)
{
	usleep(1000 * ms);
}
	
void SwitchCurrentThread()
{
	usleep(1000 * 1);
}

static void FreeThreadName(void* threadname)
{
	free(threadname);
}

static void ThreadnameKeyAlloc()
{
	pthread_key_create(&threadname_key, FreeThreadName);
}

void SetCurrentThreadName(const char* szThreadName)
{
	pthread_once(&threadname_key_once, ThreadnameKeyAlloc);

	void* threadname;
	if ((threadname = pthread_getspecific(threadname_key)) != NULL)
		free(threadname);

	pthread_setspecific(threadname_key, strdup(szThreadName));

	INFO_LOG(COMMON, "%s(%s)\n", __FUNCTION__, szThreadName);
}

void EnableCrashingOnCrashes() 
{

}

#endif
	
} // namespace Common
