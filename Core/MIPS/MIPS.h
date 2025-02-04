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

#include "../../Globals.h"
#include "../CPU.h"

enum
{
	MIPS_REG_ZERO=0,
	MIPS_REG_COMPILER_SCRATCH=1,

	MIPS_REG_V0=2,
	MIPS_REG_V1=3,

	MIPS_REG_A0=4,
	MIPS_REG_A1=5,
	MIPS_REG_A2=6,
	MIPS_REG_A3=7,
	MIPS_REG_A4=8,	// Seems to be N32 register calling convention - there are 8 args instead of 4.
	MIPS_REG_A5=9,

	MIPS_REG_S0=16,
	MIPS_REG_S1=17,
	MIPS_REG_S2=18,
	MIPS_REG_S3=19,
	MIPS_REG_S4=20,
	MIPS_REG_S5=21,
	MIPS_REG_S6=22,
	MIPS_REG_S7=23,
	MIPS_REG_K0=26,
	MIPS_REG_K1=27,
	MIPS_REG_GP=28,
	MIPS_REG_SP=29,
	MIPS_REG_FP=30,
	MIPS_REG_RA=31,

	// ID for callback is stored here - from JPCSP
	MIPS_REG_CB_ID=MIPS_REG_S0,
};

enum
{
	VFPU_CTRL_SPREFIX,
	VFPU_CTRL_TPREFIX,
	VFPU_CTRL_DPREFIX,
	VFPU_CTRL_CC,
	VFPU_CTRL_INF4,
	VFPU_CTRL_RSV5,
	VFPU_CTRL_RSV6,
	VFPU_CTRL_REV,
	VFPU_CTRL_RCX0,
	VFPU_CTRL_RCX1,
	VFPU_CTRL_RCX2,
	VFPU_CTRL_RCX3,
	VFPU_CTRL_RCX4,
	VFPU_CTRL_RCX5,
	VFPU_CTRL_RCX6,
	VFPU_CTRL_RCX7,

	//unknown....
};

// George Marsaglia-style random number generator.
class GMRng {
public:
	void Init(int seed) {
		m_w = seed ^ (seed << 16);
		if (!m_w) m_w = 1337;
		m_z = ~seed;
		if (!m_z) m_z = 31337;
	}
	u32 R32() {
		m_z = 36969 * (m_z & 65535) + (m_z >> 16);
		m_w = 18000 * (m_w & 65535) + (m_w >> 16);
		return (m_z << 16) + m_w;
	}

private:
	u32 m_w;
	u32 m_z;
};

class MIPSState : public CPU
{
public:
	MIPSState();
	~MIPSState();

	void Reset();

	u32 r[32];
	float f[32];
	float v[128];
	u32 vfpuCtrl[16];
	bool vfpuWriteMask[4];

	u32 pc;
	u32 nextPC;
	u32 hi;
	u32 lo;

	u32 fpcond; //separate for speed - TODO: not worth it

	u32 fcr0;
	u32 fcr31; //fpu control register

	GMRng rng;	// VFPU hardware random number generator. Probably not the right type.

	bool inDelaySlot;

	CPUType cpuType;

	// TODO: How do we handle exceptions?
	u32 exceptions;
	enum { BREAKING_EXCEPTIONS = 1 };

	// Debug stuff
	u32 debugCount;	// can be used to count basic blocks before crashes, etc.

	void WriteFCR(int reg, int value);
	u32 ReadFCR(int reg);
	void SetWriteMask(const bool wm[4]);

	void Irq();
	void SWI();
	void Abort();

	void SingleStep();
	void RunLoopUntil(u64 globalTicks);
};


class MIPSDebugInterface;

//The one we are compiling or running currently
extern MIPSState *currentMIPS;
extern MIPSDebugInterface *currentDebugMIPS;
extern MIPSState mipsr4k;

void MIPS_Init();
int MIPS_SingleStep();

void MIPS_Shutdown();

void MIPS_Irq();
void MIPS_SWI();
