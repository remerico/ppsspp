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

#include "MIPSTables.h"

#include "MIPS.h"
#include "MIPSDis.h"
#include "MIPSDisVFPU.h"
#include "MIPSInt.h"
#include "MIPSIntVFPU.h"
#include "MIPSCodeUtils.h"

#ifdef ANDROID
#include "ARM/Jit.h"
#else
#include "x86/Jit.h"
#endif
#include "JitCommon/JitCommon.h"

enum MipsEncoding
{
	Imme, 
	Spec, 
	Spe2, 
	Spe3, 
	RegI, 
	Cop0, 
	Cop0CO, 
	Cop1,
	Cop1BC,
	Cop2, 
	Cop2BC2, 
	Cop2Rese,
	VFPU0, 
	VFPU1, 
	VFPU3,
	VFPU4Jump,
	VFPU7,
	VFPU4,
	VFPU5,
	VFPU6,
	VFPUMatrix1,
	VFPU9,
	ALLEGREX0,
	Emu, 
	Rese, 
	NumEncodings 
};

struct MIPSInstruction
{
	int altEncoding;
	const char *name;
	MIPSComp::MIPSCompileFunc compile;
#ifndef FINAL
	MIPSDisFunc disasm;
#endif
	MIPSInterpretFunc interpret;
	//MIPSInstructionInfo information;
	u32 flags;
};

#define INVALID {-2}
#define N(a) a

#ifndef FINAL
#define ENCODING(a) {a}
#define INSTR(name, comp, dis, inter, flags) {-1, N(name), comp, dis, inter, flags}
#else
#define ENCODING(a) {a}
#define INSTR(name, comp, dis, inter, flags) {-1, comp, inter, flags}
#endif


