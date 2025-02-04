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

#include "../Globals.h"
#include "CoreParameter.h"

// called from emu thread
void Core_Run();
void Core_Pause();
void Core_Stop();
void Core_ErrorPause();
// called from gui
void Core_EnableStepping(bool step);
void Core_DoSingleStep();

void Core_Halt(const char *msg);

bool Core_IsStepping();

enum CoreState
{
	CORE_RUNNING,
	CORE_STEPPING,
	CORE_POWERDOWN,
	CORE_ERROR
};


extern volatile CoreState coreState;
