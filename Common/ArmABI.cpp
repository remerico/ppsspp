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

#include "Common.h"
#include "ArmEmitter.h"
#include "ArmABI.h"

using namespace ArmGen;
// If passing arguments, don't use this.
void ARMXEmitter::ARMABI_CallFunction(void *func) 
{
	ARMABI_MOVI2R(R14, Mem(func));	
	PUSH(5, R0, R1, R2, R3, _LR);
	BL(R14);
	POP(5, R0, R1, R2, R3, _LR);
}
void ARMXEmitter::ARMABI_CallFunctionC(void *func, u32 Arg)
{
	ARMABI_MOVI2R(R14, Mem(func));	
	PUSH(5, R0, R1, R2, R3, _LR);
	ARMABI_MOVI2R(R0, Arg);
	BL(R14);
	POP(5, R0, R1, R2, R3, _LR);

}
void ARMXEmitter::ARMABI_CallFunctionCC(void *func, u32 Arg1, u32 Arg2)
{
	ARMABI_MOVI2R(R14, Mem(func));	
	PUSH(5, R0, R1, R2, R3, _LR);
	ARMABI_MOVI2R(R0, Arg1);
	ARMABI_MOVI2R(R1, Arg2);
	BL(R14);
	POP(5, R0, R1, R2, R3, _LR);

}
void ARMXEmitter::ARMABI_PushAllCalleeSavedRegsAndAdjustStack() {
	// Note: 4 * 4 = 16 bytes, so alignment is preserved.
	PUSH(4, R0, R1, R2, R3);
}

void ARMXEmitter::ARMABI_PopAllCalleeSavedRegsAndAdjustStack() {
	POP(4, R0, R1, R2, R3);
}
void ARMXEmitter::ARMABI_MOVI2R(ARMReg reg, Operand2 val)
{
	// TODO: There are more fancy ways to save calls if we check if 
	// The imm can be rotated or shifted a certain way.
	// Masks tend to be able to be moved in to a reg with one call
	MOVW(reg, val); 
	if(val.Value & 0xFFFF0000) 
		MOVT(reg, val, true);
}
// Moves IMM to memory location
void ARMXEmitter::ARMABI_MOVI2M(Operand2 op, Operand2 val)
{
	// This moves imm to a memory location
	MOVW(R14, val); MOVT(R14, val, true);
	MOVW(R12, op); MOVT(R12, op, true);
	STR(R12, R14); // R10 is what we want to store
}
const char *conditions[] = {"EQ", "NEQ", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE", "AL" };      
static void ShowCondition(u32 cond)
{
	printf("Condition: %s[%d]\n", conditions[cond], cond);
}
void ARMXEmitter::ARMABI_ShowConditions()
{
	const u8 *ptr = GetCodePtr();
	FixupBranch cc[15];
	for(u32 a = 0; a < 15; ++a)
		cc[a] = B_CC((CCFlags)a);  

	for(u32 a = 0; a < 15; ++a)
	{
		SetJumpTarget(cc[a]);
		ARMABI_CallFunctionC((void*)&ShowCondition, a);
		if(a != 14)
			B(ptr + ((a + 1) * 4));
	}
}
// NZCVQ is stored in the lower five bits of the Flags variable
// GE values are in the lower four bits of the GEval variable

void ARMXEmitter::UpdateAPSR(bool NZCVQ, u8 Flags, bool GE, u8 GEval)
{
	if(NZCVQ && GE)
	{
		// Can't update GE with the other ones with a immediate
		// Got to use a scratch register
		u32 Imm = (Flags << 27) | ((GEval & 0xF) << 16);
		ARMABI_MOVI2R(R14, IMM(Imm));
		_MSR(true, true, R14);
	}
	else
		if(NZCVQ)
		{
			Operand2 value(Flags << 1, 3);
			_MSR(true, false, value);
		}
		else if(GE)
		{
			Operand2 value(GEval << 2, 9);
			_MSR(false, true, value);
		}
}