using namespace MIPSDis;
using namespace MIPSInt;
using namespace MIPSComp;
//regregreg instructions
const MIPSInstruction tableImmediate[64] =  //xxxxxx .....
{
	//0
	ENCODING(Spec),
	ENCODING(RegI),
	INSTR("j",    &Jit::Comp_Jump, Dis_JumpType, Int_JumpType, IS_JUMP|IN_IMM26|DELAYSLOT),
	INSTR("jal",  &Jit::Comp_Jump, Dis_JumpType, Int_JumpType, IS_JUMP|IN_IMM26|OUT_RA|DELAYSLOT),
	INSTR("beq",  &Jit::Comp_RelBranch, Dis_RelBranch2, Int_RelBranch, IS_CONDBRANCH|IN_RS|IN_RT|DELAYSLOT),
	INSTR("bne",  &Jit::Comp_RelBranch, Dis_RelBranch2, Int_RelBranch, IS_CONDBRANCH|IN_RS|IN_RT|DELAYSLOT),
	INSTR("blez", &Jit::Comp_RelBranch, Dis_RelBranch,  Int_RelBranch, IS_CONDBRANCH|IN_RS|DELAYSLOT),
	INSTR("bgtz", &Jit::Comp_RelBranch, Dis_RelBranch,  Int_RelBranch, IS_CONDBRANCH|IN_RS|DELAYSLOT),
	//8
	INSTR("addi",  &Jit::Comp_IType, Dis_addi,   Int_IType, IN_RS|IN_IMM16|OUT_RT),
	INSTR("addiu", &Jit::Comp_IType, Dis_addi,   Int_IType, IN_RS|IN_IMM16|OUT_RT),
	INSTR("slti",  &Jit::Comp_IType, Dis_IType,  Int_IType, IN_RS|IN_IMM16|OUT_RT),
	INSTR("sltiu", &Jit::Comp_IType, Dis_IType,  Int_IType, IN_RS|IN_IMM16|OUT_RT),
	INSTR("andi",  &Jit::Comp_IType, Dis_IType,  Int_IType, IN_RS|IN_IMM16|OUT_RT),
	INSTR("ori",   &Jit::Comp_IType, Dis_ori,    Int_IType, IN_RS|IN_IMM16|OUT_RT),
	INSTR("xori",  &Jit::Comp_IType, Dis_IType,  Int_IType, IN_RS|IN_IMM16|OUT_RT),
	INSTR("lui",   &Jit::Comp_IType, Dis_IType1, Int_IType, IN_IMM16|OUT_RT),
	//16
	ENCODING(Cop0), //cop0
	ENCODING(Cop1), //cop1
	ENCODING(Cop2), //cop2
	INVALID, //copU

	INSTR("beql",  &Jit::Comp_RelBranch, Dis_RelBranch2, Int_RelBranch, IS_CONDBRANCH|IN_RS|IN_RT|DELAYSLOT), //L = likely
	INSTR("bnel",  &Jit::Comp_RelBranch, Dis_RelBranch2, Int_RelBranch, IS_CONDBRANCH|IN_RS|IN_RT|DELAYSLOT),
	INSTR("blezl", &Jit::Comp_RelBranch, Dis_RelBranch,  Int_RelBranch, IS_CONDBRANCH|IN_RS|DELAYSLOT),
	INSTR("bgtzl", &Jit::Comp_RelBranch, Dis_RelBranch,  Int_RelBranch, IS_CONDBRANCH|IN_RS|DELAYSLOT),
	//24
	{VFPU0},
	{VFPU1},
	{Emu},
	{VFPU3},
	{Spe2},//special2
	{-2}, //, "jalx", 0, Dis_JumpType, Int_JumpType},
	{-2},
	{Spe3},//special3
	//32
	INSTR("lb",  &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_MEM|IN_IMM16|IN_RS_ADDR|OUT_RT),
	INSTR("lh",  &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_MEM|IN_IMM16|IN_RS_ADDR|OUT_RT),
	INSTR("lwl", &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_MEM|IN_IMM16|IN_RS_ADDR|OUT_RT),
	INSTR("lw",  &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_MEM|IN_IMM16|IN_RS_ADDR|OUT_RT),
	INSTR("lbu", &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_MEM|IN_IMM16|IN_RS_ADDR|OUT_RT),
	INSTR("lhu", &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_MEM|IN_IMM16|IN_RS_ADDR|OUT_RT),
	INSTR("lwr", &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_MEM|IN_IMM16|IN_RS_ADDR|OUT_RT),
	{-2},
	//40
	INSTR("sb",  &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_IMM16|IN_RS_ADDR|IN_RT|OUT_MEM),
	INSTR("sh",  &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_IMM16|IN_RS_ADDR|IN_RT|OUT_MEM),
	INSTR("swl", &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_IMM16|IN_RS_ADDR|IN_RT|OUT_MEM),
	INSTR("sw",  &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_IMM16|IN_RS_ADDR|IN_RT|OUT_MEM),
	{-2},
	{-2},
	INSTR("swr", &Jit::Comp_ITypeMem, Dis_ITypeMem, Int_ITypeMem, IN_IMM16|IN_RS_ADDR|IN_RT|OUT_MEM),
	INSTR("cache", &Jit::Comp_Generic, Dis_Generic, Int_Cache, 0),
	//48
	INSTR("ll", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("lwc1", &Jit::Comp_FPULS, Dis_FPULS, Int_FPULS, IN_RT|IN_RS_ADDR),
	INSTR("lv.s", &Jit::Comp_Generic, Dis_SV, Int_SV, IS_VFPU),
	{-2}, // HIT THIS IN WIPEOUT
	{VFPU4Jump},
	INSTR("lv", &Jit::Comp_Generic, Dis_SVLRQ, Int_SVQ, IS_VFPU),
	INSTR("lv.q", &Jit::Comp_Generic, Dis_SVQ, Int_SVQ, IS_VFPU), //copU
	{VFPU5},
	//56
	INSTR("sc", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("swc1", &Jit::Comp_FPULS, Dis_FPULS, Int_FPULS, 0), //copU
	INSTR("sv.s", &Jit::Comp_Generic, Dis_SV, Int_SV,IS_VFPU),
	{-2}, 
	//60
	{VFPU6},
	INSTR("sv", &Jit::Comp_Generic, Dis_SVLRQ, Int_SVQ, IS_VFPU), //copU
	INSTR("sv.q", &Jit::Comp_Generic, Dis_SVQ, Int_SVQ, IS_VFPU),
	INSTR("vflush", &Jit::Comp_Generic, Dis_Vflush, Int_Vflush, IS_VFPU),
};

const MIPSInstruction tableSpecial[64] = /// 000000 ...... ...... .......... xxxxxx
{
	INSTR("sll",   &Jit::Comp_ShiftType, Dis_ShiftType, Int_ShiftType, OUT_RD|IN_RT|IN_SA),
	{-2},  // copu
	
	INSTR("srl",   &Jit::Comp_ShiftType, Dis_ShiftType, Int_ShiftType, OUT_RD|IN_RT|IN_SA),
	INSTR("sra",   &Jit::Comp_ShiftType, Dis_ShiftType, Int_ShiftType, OUT_RD|IN_RT|IN_SA),
	INSTR("sllv",  &Jit::Comp_ShiftType, Dis_VarShiftType, Int_ShiftType, OUT_RD|IN_RT|IN_RS_SHIFT),
	{-2},
	INSTR("srlv",  &Jit::Comp_ShiftType, Dis_VarShiftType, Int_ShiftType, OUT_RD|IN_RT|IN_RS_SHIFT),
	INSTR("srav",  &Jit::Comp_ShiftType, Dis_VarShiftType, Int_ShiftType, OUT_RD|IN_RT|IN_RS_SHIFT),

	//8
	INSTR("jr",    &Jit::Comp_JumpReg, Dis_JumpRegType, Int_JumpRegType,0),
	INSTR("jalr",  &Jit::Comp_JumpReg, Dis_JumpRegType, Int_JumpRegType,0),
	INSTR("movz",  &Jit::Comp_Generic, Dis_RType3, Int_RType3, OUT_RD|IN_RS|IN_RT),
	INSTR("movn",  &Jit::Comp_Generic, Dis_RType3, Int_RType3, OUT_RD|IN_RS|IN_RT),
	INSTR("syscall", &Jit::Comp_Syscall, Dis_Syscall, Int_Syscall,0),
	INSTR("break", &Jit::Comp_Generic, Dis_Generic, Int_Break, 0),
	{-2},
	INSTR("sync",  &Jit::Comp_Generic, Dis_Generic, Int_Sync, 0),

	//16
	INSTR("mfhi",  &Jit::Comp_Generic, Dis_FromHiloTransfer, Int_MulDivType, OUT_RD|IN_OTHER),
	INSTR("mthi",  &Jit::Comp_Generic, Dis_ToHiloTransfer,   Int_MulDivType, IN_RS|OUT_OTHER),
	INSTR("mflo",  &Jit::Comp_Generic, Dis_FromHiloTransfer, Int_MulDivType, OUT_RD|IN_OTHER),
	INSTR("mtlo",  &Jit::Comp_Generic, Dis_ToHiloTransfer,   Int_MulDivType, IN_RS|OUT_OTHER),
	{-2},
	{-2},
	INSTR("clz",   &Jit::Comp_Generic, Dis_RType2, Int_RType2, OUT_RD|IN_RS|IN_RT),
	INSTR("clo",   &Jit::Comp_Generic, Dis_RType2, Int_RType2, OUT_RD|IN_RS|IN_RT),

	//24
	INSTR("mult",  &Jit::Comp_Generic, Dis_MulDivType, Int_MulDivType, IN_RS|IN_RT|OUT_OTHER),
	INSTR("multu", &Jit::Comp_Generic, Dis_MulDivType, Int_MulDivType, IN_RS|IN_RT|OUT_OTHER),
	INSTR("div",   &Jit::Comp_Generic, Dis_MulDivType, Int_MulDivType, IN_RS|IN_RT|OUT_OTHER),
	INSTR("divu",  &Jit::Comp_Generic, Dis_MulDivType, Int_MulDivType, IN_RS|IN_RT|OUT_OTHER),
	INSTR("madd",  &Jit::Comp_Generic, Dis_MulDivType, Int_MulDivType, IN_RS|IN_RT|OUT_OTHER),
	INSTR("maddu", &Jit::Comp_Generic, Dis_MulDivType, Int_MulDivType, IN_RS|IN_RT|OUT_OTHER),
	{-2},
	{-2},

	//32
	INSTR("add",  &Jit::Comp_RType3, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
	INSTR("addu", &Jit::Comp_RType3, Dis_addu,   Int_RType3,IN_RS|IN_RT|OUT_RD),
	INSTR("sub",  &Jit::Comp_RType3, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
  INSTR("subu", &Jit::Comp_RType3, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
  INSTR("and",  &Jit::Comp_RType3, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
  INSTR("or",   &Jit::Comp_RType3, Dis_addu,   Int_RType3,IN_RS|IN_RT|OUT_RD),
  INSTR("xor",  &Jit::Comp_RType3, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
	INSTR("nor",  &Jit::Comp_RType3, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),

	//40
	{-2},
	{-2},
	INSTR("slt",  &Jit::Comp_Generic, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
	INSTR("sltu", &Jit::Comp_Generic, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
	INSTR("max",  &Jit::Comp_Generic, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
	INSTR("min",  &Jit::Comp_Generic, Dis_RType3, Int_RType3,IN_RS|IN_RT|OUT_RD),
	{-2},
	{-2},

	//48
	INSTR("tge",  &Jit::Comp_Generic, Dis_RType3, 0, 0),
	INSTR("tgeu", &Jit::Comp_Generic, Dis_RType3, 0, 0),
	INSTR("tlt",  &Jit::Comp_Generic, Dis_RType3, 0, 0),
	INSTR("tltu", &Jit::Comp_Generic, Dis_RType3, 0, 0),
	INSTR("teq",  &Jit::Comp_Generic, Dis_RType3, 0, 0),
	{-2},
	INSTR("tne",  &Jit::Comp_Generic, Dis_RType3, 0, 0),
	{-2},

	//56
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
};

const MIPSInstruction tableSpecial2[64] = 
{
	INSTR("add.s",  &Jit::Comp_FPU3op, Dis_FPU3op, Int_FPU3op, 0),
	INSTR("sub.s",  &Jit::Comp_FPU3op, Dis_FPU3op, Int_FPU3op, 0),
	INSTR("mul.s",  &Jit::Comp_FPU3op, Dis_FPU3op, Int_FPU3op, 0),
	INSTR("div.s",  &Jit::Comp_FPU3op, Dis_FPU3op, Int_FPU3op, 0),
	INSTR("sqrt.s", &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
	INSTR("abs.s",  &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
	INSTR("mov.s",  &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
	INSTR("neg.s",  &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
//8
	{-2}, {-2}, {-2}, {-2},
	INSTR("round.w.s",  &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
	INSTR("trunc.w.s",  &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
	INSTR("ceil.w.s",   &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
	INSTR("floor.w.s",  &Jit::Comp_FPU2op, Dis_FPU2op, Int_FPU2op, 0),
//16	
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
//24
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
//32
	INSTR("cvt.s.w", &Jit::Comp_Generic, Dis_FPU2op, Int_FPU2op, 0),
	{-2}, {-2}, {-2}, 
//36
	INSTR("cvt.w.s", &Jit::Comp_Generic, Dis_FPU2op, Int_FPU2op, 0),
	{-2}, 
	INSTR("dis.int", &Jit::Comp_Generic, Dis_Generic, Int_Interrupt, 0), 
	{-2}, 
//40
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
//48
	INSTR("c.f",   &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
	INSTR("c.un",  &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
	INSTR("c.eq",  &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
	INSTR("c.ueq", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.olt", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.ult", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.ole", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.ule", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.sf",  &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.ngle",&Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.seq", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.ngl", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.lt",  &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.nge", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.le",  &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
  INSTR("c.ngt", &Jit::Comp_Generic, Dis_FPUComp, Int_FPUComp, OUT_FPUFLAG),
};


const MIPSInstruction tableSpecial3[64] = 
{
	INSTR("ext", &Jit::Comp_Generic, Dis_Special3, Int_Special3, IN_RS|OUT_RT),
	{-2},
	{-2},
	{-2},
	INSTR("ins", &Jit::Comp_Generic, Dis_Special3, Int_Special3, IN_RS|OUT_RT),
	{-2},
	{-2},
	{-2},
	//8
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
	//16
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
	//32
	{ALLEGREX0}, 
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
	//40
	{ALLEGREX0},
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},

	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},

	{-2}, {-2}, {-2}, 
	INSTR("rdhwr", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2}, {-2}, {-2}, {-2},
};


const MIPSInstruction tableRegImm[32] = 
{
	INSTR("bltz",  &Jit::Comp_RelBranchRI, Dis_RelBranch, Int_RelBranchRI, IS_CONDBRANCH|IN_RS),
	INSTR("bgez",  &Jit::Comp_RelBranchRI, Dis_RelBranch, Int_RelBranchRI, IS_CONDBRANCH|IN_RS),
	INSTR("bltzl", &Jit::Comp_RelBranchRI, Dis_RelBranch, Int_RelBranchRI, IS_CONDBRANCH|IN_RS),
	INSTR("bgezl", &Jit::Comp_RelBranchRI, Dis_RelBranch, Int_RelBranchRI, IS_CONDBRANCH|IN_RS),
	{-2},
	{-2},
	{-2},
	{-2},

	INSTR("tgei",  &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("tgeiu", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("tlti",  &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("tltiu", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("teqi",  &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2},
	INSTR("tnei",  &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2},

  INSTR("bltzal",  &Jit::Comp_Generic, Dis_RelBranch, 0, IS_CONDBRANCH|IN_RS|OUT_RA),  
  INSTR("bgezal",  &Jit::Comp_Generic, Dis_RelBranch, 0, IS_CONDBRANCH|IN_RS|OUT_RA),
  INSTR("bltzall", &Jit::Comp_Generic, Dis_RelBranch, 0, IS_CONDBRANCH|IN_RS|OUT_RA), //L = likely
  INSTR("bgezall", &Jit::Comp_Generic, Dis_RelBranch, 0, IS_CONDBRANCH|IN_RS|OUT_RA),
	{-2},
	{-2},
	{-2},
	{-2},

	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, 
	INSTR("synci", &Jit::Comp_Generic, Dis_Generic, 0, 0),
};

const MIPSInstruction tableCop2[32] = 
{
	INSTR("mfc2", &Jit::Comp_Generic, Dis_Generic, 0, OUT_RT),
	{-2},
	INSTR("cfc2", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("mfv", &Jit::Comp_Generic, Dis_Mftv, Int_Mftv, 0),
	INSTR("mtc2", &Jit::Comp_Generic, Dis_Generic, 0, IN_RT),
	{-2},
	INSTR("ctc2", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("mtv", &Jit::Comp_Generic, Dis_Mftv, Int_Mftv, 0),

	{Cop2BC2},
	INSTR("??", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("??", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("??", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("??", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("??", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("??", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("??", &Jit::Comp_Generic, Dis_Generic, 0, 0),

	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
};

const MIPSInstruction tableCop2BC2[4] = 
{
	INSTR("bvf", &Jit::Comp_VBranch, Dis_VBranch,Int_VBranch,IS_CONDBRANCH),
	INSTR("bvt", &Jit::Comp_VBranch, Dis_VBranch,Int_VBranch,IS_CONDBRANCH),
	INSTR("bvfl", &Jit::Comp_VBranch, Dis_VBranch,Int_VBranch,IS_CONDBRANCH),
	INSTR("bvtl", &Jit::Comp_VBranch, Dis_VBranch,Int_VBranch,IS_CONDBRANCH),
};

const MIPSInstruction tableCop0[32] = 
{
	INSTR("mfc0", &Jit::Comp_Generic, Dis_Generic, 0, OUT_RT),
	{-2}, 
	{-2}, 
	{-2}, 
	INSTR("mtc0", &Jit::Comp_Generic, Dis_Generic, 0, IN_RT),
	{-2}, 
	{-2}, 
	{-2}, 

	{-2}, 
	{-2}, 
	INSTR("rdpgpr", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("mfmc0", &Jit::Comp_Generic, Dis_Generic, 0, 0),

	{-2}, 
	{-2}, 
	INSTR("wrpgpr", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2}, 

	{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},
	{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},{Cop0CO},
};


//we won't encounter these since we only do user mode emulation
MIPSInstruction tableCop0CO[64] = 
{
	{-2}, 
	INSTR("tlbr", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("tlbwi", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2}, 
	{-2}, 
	{-2}, 
	INSTR("tlbwr", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2}, 

	INSTR("tlbp", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, 
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},

	INSTR("eret", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	INSTR("iack", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2}, {-2}, {-2}, {-2}, {-2}, 
	INSTR("deret", &Jit::Comp_Generic, Dis_Generic, 0, 0),

	INSTR("wait", &Jit::Comp_Generic, Dis_Generic, 0, 0),
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},

	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
	{-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2}, {-2},
};


MIPSInstruction tableCop1[32] = 
{
	INSTR("mfc1",&Jit::Comp_mxc1, Dis_mxc1,Int_mxc1, OUT_RT),
	{-2},
	INSTR("cfc1",&Jit::Comp_mxc1, Dis_mxc1,Int_mxc1, 0),
	{-2},
	INSTR("mtc1",&Jit::Comp_mxc1, Dis_mxc1,Int_mxc1, IN_RT),
	{-2},
	INSTR("ctc1",&Jit::Comp_mxc1, Dis_mxc1,Int_mxc1, 0),
	{-2},

	{Cop1BC}, {-2},	{-2},	{-2},	{-2},	{-2},	{-2},	{-2},

	{Spe2},	{-2},	{-2},	{-2},
	{Spe2},	{-2},	{-2},	{-2},

	{-2},{-2},{-2},{-2},{-2},{-2},{-2},{-2},
};

MIPSInstruction tableCop1BC[32] = 
{
	{-1,"bc1f",  &Jit::Comp_FPUBranch, Dis_FPUBranch, Int_FPUBranch,IS_CONDBRANCH|IN_FPUFLAG},
	{-1,"bc1t",  &Jit::Comp_FPUBranch, Dis_FPUBranch, Int_FPUBranch,IS_CONDBRANCH|IN_FPUFLAG},
	{-1,"bc1fl", &Jit::Comp_FPUBranch, Dis_FPUBranch, Int_FPUBranch,IS_CONDBRANCH|IN_FPUFLAG},
	{-1,"bc1tl", &Jit::Comp_FPUBranch, Dis_FPUBranch, Int_FPUBranch,IS_CONDBRANCH|IN_FPUFLAG},
	{-2},{-2},{-2},{-2},
	{-2},{-2},{-2},{-2},{-2},{-2},{-2},{-2},
	{-2},{-2},{-2},{-2},{-2},{-2},{-2},{-2},
	{-2},{-2},{-2},{-2},{-2},{-2},{-2},{-2},
};

MIPSInstruction tableVFPU0[8] = 
{
	INSTR("vadd",&Jit::Comp_Generic, Dis_VectorSet3, Int_VecDo3, IS_VFPU),
	INSTR("vsub",&Jit::Comp_Generic, Dis_VectorSet3, Int_VecDo3, IS_VFPU), 
	INSTR("vsbn",&Jit::Comp_Generic, Dis_VectorSet3, 0, IS_VFPU), 
	{-2}, {-2}, {-2}, {-2}, 
	
	INSTR("vdiv",&Jit::Comp_Generic, Dis_VectorSet3, Int_VecDo3, IS_VFPU),
};

MIPSInstruction tableVFPU1[8] = 
{
	INSTR("vmul",&Jit::Comp_Generic, Dis_VectorSet3, Int_VecDo3, IS_VFPU),
	INSTR("vdot",&Jit::Comp_Generic, Dis_VectorDot, Int_VDot, IS_VFPU), 
	INSTR("vscl",&Jit::Comp_Generic, Dis_VScl, Int_VScl, IS_VFPU),
	INSTR("vhdp",&Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU), 
	{-2}, 
	INSTR("vcrs",&Jit::Comp_Generic, Dis_Vcrs, Int_Vcrs, IS_VFPU), 
	INSTR("vdet",&Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU), 
	{-2},
};

MIPSInstruction tableVFPU3[8] = //011011 xxx
{
	INSTR("vcmp",&Jit::Comp_Generic, Dis_Vcmp, Int_Vcmp, IS_VFPU),
	INSTR("v???",&Jit::Comp_Generic, Dis_Generic, Int_Vcmp, IS_VFPU),   // 0x6de84848 in motogp
	INSTR("vmin",&Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vmax",&Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU), 
	{-2}, 
	INSTR("vscmp",&Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU), 
	INSTR("vsge",&Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU), 
	INSTR("vslt",&Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
};


MIPSInstruction tableVFPU4Jump[32] = //110100 xxxxx
{
	{VFPU4},
	{VFPU7},
	{VFPU9},
	INSTR("vcst", &Jit::Comp_Generic, Dis_Vcst, Int_Vcst, IS_VFPU),
	{-2},{-2},{-2},{-2},

	//8
	{-2},{-2},{-2},{-2},
	{-2},{-2},{-2},{-2},

	//16
	INSTR("vf2in", &Jit::Comp_Generic, Dis_Vf2i, Int_Vf2i, IS_VFPU),
	INSTR("vf2iz", &Jit::Comp_Generic, Dis_Vf2i, Int_Vf2i, IS_VFPU),
	INSTR("vf2iu", &Jit::Comp_Generic, Dis_Vf2i, Int_Vf2i, IS_VFPU),
	INSTR("vf2id", &Jit::Comp_Generic, Dis_Vf2i, Int_Vf2i, IS_VFPU),
	//20
	INSTR("vi2f", &Jit::Comp_Generic, Dis_Vf2i, Int_Vi2f, IS_VFPU),
	INSTR("vcmov", &Jit::Comp_Generic, Dis_Vcmov,Int_Vcmov,IS_VFPU),
	{-2},
	{-2},

	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vwbn.s", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
};

MIPSInstruction tableVFPU7[32] = 
{
	INSTR("vrnds", &Jit::Comp_Generic, Dis_Generic, Int_Vrnds, IS_VFPU),
	INSTR("vrndi", &Jit::Comp_Generic, Dis_Generic, Int_VrndX, IS_VFPU),
	INSTR("vrndf1", &Jit::Comp_Generic, Dis_Generic, Int_VrndX, IS_VFPU),
	INSTR("vrndf2", &Jit::Comp_Generic, Dis_Generic, Int_VrndX, IS_VFPU),
	
	{-2},{-2},{-2},{-2},
	//8
	{-2},{-2},{-2},{-2},
	{-2},{-2},{-2},{-2},
	//16
	{-2},
	{-2},
	INSTR("vf2h", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vh2f", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),

	{-2},
	{-2},
	{-2},
	INSTR("vlgb", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	//24
	{-2},  // Seen in BraveStory, initialization
	{-2},
	INSTR("vus2i", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vs2i", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),

	INSTR("vi2uc", &Jit::Comp_Generic, Dis_Vi2x, Int_Vi2x, IS_VFPU),
	INSTR("vi2c",  &Jit::Comp_Generic, Dis_Vi2x, Int_Vi2x, IS_VFPU),
	INSTR("vi2us", &Jit::Comp_Generic, Dis_Vi2x, Int_Vi2x, IS_VFPU),
	INSTR("vi2s",  &Jit::Comp_Generic, Dis_Vi2x, Int_Vi2x, IS_VFPU),
};

// 110100 00000 10100 0000000000000000
// 110100 00000 10111 0000000000000000
MIPSInstruction tableVFPU4[32] =  //110100 00000 xxxxx
{
	INSTR("vmov", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op,IS_VFPU), 
	INSTR("vabs", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op,IS_VFPU), 
	INSTR("vneg", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op,IS_VFPU), 
	INSTR("vidt", &Jit::Comp_Generic, Dis_VectorSet1, Int_Vidt,IS_VFPU), 
	INSTR("vsat0", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vsat1", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vzero", &Jit::Comp_Generic, Dis_VectorSet1, Int_VVectorInit, IS_VFPU),
	INSTR("vone",  &Jit::Comp_Generic, Dis_VectorSet1, Int_VVectorInit, IS_VFPU),
//8
	{-2},{-2},{-2},{-2},{-2},{-2},{-2},{-2},
//16
	INSTR("vrcp", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vrsq", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vsin", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vcos", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vexp2", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vlog2", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vsqrt", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	INSTR("vasin", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
//24
	INSTR("vnrcp", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op,IS_VFPU),
	{-2},
	INSTR("vnsin", &Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op,IS_VFPU), 
	{-2},
	INSTR("vrexp2",&Jit::Comp_Generic, Dis_VectorSet2, Int_VV2Op, IS_VFPU),
	{-2},{-2},{-2},
//32
};

MIPSInstruction tableVFPU5[8] =  //110111 xxx
{
	INSTR("vpfxs",&Jit::Comp_Generic, Dis_VPFXST, Int_VPFX, IS_VFPU),
	INSTR("vpfxs",&Jit::Comp_Generic, Dis_VPFXST, Int_VPFX, IS_VFPU),
	INSTR("vpfxt",&Jit::Comp_Generic, Dis_VPFXST, Int_VPFX, IS_VFPU),
	INSTR("vpfxt",&Jit::Comp_Generic, Dis_VPFXST, Int_VPFX, IS_VFPU),
	INSTR("vpfxd", &Jit::Comp_Generic, Dis_VPFXD, Int_VPFX, IS_VFPU),
	INSTR("vpfxd", &Jit::Comp_Generic, Dis_VPFXD, Int_VPFX, IS_VFPU),
	INSTR("viim.s",&Jit::Comp_Generic,Dis_Viim,Int_Viim, IS_VFPU),
	INSTR("vfim.s",&Jit::Comp_Generic,Dis_Viim,Int_Viim, IS_VFPU),
};

MIPSInstruction tableVFPU6[32] =  //111100 xxx
{
//0
	INSTR("vmmul",&Jit::Comp_Generic, Dis_MatrixMult, Int_Vmmul, IS_VFPU),
	INSTR("vmmul",&Jit::Comp_Generic, Dis_MatrixMult, Int_Vmmul, IS_VFPU),
	INSTR("vmmul",&Jit::Comp_Generic, Dis_MatrixMult, Int_Vmmul, IS_VFPU),
	INSTR("vmmul",&Jit::Comp_Generic, Dis_MatrixMult, Int_Vmmul, IS_VFPU),

	INSTR("v(h)tfm2",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm2",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm2",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm2",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
//8
	INSTR("v(h)tfm3",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm3",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm3",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm3",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),

	INSTR("v(h)tfm4",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm4",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm4",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	INSTR("v(h)tfm4",&Jit::Comp_Generic, Dis_Vtfm, Int_Vtfm, IS_VFPU),
	//16
	INSTR("vmscl",&Jit::Comp_Generic, Dis_Generic, Int_Vmscl, IS_VFPU),
	INSTR("vmscl",&Jit::Comp_Generic, Dis_Generic, Int_Vmscl, IS_VFPU),
	INSTR("vmscl",&Jit::Comp_Generic, Dis_Generic, Int_Vmscl, IS_VFPU),
	INSTR("vmscl",&Jit::Comp_Generic, Dis_Generic, Int_Vmscl, IS_VFPU),

	INSTR("vcrsp/vqm",&Jit::Comp_Generic, Dis_CrossQuat, Int_CrossQuat, IS_VFPU),
	INSTR("vcrsp/vqm",&Jit::Comp_Generic, Dis_CrossQuat, Int_CrossQuat, IS_VFPU),
	INSTR("vcrsp/vqm",&Jit::Comp_Generic, Dis_CrossQuat, Int_CrossQuat, IS_VFPU),
	INSTR("vcrsp/vqm",&Jit::Comp_Generic, Dis_CrossQuat, Int_CrossQuat, IS_VFPU),
//24
	{-2},
	{-2},
	{-2},
	{-2},

	{VFPUMatrix1},
	INSTR("vrot",&Jit::Comp_Generic, Dis_VRot, Int_Vrot, IS_VFPU),
	{-2},
	{-2},
};

MIPSInstruction tableVFPUMatrixSet1[16] = //111100 11100 0xxxx   (rm x is 16)
{
	INSTR("vmmov",&Jit::Comp_Generic, Dis_MatrixSet2, Int_Vmmov, IS_VFPU),
	{-2},
	{-2},
	INSTR("vmidt",&Jit::Comp_Generic, Dis_MatrixSet1, Int_VMatrixInit, IS_VFPU),

	{-2},
	{-2},
	INSTR("vmzero", &Jit::Comp_Generic, Dis_MatrixSet1, Int_VMatrixInit, IS_VFPU),
	INSTR("vmone",  &Jit::Comp_Generic, Dis_MatrixSet1, Int_VMatrixInit, IS_VFPU),

	{-2},{-2},{-2},{-2},
  {-2},{-2},{-2},{-2},
};

MIPSInstruction tableVFPU9[32] = //110100 00010 xxxxx
{
	INSTR("vsrt1", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vsrt2", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vbfy1", &Jit::Comp_Generic, Dis_Vbfy, Int_Vbfy, IS_VFPU),
	INSTR("vbfy2", &Jit::Comp_Generic, Dis_Vbfy, Int_Vbfy, IS_VFPU),
	//4
	INSTR("vocp", &Jit::Comp_Generic, Dis_Vbfy, Int_Vocp, IS_VFPU),  // one's complement
	INSTR("vsocp", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vfad", &Jit::Comp_Generic, Dis_Vfad, Int_Vfad, IS_VFPU),
	INSTR("vavg", &Jit::Comp_Generic, Dis_Vfad, Int_Vavg, IS_VFPU),
	//8
	INSTR("vsrt3", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vsrt4", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vsgn", &Jit::Comp_Generic, Dis_Vbfy, Int_Vsgn, IS_VFPU),
	{-2},
	//12
	{-2},
	{-2},
	{-2},
	{-2},

	//16
	INSTR("vmfvc", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	INSTR("vmtvc", &Jit::Comp_Generic, Dis_Generic, 0, IS_VFPU),
	{-2},
	{-2},

	//20
	{-2},{-2},{-2},{-2},
	//24
	{-2},
	INSTR("vt4444", &Jit::Comp_Generic, Dis_Generic, Int_ColorConv, IS_VFPU),
	INSTR("vt5551", &Jit::Comp_Generic, Dis_Generic, Int_ColorConv, IS_VFPU),
	INSTR("vt5650", &Jit::Comp_Generic, Dis_Generic, Int_ColorConv, IS_VFPU),
	
	//28
	{-2},{-2},{-2},{-2},
};

MIPSInstruction tableALLEGREX0[32] =  //111111
{
	{-2},
	{-2},
	INSTR("wsbh",&Jit::Comp_Generic, Dis_Allegrex2,Int_Allegrex2,0),
	INSTR("wsbw",&Jit::Comp_Generic, Dis_Allegrex2,Int_Allegrex2,0),
	{-2},	{-2},	{-2},	{-2},
//8
	{-2},	{-2},	{-2},	{-2},	{-2},	{-2},	{-2},	{-2},
//16
	INSTR("seb", &Jit::Comp_Allegrex, Dis_Allegrex,Int_Allegrex, IN_RT|OUT_RD),
	{-2},
	{-2},
	{-2},
//20
	INSTR("bitrev",&Jit::Comp_Generic, Dis_Allegrex,Int_Allegrex, IN_RT|OUT_RD),
	{-2},
	{-2},
	{-2},
//24
	INSTR("seh", &Jit::Comp_Allegrex, Dis_Allegrex,Int_Allegrex, IN_RT|OUT_RD),
	{-2},
	{-2},
	{-2},
//28
	{-2},
	{-2},
	{-2},
	{-2},
};


MIPSInstruction tableEMU[4] = 
{
	INSTR("RUNBLOCK",&Jit::Comp_RunBlock,Dis_Emuhack,Int_Emuhack, 0xFFFFFFFF),
	INSTR("RetKrnl", 0,Dis_Emuhack,Int_Emuhack, 0),
	{-2},
	{-2},
};

const int encodingBits[NumEncodings][2] =
{
  {26, 6}, //IMME
  {0,  6}, //Special
  {0,  6}, //special2
	{0,  6}, //special3
	{16, 5}, //RegImm
	{21, 5}, //Cop0
	{0,  6}, //Cop0CO
	{21, 5}, //Cop1
	{16, 5}, //Cop1BC
	{21, 5}, //Cop2
	{16, 2}, //Cop2BC2
	{0,  0}, //Cop2Rese
	{23, 3}, //VFPU0
	{23, 3}, //VFPU1
	{23, 1}, //VFPU3
	{21, 5}, //VFPU4Jump
	{16, 5}, //VFPU7
	{16, 5}, //VFPU4
	{23, 3}, //VFPU5
	{21, 5}, //VFPU6
	{16, 3}, //VFPUMatrix1
	{16, 5}, //VFPU9
	{6,  5}, //ALLEGREX0
	{24, 2}, //EMUHACK
};		


const MIPSInstruction *mipsTables[NumEncodings] =
{
	tableImmediate,
	tableSpecial,
	tableSpecial2,
	tableSpecial3,
	tableRegImm,
	tableCop0,
	tableCop0CO,
	tableCop1,
	tableCop1BC,
	tableCop2,
	tableCop2BC2,
	0,
	tableVFPU0, //vfpu0
	tableVFPU1, //vfpu1
	tableVFPU3, //vfpu3
	tableVFPU4Jump,
	tableVFPU7, //vfpu4     110100 00001
	tableVFPU4, //vfpu4     110100 00000
	tableVFPU5, //vfpu5     110111
	tableVFPU6, //vfpu6     111100
	tableVFPUMatrixSet1,
	tableVFPU9,
	tableALLEGREX0,
	tableEMU
};



//arm encoding table
//const MIPSInstruction mipsinstructions[] = 
//{
//{Comp_Unimpl,Dis_Unimpl, Info_NN,    0, 0x601,       0x1FE,0}, //could be used for drec hook :) bits 5-24 plus 0-3 are available, 19 bits are more than enough
// DATA PROCESSING INSTRUCTIONS
//                        S
//	{Comp_AND,   Dis_AND,    Info_DP,    0, DATAP(0, 0), 0x20F, {0}},
//};


//Todo : generate dispatcher functions from above tables
//instead of this horribly slow abomination

const MIPSInstruction *MIPSGetInstruction(u32 op)
{
	MipsEncoding encoding = Imme;
	const MIPSInstruction *instr = &tableImmediate[op>>26];
	while (instr->altEncoding != -1) 
	{
		const MIPSInstruction *table = mipsTables[encoding];
		int mask = ((1<<encodingBits[encoding][1])-1);
		int shift = encodingBits[encoding][0];
		int subop = (op >> shift) & mask;
		instr = &table[subop];
		if (encoding == Rese)
			return 0; //invalid instruction
//		if (encoding == Spe3)
//			__asm int 3
		if (!instr)
			return 0;
		if (instr->altEncoding == -2)
		{
			//BAD!!
			return 0; //invalid instruction
		}
		encoding = (MipsEncoding)instr->altEncoding;
	} 
	//alright, we have a valid MIPS instruction!
	return instr;
}



void MIPSCompileOp(u32 op)
{
	if (op==0)
		return;
	const MIPSInstruction *instr = MIPSGetInstruction(op);
	if (instr)
	{
		if (instr->compile)
			(MIPSComp::jit->*(instr->compile))(op);   // woohoo, member functions pointers!
		else 
		{
			ERROR_LOG(CPU,"MIPSCompileOp %08x failed",op);
			//MessageBox(0,"ARGH2",0,0);//compile an interpreter call
		}
	}
	else
	{
		ERROR_LOG(CPU, "MIPSCompileOp: Invalid instruction %08x", op);
	}
}


void MIPSDisAsm(u32 op, u32 pc, char *out)
{
	if (op == 0)
	{
		//ANDEQ R0,R0,R0 is probably not used for legitimate purposes :P
		sprintf(out,"---\t---");
	}
	else
	{
		disPC = pc;
		const MIPSInstruction *instr = MIPSGetInstruction(op);
		if (instr && instr->disasm)
			instr->disasm(op, out);
		else
		{
			strcpy(out, "no instruction :(");
			//__asm int 3
			MIPSGetInstruction(op);
		}
	}
}


void MIPSInterpret(u32 op) //only for those rare ones
{
	//if ((op&0xFFFFF000) == 0xd0110000)
	//	Crash();
	//if (atable[CRUNCH_MIPS_OP(op)].interpret)
	//		atable[CRUNCH_MIPS_OP(op)].interpret(op);
	//	else
	//		_dbg_assert_msg_(MIPS,0,"Trying to interpret instruction that can't be interpreted");
	const MIPSInstruction *instr = MIPSGetInstruction(op);
	if (instr && instr->interpret)
		instr->interpret(op);
	else
  {
    ERROR_LOG(CPU,"Unknown instruction %08x", op);
    // Try to disassemble it
    char disasm[256];
    MIPSDisAsm(op, currentMIPS->pc, disasm);
		_dbg_assert_msg_(CPU, 0, disasm);
    currentMIPS->pc += 4;
  }
}

#define _RS   ((op>>21) & 0x1F)
#define _RT   ((op>>16) & 0x1F)
#define _RD   ((op>>11) & 0x1F)
#define R(i)   (currentMIPS->r[i])

void MIPSInterpret_Fast(u32 op)
{
	switch (op >> 29)
	{
	case 0x1:
		{
			s32 simm = (s32)(s16)(op & 0xFFFF);
			u32 uimm = (u32)(u16)(op & 0xFFFF);
			u32 suimm = (u32)simm;

			int rt = _RT;
			int rs = _RS;

			if (rt == 0) //destination register is zero register
				return; //nop

			switch (op>>26) 
			{
			case 8:  R(rt) = R(rs) + simm; break; //addi
			case 9:  R(rt) = R(rs) + simm; break;  //addiu
			case 10: R(rt) = (s32)R(rs) < simm; break; //slti
			case 11: R(rt) = R(rs) < suimm; break; //sltiu
			case 12: R(rt) = R(rs) & uimm; break; //andi
			case 13: R(rt) = R(rs) | uimm; break; //ori
			case 14: R(rt) = R(rs) ^ uimm; break; //xori
			case 15: R(rt) = uimm << 16;   break; //lui
			default:
				break;
			}
			currentMIPS->pc += 4;
		}
		break;

	default:
		MIPSInterpret(op);
	}
}


const char *MIPSGetName(u32 op)
{
	static const char *noname = "unk";
	const MIPSInstruction *instr = MIPSGetInstruction(op);
	if (!instr)
		return noname;
	else
		return instr->name;
}

u32 MIPSGetInfo(u32 op)
{
	//	int crunch = CRUNCH_MIPS_OP(op);
	const MIPSInstruction *instr = MIPSGetInstruction(op);
	if (instr)
		return instr->flags;
	else
		return 0;
}

MIPSInterpretFunc MIPSGetInterpretFunc(u32 op)
{
	const MIPSInstruction *instr = MIPSGetInstruction(op);
	if (instr->interpret)
		return instr->interpret;
	else
		return 0;
}

// TODO: Do something that makes sense here.
int MIPSGetInstructionCycleEstimate(u32 op)
{
  u32 info = MIPSGetInfo(op);
  if (info & DELAYSLOT)
    return 2;
  else
    return 1;
}
