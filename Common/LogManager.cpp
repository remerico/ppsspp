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

#include <algorithm>

#include "LogManager.h"
#include "ConsoleListener.h"
#include "Timer.h"
#include "Thread.h"
#include "FileUtil.h"

void GenericLog(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, 
		const char *file, int line, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	if (LogManager::GetInstance())
		LogManager::GetInstance()->Log(level, type,
			file, line, fmt, args);
	va_end(args);
}

LogManager *LogManager::m_logManager = NULL;

LogManager::LogManager()
{
	// create log files
  m_Log[LogTypes::MASTER_LOG] = new LogContainer("*",				"Master Log");
  m_Log[LogTypes::BOOT]       = new LogContainer("BOOT",			"Boot");
  m_Log[LogTypes::COMMON]     = new LogContainer("COMMON",		"Common");
  m_Log[LogTypes::CPU]        = new LogContainer("CPU",			"CPU");
  m_Log[LogTypes::LOADER]     = new LogContainer("LOAD",			"Loader");
  m_Log[LogTypes::IO]         = new LogContainer("IO",	    	"IO");
  m_Log[LogTypes::DISCIO]     = new LogContainer("DIO",	    	"DiscIO");
  m_Log[LogTypes::PAD]        = new LogContainer("PAD",			"Pad");
  m_Log[LogTypes::FILESYS]    = new LogContainer("FileSys",		"File System");
  m_Log[LogTypes::G3D]        = new LogContainer("G3D",			"3D Graphics");
  m_Log[LogTypes::DMA]        = new LogContainer("DMA",			"DMA");
  m_Log[LogTypes::INTC]       = new LogContainer("INTC",			"Interrupts");
  m_Log[LogTypes::MEMMAP]     = new LogContainer("MM",		"Memory Map");
  m_Log[LogTypes::SOUND]      = new LogContainer("SND",			"Sound");
  m_Log[LogTypes::HLE]        = new LogContainer("HLE",			"HLE");
  m_Log[LogTypes::TIMER]      = new LogContainer("TMR",			"Timer");
  m_Log[LogTypes::VIDEO]      = new LogContainer("VID",			"Video");
  m_Log[LogTypes::DYNA_REC]   = new LogContainer("Jit",			"JIT compiler");
  m_Log[LogTypes::NETPLAY]    = new LogContainer("NET",			"Net play");

#ifndef ANDROID
	m_fileLog = new FileLogListener(File::GetUserPath(F_MAINLOG_IDX).c_str());
	m_consoleLog = new ConsoleListener();
	m_debuggerLog = new DebuggerLogListener();
#endif

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		m_Log[i]->SetEnable(true);
#ifndef ANDROID
		m_Log[i]->AddListener(m_fileLog);
		m_Log[i]->AddListener(m_consoleLog);
#ifdef _MSC_VER
		if (IsDebuggerPresent())
			m_Log[i]->AddListener(m_debuggerLog);
#endif
#endif
	}
}

LogManager::~LogManager()
{
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
#ifndef ANDROID
		m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_fileLog);
		m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_consoleLog);
		m_logManager->RemoveListener((LogTypes::LOG_TYPE)i, m_debuggerLog);
#endif
	}

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		delete m_Log[i];
#ifndef ANDROID
	delete m_fileLog;
	delete m_consoleLog;
#endif
}

void LogManager::SaveConfig(IniFile::Section *section)
{
  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
  {
    section->Set((std::string(m_Log[i]->GetShortName()) + "Enabled").c_str(), m_Log[i]->IsEnabled());
    section->Set((std::string(m_Log[i]->GetShortName()) + "Level").c_str(), (int)m_Log[i]->GetLevel());
  }
}

void LogManager::LoadConfig(IniFile::Section *section)
{
  for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
  {
    bool enabled;
    int level;
    section->Get((std::string(m_Log[i]->GetShortName()) + "Enabled").c_str(), &enabled, true);
    section->Get((std::string(m_Log[i]->GetShortName()) + "Level").c_str(), &level, 0);
    m_Log[i]->SetEnable(enabled);
    m_Log[i]->SetLevel((LogTypes::LOG_LEVELS)level);
  }
}

void LogManager::Log(LogTypes::LOG_LEVELS level, LogTypes::LOG_TYPE type, const char *file, int line, const char *format, va_list args)
{
	char temp[MAX_MSGLEN];
	char msg[MAX_MSGLEN * 2];
	LogContainer *log = m_Log[type];
	if (!log || !log->IsEnabled() || level > log->GetLevel() || ! log->HasListeners())
		return;

	CharArrayFromFormatV(temp, MAX_MSGLEN, format, args);

	static const char level_to_char[7] = "-NEWID";
  char formattedTime[13];
  Common::Timer::GetTimeFormatted(formattedTime);
	sprintf(msg, "%s %s:%u %c[%s]: %s\n",
		formattedTime,
		file, line, level_to_char[(int)level],
		log->GetShortName(), temp);

	log->Trigger(level, msg);
}

void LogManager::Init()
{
	m_logManager = new LogManager();
}

void LogManager::Shutdown()
{
	delete m_logManager;
	m_logManager = NULL;
}

LogContainer::LogContainer(const char* shortName, const char* fullName, bool enable)
	: m_enable(enable)
{
	strncpy(m_fullName, fullName, 128);
	strncpy(m_shortName, shortName, 32);
	m_level = LogTypes::LDEBUG;
}

// LogContainer
void LogContainer::AddListener(LogListener *listener)
{
	std::lock_guard<std::mutex> lk(m_listeners_lock);
	m_listeners.insert(listener);
}

void LogContainer::RemoveListener(LogListener *listener)
{
	std::lock_guard<std::mutex> lk(m_listeners_lock);
	m_listeners.erase(listener);
}

void LogContainer::Trigger(LogTypes::LOG_LEVELS level, const char *msg)
{
	std::lock_guard<std::mutex> lk(m_listeners_lock);

	std::set<LogListener*>::const_iterator i;
	for (i = m_listeners.begin(); i != m_listeners.end(); ++i)
	{
		(*i)->Log(level, msg);
	}
}

FileLogListener::FileLogListener(const char *filename)
{
	m_logfile.open(filename, std::ios::app);
	SetEnable(true);
}

void FileLogListener::Log(LogTypes::LOG_LEVELS, const char *msg)
{
	if (!IsEnabled() || !IsValid())
		return;

	std::lock_guard<std::mutex> lk(m_log_lock);
	m_logfile << msg << std::flush;
}

void DebuggerLogListener::Log(LogTypes::LOG_LEVELS, const char *msg)
{
#if _MSC_VER
	::OutputDebugStringA(msg);
#endif
}
