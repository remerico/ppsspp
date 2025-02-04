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

#include "../../Core.h"
#include "../../CoreTiming.h"
#include "../MIPS.h"
#include "../MIPSCodeUtils.h"
#include "../MIPSInt.h"
#include "../MIPSTables.h"

#include "RegCache.h"
#include "Jit.h"


extern u32 *pspmainram;

namespace MIPSComp
{
/*
void Comp_MemRead32KnownRAM(int srcreg, int offset, int reg)
{
	MOV_MemoryToReg(1, reg, ModRM_disp32_EAX+srcreg, (u32)pspmainram - 0x08000000 + offset);
}
void Comp_MemWrite32KnownRAM(int srcreg, int offset, int reg)
{
	MOV_RegToMemory(1, reg, ModRM_disp32_EAX+srcreg, (u32)pspmainram - 0x08000000 + offset);
}

#define RM(reg) (&(currentMIPS->r[reg]))
struct RegInfo
{
	bool cached;
	int x86reg;
};

RegInfo regs[32];

void MovToReg(int reg, u32 value)
{
	if (regs[reg].cached)
	{
		::MOV_ImmToReg(1,regs[reg].x86reg,value,0);
	}
	else
	{
		::MOV_ImmToMemory(1,ModRM_disp32,(u32)(RM(reg)),value);
	}
}
*/

Jit::Jit(MIPSState *mips) : blocks(mips), mips_(mips)
{ 
	blocks.Init();
	asm_.Init(mips, this);
	gpr.SetEmitter(this);
	fpr.SetEmitter(this);
	AllocCodeSpace(1024 * 1024 * 16);
}


void Jit::FlushAll()
{
	gpr.Flush(FLUSH_ALL);
	fpr.Flush(FLUSH_ALL);
}

void Jit::ClearCache()
{
	blocks.Clear();
	ClearCodeSpace();
}

u8 *codeCache;
#define CACHESIZE 16384*1024
void Jit::CompileAt(u32 addr)
{
	u32 op = Memory::Read_Instruction(addr);
	MIPSCompileOp(op);
}

void Jit::Compile(u32 em_address)
{
	if (GetSpaceLeft() < 0x10000 || blocks.IsFull())
	{
		ClearCache();
	}

	int block_num = blocks.AllocateBlock(em_address);
	JitBlock *b = blocks.GetBlock(block_num);
	blocks.FinalizeBlock(block_num, jo.enableBlocklink, DoJit(em_address, b));
}

void Jit::RunLoopUntil(u64 globalticks)
{
	// TODO: copy globalticks somewhere
	((void (*)())asm_.enterCode)();
}

const u8 *Jit::DoJit(u32 em_address, JitBlock *b)
{
	js.cancel = false;
	js.blockStart = js.compilerPC = mips_->pc;
	js.downcountAmount = 0;
	js.curBlock = b;
	js.compiling = true;
	js.inDelaySlot = false;
	
	b->normalEntry = GetCodePtr();

	// TODO: this needs work
	MIPSAnalyst::AnalysisResults analysis; // = MIPSAnalyst::Analyze(em_address);

	gpr.Start(mips_, analysis);
	fpr.Start(mips_, analysis);

	int numInstructions = 0;
	int cycles = 0;
	while (js.compiling)
	{
		u32 inst = Memory::Read_Instruction(js.compilerPC);
		js.downcountAmount += MIPSGetInstructionCycleEstimate(inst);

		MIPSCompileOp(inst);

		js.compilerPC += 4;
		numInstructions++;
	}
	
	b->codeSize = GetCodePtr() - b->normalEntry;
	NOP();
	AlignCode16();
	b->originalSize = numInstructions;
	return b->normalEntry;
}

void Jit::Comp_RunBlock(u32 op)
{
	// This shouldn't be necessary, the dispatcher should catch us before we get here.
	ERROR_LOG(DYNA_REC, "Comp_RunBlock");
}

void Jit::Comp_Generic(u32 op)
{
	FlushAll();
	MIPSInterpretFunc func = MIPSGetInterpretFunc(op);
	if (func)
	{
		//MOV(32, M(&mips_->pc), Imm32(js.compilerPC));
		//ABI_CallFunctionC(func, op);
	}
}

void Jit::WriteExit(u32 destination, int exit_num)
{
	//SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 

	//If nobody has taken care of this yet (this can be removed when all branches are done)
	JitBlock *b = js.curBlock;
	b->exitAddress[exit_num] = destination;
	b->exitPtrs[exit_num] = GetWritableCodePtr();

	// Link opportunity!
	int block = blocks.GetBlockNumberFromStartAddress(destination);
	if (block >= 0 && jo.enableBlocklink) 
	{
		// It exists! Joy of joy!
		//JMP(blocks.GetBlock(block)->checkedEntry, true);
		b->linkStatus[exit_num] = true;
	}
	else 
	{
		//MOV(32, M(&mips_->pc), Imm32(destination));
		//JMP(asm_.dispatcher, true);
	}
}

void Jit::WriteExitDestInEAX() 
{
	/*
	MOV(32, M(&mips_->pc), R(EAX));
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
	JMP(asm_.dispatcher, true);
	*/
}
/*
void Jit::WriteRfiExitDestInEAX() 
{
	MOV(32, M(&mips_->pc), R(EAX));
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
	JMP(asm_routines.testExceptions, true);
}*/

void Jit::WriteSyscallExit()
{
	
	/*
	SUB(32, M(&CoreTiming::downcount), js.downcountAmount > 127 ? Imm32(js.downcountAmount) : Imm8(js.downcountAmount)); 
	JMP(asm_.dispatcher, true);
	*/
}

#define _RS ((op>>21) & 0x1F)
#define _RT ((op>>16) & 0x1F)
#define _RD ((op>>11) & 0x1F)
#define _FS ((op>>11) & 0x1F)
#define _FT ((op>>16) & 0x1F)
#define _FD ((op>>6) & 0x1F)
#define _POS	((op>>6) & 0x1F)
#define _SIZE ((op>>11) & 0x1F)

//memory regions:
//
// 08-0A
// 48-4A
// 04-05
// 44-45
// mov eax, addrreg
	// shr eax, 28
// mov eax, [table+eax]
// mov dreg, [eax+offreg]
	
}
