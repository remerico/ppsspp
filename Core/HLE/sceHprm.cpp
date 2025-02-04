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

#include "HLE.h"
#include "../MIPS/MIPS.h"

#include "sceCtrl.h"

u32 sceHprmPeekCurrentKey(u32 keyAddress)
{
	INFO_LOG(HLE,"0=sceHprmPeekCurrentKey(ptr)");
	Memory::Write_U32(0, keyAddress);
	return 0;
}

const HLEFunction sceHprm[] = 
{
	{0x089fdfa4, 0, "sceHprm_0x089fdfa4"},
	{0x1910B327, &WrapU_U<sceHprmPeekCurrentKey>, "sceHprmPeekCurrentKey"},
	{0x208DB1BD, 0, "sceHprmIsRemoteExist"},
	{0x7E69EDA4, 0, "sceHprmIsHeadphoneExist"},
	{0x219C58F1, 0, "sceHprmIsMicrophoneExist"},
	{0xC7154136, 0, "sceHprmRegisterCallback"},
	{0x444ED0B7, 0, "sceHprmUnregisterCallback"},
	{0x1910B327, 0, "sceHprmPeekCurrentKey"},
	{0x2BCEC83E, 0, "sceHprmPeekLatch"},
	{0x40D2F9F0, 0, "sceHprmReadLatch"},
};
const int sceHprmCount = ARRAY_SIZE(sceHprm);

void Register_sceHprm()
{
	RegisterModule("sceHprm", sceHprmCount, sceHprm);
}
