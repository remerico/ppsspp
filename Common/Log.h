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

#ifndef _LOG_H_
#define _LOG_H_

#define	NOTICE_LEVEL  1  // VERY important information that is NOT errors. Like startup and OSReports.
#define	ERROR_LEVEL   2  // Critical errors 
#define	WARNING_LEVEL 3  // Something is suspicious.
#define	INFO_LEVEL    4  // General information.
#define	DEBUG_LEVEL   5  // Detailed debugging - might make things slow.

namespace LogTypes
{

enum LOG_TYPE {
  MASTER_LOG,
  BOOT,
  COMMON,
  CPU,
  LOADER,
  IO,
  PAD,
  FILESYS,
  DISCIO,
  G3D,
  DMA,
  INTC,
  MEMMAP,
  SOUND,
  HLE,
  TIMER,
  VIDEO,
  DYNA_REC,
  NETPLAY,

  NUMBER_OF_LOGS,  // Must be last
  JIT = DYNA_REC,
};

// FIXME: should this be removed?
enum LOG_LEVELS {
	LNOTICE = NOTICE_LEVEL,
	LERROR = ERROR_LEVEL,
	LWARNING = WARNING_LEVEL,
	LINFO = INFO_LEVEL,
	LDEBUG = DEBUG_LEVEL,
};

#define LOGTYPES_LEVELS LogTypes::LOG_LEVELS
#define LOGTYPES_TYPE LogTypes::LOG_TYPE

}  // namespace

void GenericLog(LOGTYPES_LEVELS level, LOGTYPES_TYPE type,
		const char *file, int line, const char *fmt, ...)
#ifdef __GNUC__
		__attribute__((format(printf, 5, 6)))
#endif
		;

#if defined(LOGGING) || defined(_DEBUG) || defined(DEBUGFAST) // || defined(ANDROID) //|| defined(__APPLE__)//
#define MAX_LOGLEVEL DEBUG_LEVEL
#else
#ifndef MAX_LOGLEVEL
#define MAX_LOGLEVEL INFO_LEVEL
//#define MAX_LOGLEVEL NOTICE_LEVEL
#endif // loglevel
#endif // logging

#ifdef GEKKO
#define GENERIC_LOG(t, v, ...)
#else
// Let the compiler optimize this out
#define GENERIC_LOG(t, v, ...) { \
	if (v <= MAX_LOGLEVEL) \
		GenericLog(v, t, __FILE__, __LINE__, __VA_ARGS__); \
	}
#endif

#define ERROR_LOG(t,...) { GENERIC_LOG(LogTypes::t, LogTypes::LERROR, __VA_ARGS__) }
#define WARN_LOG(t,...) { GENERIC_LOG(LogTypes::t, LogTypes::LWARNING, __VA_ARGS__) }
#define NOTICE_LOG(t,...) { GENERIC_LOG(LogTypes::t, LogTypes::LNOTICE, __VA_ARGS__) }
#define INFO_LOG(t,...) { GENERIC_LOG(LogTypes::t, LogTypes::LINFO, __VA_ARGS__) }
#define DEBUG_LOG(t,...) { GENERIC_LOG(LogTypes::t, LogTypes::LDEBUG, __VA_ARGS__) }

#if MAX_LOGLEVEL >= DEBUG_LEVEL
#define _dbg_assert_(_t_, _a_) \
	if (!(_a_)) {\
		ERROR_LOG(_t_, "Error...\n\n  Line: %d\n  File: %s\n  Time: %s\n\nIgnore and continue?", \
					   __LINE__, __FILE__, __TIME__); \
		if (!PanicYesNo("*** Assertion (see log)***\n")) {Crash();} \
	}
#define _dbg_assert_msg_(_t_, _a_, ...)\
	if (!(_a_)) {\
		ERROR_LOG(_t_, __VA_ARGS__); \
		if (!PanicYesNo(__VA_ARGS__)) {Crash();} \
	}
#define _dbg_update_() ; //Host_UpdateLogDisplay();

#else // not debug
#define _dbg_update_() ;

#ifndef _dbg_assert_
#define _dbg_assert_(_t_, _a_) {}
#define _dbg_assert_msg_(_t_, _a_, _desc_, ...) {}
#endif // dbg_assert
#endif // MAX_LOGLEVEL DEBUG

#define _assert_(_a_) _dbg_assert_(MASTER_LOG, _a_)

#ifndef GEKKO
#ifdef _WIN32
#define _assert_msg_(_t_, _a_, _fmt_, ...)		\
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, __VA_ARGS__)) {Crash();} \
	}
#else // not win32
#define _assert_msg_(_t_, _a_, _fmt_, ...)		\
	if (!(_a_)) {\
		if (!PanicYesNo(_fmt_, ##__VA_ARGS__)) {Crash();} \
	}
#endif // WIN32
#else // GEKKO
#define _assert_msg_(_t_, _a_, _fmt_, ...)
#endif

#endif // _LOG_H_
