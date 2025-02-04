// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.


#pragma once

#include <string>

enum CPUCore {
	CPU_INTERPRETER,
	CPU_JIT,
};

enum GPUCore {
	GPU_NULL,
	GPU_GLES,
	GPU_SOFTWARE,
};


struct CoreParameter
{
	// 0 = Interpreter
	// 1 = Jit
	// 2 = JitIL
	CPUCore cpuCore;
	GPUCore gpuCore;
	bool enableSound;  // there aren't multiple sound cores.

	std::string fileToStart;
	std::string mountIso;  // If non-empty, and fileToStart is an ELF or PBP, will mount this ISO in the background.

	bool startPaused;
	bool enableDebugging;  // enables breakpoints and other time-consuming debugger features
	bool printfEmuLog;  // writes "emulator:" logging to stdout
	bool headLess;   // Try to avoid messageboxes etc
};
