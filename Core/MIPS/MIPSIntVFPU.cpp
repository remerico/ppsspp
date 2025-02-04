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

#include "../Core.h"

#include <cmath>

#include "MIPS.h"
#include "MIPSInt.h"
#include "MIPSTables.h"
#include "MIPSVFPUUtils.h"

#include <limits>

#define R(i)   (currentMIPS->r[i])
#define RF(i)  (*(float*)(&(currentMIPS->r[i])))
#define V(i)   (currentMIPS->v[i])
#define VI(i)   (*(u32*)(&(currentMIPS->v[i])))
#define FI(i)  (*(u32*)(&(currentMIPS->f[i])))
#define FsI(i) (*(s32*)(&(currentMIPS->f[i])))
#define PC     (currentMIPS->pc)

#define _RS   ((op >> 21) & 0x1F)
#define _RT   ((op >> 16) & 0x1F)
#define _RD   ((op>>11) & 0x1F)
#define _FS   ((op>>11) & 0x1F)
#define _FT   ((op >> 16) & 0x1F)
#define _FD   ((op>>6 ) & 0x1F)
#define _POS  ((op>>6 ) & 0x1F)
#define _SIZE ((op>>11 ) & 0x1F)

#define HI currentMIPS->hi
#define LO currentMIPS->lo

#ifndef M_LOG2E
#define M_E        2.71828182845904523536
#define M_LOG2E    1.44269504088896340736
#define M_LOG10E   0.434294481903251827651
#define M_LN2      0.693147180559945309417
#define M_LN10     2.30258509299404568402
#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923
#define M_PI_4     0.785398163397448309616
#define M_1_PI     0.318309886183790671538
#define M_2_PI     0.636619772367581343076
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2    1.41421356237309504880
#define M_SQRT1_2  0.707106781186547524401
#endif

void ApplyPrefixST(float *v, u32 data, VectorSize size)
{
  // Possible optimization shortcut:
  // if (data == 0xe4)
  //   return;

	int n = GetNumVectorElements(size);
	float origV[4];
	static const float constantArray[8] = {0.f, 1.f, 2.f, 0.5f, 3.f, 1.f/3.f, 0.25f, 1.f/6.f};

	for (int i = 0; i < n; i++)
	{
		origV[i] = v[i];
	}

	for (int i = 0; i < n; i++)
	{
		int regnum = (data >> (i*2)) & 3;
		int abs    = (data >> (8+i)) & 1;
		int negate = (data >> (16+i)) & 1;
		int constants = (data >> (12+i)) & 1;

		if (!constants)
		{
			v[i] = origV[regnum];
			if (abs)
				v[i] = fabs(v[i]);
		}
		else
		{
			v[i] = constantArray[regnum + (abs<<2)];
		}

		if (negate)
			v[i] = -v[i];
	}
}

void ApplySwizzleS(float *v, VectorSize size)
{
	ApplyPrefixST(v, currentMIPS->vfpuCtrl[VFPU_CTRL_SPREFIX], size);
}

void ApplySwizzleT(float *v, VectorSize size)
{
	ApplyPrefixST(v, currentMIPS->vfpuCtrl[VFPU_CTRL_TPREFIX], size);
}

void ApplyPrefixD(float *v, VectorSize size)
{
	int n = GetNumVectorElements(size);
	bool writeMask[4];
	u32 data = currentMIPS->vfpuCtrl[VFPU_CTRL_DPREFIX];
	for (int i = 0; i < n; i++)
	{
		int sat = (data >> i*2) & 3;
		int mask = (data >> (8+i)) & 1;
		writeMask[i] = mask ? true : false;
		if (sat == 1)
		{
			if (v[i] > 1.0f) v[i]=1.0f;
			if (v[i] < 0.0f) v[i]=0.0f;
		}
		else if (sat == 3)
		{
			if (v[i] > 1.0f)  v[i]=1.0f;
			if (v[i] < -1.0f) v[i]=-1.0f;
		}
	}
  currentMIPS->SetWriteMask(writeMask);
}

void EatPrefixes()
{
	currentMIPS->vfpuCtrl[VFPU_CTRL_SPREFIX] = 0xe4; //passthru
	currentMIPS->vfpuCtrl[VFPU_CTRL_TPREFIX] = 0xe4; //passthru
	currentMIPS->vfpuCtrl[VFPU_CTRL_DPREFIX] = 0;
  static const bool noWriteMask[4] = {false, false, false, false};
	currentMIPS->SetWriteMask(noWriteMask);
}

namespace MIPSInt
{
#define S_not(a,b,c) (a<<2) | (b) | (c << 5)
#define SgetA(v) (((v)>>2)&0x7)
#define SgetB(v) ((v)&3)
#define SgetC(v) (((v)>>5)&0x3)
#define VS(m,row,col) V(m*4+(row)+(col)*32)

	void Int_VPFX(u32 op)
	{
		int data = op & 0xFFFFF;
		int regnum = (op >> 24) & 3;
		currentMIPS->vfpuCtrl[VFPU_CTRL_SPREFIX + regnum] = data;
		PC += 4;
	}

	void Int_SVQ(u32 op)
	{
		int imm = (signed short)(op&0xFFFC);
		int rs = _RS;
		int vt = (((op >> 16) & 0x1f)) | ((op&1) << 5);

		u32 addr = R(rs) + imm;

		switch (op >> 26)
		{
		case 53: //lvl.q/lvr.q
      if (addr & 0x3)
      {
        _dbg_assert_msg_(CPU, 0, "Misaligned lvX.q");
      }
      if ((op&2) == 0)
      {
        // It's an LVL
        float d[4];
        ReadVector(d, V_Quad, vt);
        int offset = (addr >> 2) & 3;
        for (int i = 0; i < offset + 1; i++)
        {
          d[3 - i] = Memory::Read_Float(addr - i * 4);
        }
        WriteVector(d, V_Quad, vt);
      }
      else
      {
        // It's an LVR
        float d[4];
        ReadVector(d, V_Quad, vt);
        int offset = (addr >> 2) & 3;
        for (int i = 0; i < (3 - offset) + 1; i++)
        {
          d[i] = Memory::Read_Float(addr + 4 * i);
        }
        WriteVector(d, V_Quad, vt);
      }
      break;

		case 54: //lv.q
      if (addr & 0xF)
      {
        _dbg_assert_msg_(CPU, 0, "Misaligned lv.q");
      }
			WriteVector((const float*)Memory::GetPointer(addr), V_Quad, vt);
			break;

		case 61: // svl.q/svr.q
      if (addr & 0x3)
      {
        _dbg_assert_msg_(CPU, 0, "Misaligned svX.q");
      }
      if ((op&2) == 0)
      {
        // It's an SVL
        float d[4];
        ReadVector(d, V_Quad, vt);
        int offset = (addr >> 2) & 3;
        for (int i = 0; i < offset + 1; i++)
        {
          Memory::Write_Float(d[3 - i], addr - i * 4);
        }
      }
      else
      {
        // It's an SVR
        float d[4];
        ReadVector(d, V_Quad, vt);
        int offset = (addr >> 2) & 3;
        for (int i = 0; i < (3 - offset) + 1; i++)
        {
          Memory::Write_Float(d[i], addr + 4 * i);
        }
      }
      break;

		case 62: //sv.q
      if (addr & 0xF)
      {
        _dbg_assert_msg_(CPU, 0, "Misaligned sv.q");
      }
			ReadVector((float*)Memory::GetPointer(addr), V_Quad, vt);
			break;

		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret VQ instruction that can't be interpreted");
			break;
		}
		PC += 4;
	}

	void Int_VMatrixInit(u32 op)
	{
		static const float idt[16] = 
		{
			1,0,0,0,
			0,1,0,0,
			0,0,1,0,
			0,0,0,1,
		};
		static const float zero[16] = 
		{
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
			0,0,0,0,
		};
		static const float one[16] = 
		{
			1,1,1,1,
			1,1,1,1,
			1,1,1,1,
			1,1,1,1,
		};
		int vd = _VD;
		MatrixSize sz = GetMtxSize(op);
		const float *m;

		switch ((op >> 16) & 0xF)
		{
		case 3: m=idt; break; //identity
		case 6: m=zero; break;
		case 7: m=one; break;
		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
			return;
		}

		WriteMatrix(m, sz, vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_VVectorInit(u32 op)
	{
		int vd = _VD;
		VectorSize sz = GetVecSize(op);
		int n = GetNumVectorElements(sz);
		const float ones[4] = {1,1,1,1};
		const float zeros[4] = {0,0,0,0};
		const float *v;
		switch ((op >> 16) & 0xF)
		{
		case 6: v=zeros; break;
		case 7: v=ones; break;
		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
			return;
		}
		float o[4];
		for (int i = 0; i < n; i++)
			o[i] = v[i];
		ApplyPrefixD(o, sz);
		WriteVector(o, sz, vd);

		EatPrefixes();
		PC += 4;
	}

	void Int_Viim(u32 op)
	{
		int vt = _VT;
		int imm = op&0xFFFF;
		//V(vt) = (float)imm;
		int type = (op >> 23) & 7;
		if (type == 6)
			V(vt) = (float)imm;
		else if (type == 7)
			V(vt) = Float16ToFloat32((u16)imm);
		else
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
		
		PC += 4;
		EatPrefixes();
	}

	void Int_Vidt(u32 op)
	{
		int vd = _VD;
		VectorSize sz = GetVecSize(op);
		float f[4];
		switch (sz)
		{
		case V_Pair:
			f[0] = (vd&1)==0 ? 1.0f : 0.0f;
			f[1] = (vd&1)==1 ? 1.0f : 0.0f;
			break;
		case V_Quad:
			f[0] = (vd&3)==0 ? 1.0f : 0.0f;
			f[1] = (vd&3)==1 ? 1.0f : 0.0f;
			f[2] = (vd&3)==2 ? 1.0f : 0.0f;
			f[3] = (vd&3)==3 ? 1.0f : 0.0f;
			break;
		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
			break;
		}
		WriteVector(f, sz, vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_Vmmul(u32 op)
	{
		float s[16];
		float t[16];
		float d[16];

		int vd = _VD;
		int vs = _VS;
		int vt = _VT;
		MatrixSize sz = GetMtxSize(op);
		int n = GetMatrixSide(sz);

		ReadMatrix(s, sz, vs);
		ReadMatrix(t, sz, vt);

		for (int a = 0; a < n; a++)
		{
			for (int b = 0; b < n; b++)
			{
				float sum = 0.0f;
				for (int c = 0; c < n; c++)
				{
					sum += s[b*4 + c] * t[a*4 + c];
				}
				d[a*4 + b]=sum;
			}
		}

		WriteMatrix(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

  void Int_Vmscl(u32 op)
  {
    float s[16];
    float t[16];
    float d[16];

    int vd = _VD;
    int vs = _VS;
    int vt = _VT;
    MatrixSize sz = GetMtxSize(op);
    int n = GetMatrixSide(sz);

    ReadMatrix(s, sz, vs);
    ReadMatrix(t, sz, vt);

    for (int a = 0; a < n; a++)
    {
      for (int b = 0; b < n; b++)
      {
        d[a*4 + b] = s[a*4 + b] * t[0];
      }
    }

    WriteMatrix(d, sz, vd);
    PC += 4;
    EatPrefixes();
  }

	void Int_Vmmov(u32 op)
	{
		float s[16];
		int vd = _VD;
		int vs = _VS;
		MatrixSize sz = GetMtxSize(op);
		ReadMatrix(s, sz, vs);
		WriteMatrix(s,sz,vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_Vflush(u32 op)
	{
		DEBUG_LOG(CPU,"vflush");
		PC += 4;
	}

	void Int_VV2Op(u32 op)
	{
		float s[4], d[4];
		int vd = _VD;
		int vs = _VS;
		VectorSize sz = GetVecSize(op);
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		for (int i = 0; i < GetNumVectorElements(sz); i++)
		{
			switch ((op >> 16) & 0x1f)
			{
			case 0: d[i] = s[i]; break; //vmov
			case 1: d[i] = fabsf(s[i]); break; //vabs
			case 2: d[i] = -s[i]; break; //vneg
			case 4: if (s[i] < 0) d[i] = 0; else {if(s[i] > 1.0f) d[i] = 1.0f; else d[i] = s[i];} break;    // vsat0
			case 5: if (s[i] < -1.0f) d[i] = -1.0f; else {if(s[i] > 1.0f) d[i] = 1.0f; else d[i] = s[i];} break;  // vsat1
			case 16: d[i] = 1.0f / s[i]; break; //vrcp
			case 17: d[i] = 1.0f / sqrtf(s[i]); break; //vrsq
			case 18: d[i] = sinf((float)M_PI_2 * s[i]); break; //vsin
			case 19: d[i] = cosf((float)M_PI_2 * s[i]); break; //vcos
			case 20: d[i] = powf(2.0f, s[i]); break;
			case 21: d[i] = logf(s[i])/log(2.0f); break;
      case 22: d[i] = sqrtf(s[i]); break; //vsqrt
			case 23: d[i] = asinf(s[i] * (float)M_2_PI); break; //vasin

			default:
				_dbg_assert_msg_(CPU,0,"Trying to interpret VV2Op instruction that can't be interpreted");
				break;
			}
		}
		ApplyPrefixD(d, sz);
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

  void Int_Vocp(u32 op)
  {
    float s[4], d[4];
    int vd = _VD;
    int vs = _VS;
    VectorSize sz = GetVecSize(op);
    ReadVector(s, sz, vs);
    ApplySwizzleS(s, sz);
    for (int i = 0; i < GetNumVectorElements(sz); i++)
    {
      d[i] = 1.0f - s[i]; //vocp
    }
    ApplyPrefixD(d, sz);
    WriteVector(d, sz, vd);
    PC += 4;
    EatPrefixes();
  }

  void Int_Vsgn(u32 op)
  {
    float s[4], d[4];
    int vd = _VD;
    int vs = _VS;
    VectorSize sz = GetVecSize(op);
    ReadVector(s, sz, vs);
    ApplySwizzleS(s, sz);
    for (int i = 0; i < GetNumVectorElements(sz); i++)
    {
      d[i] = s[i] > 0.0f ? 1.0f : (s[i] < 0.0f ? -1.0f : 0.0f);
    }
    ApplyPrefixD(d, sz);
    WriteVector(d, sz, vd);
    PC += 4;
    EatPrefixes();
  }

	void Int_Vf2i(u32 op)
	{
		float s[4];
		int d[4];
		int vd = _VD;
		int vs = _VS;
		int imm = (op >> 16) & 0x1f;
		float mult = (float)(1 << imm);
		VectorSize sz = GetVecSize(op);
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz); //TODO: and the mask to kill everything but swizzle
		for (int i = 0; i < GetNumVectorElements(sz); i++)
		{
			switch ((op >> 21) & 0x1f)
			{
			case 16: d[i] = (int)floor(s[i] * mult + 0.5f); break; //n
			case 17: d[i] = s[i]>=0?(int)floor(s[i] * mult) : (int)ceil(s[i] * mult); break; //z
			case 18: d[i] = (int)ceil(s[i] * mult); break; //u
			case 19: d[i] = (int)floor(s[i] * mult); break; //d
			}
		}
		ApplyPrefixD((float*)d, sz); //TODO: and the mask to kill everything but mask
		WriteVector((float*)d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_Vi2f(u32 op)
	{
		int s[4];
		float d[4];
		int vd = _VD;
		int vs = _VS;
		int imm = (op >> 16) & 0x1f;
		float mult = 1.0f/(float)(1 << imm);
		VectorSize sz = GetVecSize(op);
		ReadVector((float*)&s, sz, vs);
		ApplySwizzleS((float*)&s, sz); //TODO: and the mask to kill everything but swizzle
		for (int i = 0; i < GetNumVectorElements(sz); i++)
		{
			d[i] = (float)s[i] * mult;
		}
		ApplyPrefixD(d, sz); //TODO: and the mask to kill everything but mask
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}
	void Int_Vi2x(u32 op)
	{
		int s[4];
    u32 d[2] = {0};
		int vd = _VD;
		int vs = _VS;
		VectorSize sz = GetVecSize(op);
		VectorSize oz;
		ReadVector((float*)s, sz, vs);
		ApplySwizzleS((float*)s, sz); //TODO: and the mask to kill everything but swizzle
		switch ((op >> 16)&3)
		{
		case 0: //vi2uc
			{
				for (int i = 0; i < 4; i++)
				{
					int v = s[i];
					if (v < 0) v = 0;
					v >>= 23;
					d[0] |= ((u32)v & 0xFF) << (i * 8);
				}
				oz = V_Single;
			}
			break;

		case 1: //vi2c
      {
        for (int i = 0; i < 4; i++)
        {
          int v = s[i];
          v >>= 24;
          d[0] |= ((u32)v & 0xFF) << (i * 8);
        }
        oz = V_Single;
      }
      break;

    case 2:  //vi2us
			//break;
		case 3:  //vi2s
			//break;
		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
			break;
		}
		ApplyPrefixD((float*)d,oz);
		WriteVector((float*)d,oz,vd);
		PC += 4;
		EatPrefixes();
	}

  void Int_ColorConv(u32 op) 
  {
    int vd = _VD;
    int vs = _VS;
    u32 s[4];
    VectorSize sz = V_Quad;
    ReadVector((float *)s, sz, vs);
    u16 colors[4];
    for (int i = 0; i < 4; i++)
    {
      u32 in = colors[i];
      u16 col = 0;
      switch ((op >> 16) & 3)
      {
      case 1:  // 4444
        {
          int a = ((in >> 24) & 0xFF) >> 4;
          int b = ((in >> 16) & 0xFF) >> 4;
          int g = ((in >> 8) & 0xFF) >> 4;
          int r = ((in) & 0xFF) >> 4;
          col = (a << 12) | (b << 8) | (g << 4 ) | (r);
          break;
        }
      case 2:  // 5551
        {
          int a = ((in >> 24) & 0xFF) >> 4;
          int b = ((in >> 16) & 0xFF) >> 4;
          int g = ((in >> 8) & 0xFF) >> 4;
          int r = ((in) & 0xFF) >> 4;
          col = (a << 15) | (b << 10) | (g << 5) | (r);
          break;
        }
      case 3:  // 565
        {
          int b = ((in >> 16) & 0xFF) >> 3;
          int g = ((in >> 8) & 0xFF) >> 2;
          int r = ((in) & 0xFF) >> 3;
          col = (b << 11) | (g << 5) | (r); 
          break;
        }
      }
      colors[i] = col;
    }
    u32 ov[2] =  {colors[0] | (colors[1] << 16), colors[2] | (colors[3] << 16)};
    WriteVector((const float *)ov, V_Pair, vd);
    PC += 4;
    EatPrefixes();
  }

	void Int_VDot(u32 op)
	{
		float s[4], t[4];
		float d;
		int vd = _VD;
		int vs = _VS;
		int vt = _VT;
		VectorSize sz = GetVecSize(op);
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		ReadVector(t, sz, vt);
		ApplySwizzleT(t, sz);
		float sum = 0.0f;
		int n = GetNumVectorElements(sz);
		for (int i = 0; i < n; i++)
		{
			sum += s[i]*t[i];
		}
		d = sum;
		ApplyPrefixD(&d,V_Single);
		V(vd) = d;
		PC += 4;
		EatPrefixes();
	}

	void Int_Vbfy(u32 op)
	{
		float s[4];
		float d[4];
		int vd = _VD;
		int vs = _VS;
		VectorSize sz = GetVecSize(op);
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		int n = GetNumVectorElements(sz);
		if (op & 0x10000)
		{
			//vbfy2
			d[0] = s[0] + s[2];
			d[1] = s[1] + s[3];
			d[2] = s[0] - s[2];
			d[3] = s[1] - s[3];
		}
		else
		{
			for (int i = 0; i < n; i+=2)
			{
				d[i]   = s[i] + s[i+1];
				d[i+1] = s[i] - s[i+1];
			}
		}
		ApplyPrefixD(d, sz);
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_Vcrs(u32 op)
	{
		//half a cross product
		float s[4], t[4];
		float d[4];
		int vd = _VD;
		int vs = _VS;
		int vt = _VT;
		VectorSize sz = GetVecSize(op);
		if (sz != V_Triple)
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");

		ReadVector(s, sz, vs);
		ReadVector(t, sz, vt);
		// no swizzles allowed
		d[0] = s[1] * t[2];
		d[1] = s[2] * t[0];
		d[2] = s[0] * t[1];
		ApplyPrefixD(d, sz);
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_Vfad(u32 op)
	{
		float s[4];
		float d;
		int vd = _VD;
		int vs = _VS;
		VectorSize sz = GetVecSize(op);
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		float sum = 0.0f;
		int n = GetNumVectorElements(sz);
		for (int i = 0; i < n; i++)
		{
			sum += s[i];
		}
		d = sum;
		ApplyPrefixD(&d,V_Single);
		V(vd) = d;
		PC += 4;
		EatPrefixes();
	}

  void Int_Vavg(u32 op)
  {
    float s[4];
    float d;
    int vd = _VD;
    int vs = _VS;
    VectorSize sz = GetVecSize(op);
    ReadVector(s, sz, vs);
    ApplySwizzleS(s, sz);
    float sum = 0.0f;
    int n = GetNumVectorElements(sz);
    for (int i = 0; i < n; i++)
    {
      sum += s[i];
    }
    d = sum / n;
    ApplyPrefixD(&d, V_Single);
    V(vd) = d;
    PC += 4;
    EatPrefixes();
  }

	void Int_VScl(u32 op)
	{
		float s[4];
		float d[4];
		int vd = _VD;
		int vs = _VS;
		int vt = _VT;
		VectorSize sz = GetVecSize(op);
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		float scale = V(vt);
		int n = GetNumVectorElements(sz);
		for (int i = 0; i < n; i++)
		{
			d[i] = s[i]*scale;
		}
		ApplyPrefixD(d, sz);
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

  void Int_Vrnds(u32 op)
  {
    int vd = _VD;
    int seed = VI(vd);
    currentMIPS->rng.Init(seed);
  }
  
  void Int_VrndX(u32 op)
  {
    float d[4];
    int vd = _VD;
    VectorSize sz = GetVecSize(op);
    int n = GetNumVectorElements(sz);
    for (int i = 0; i < n; i++)
    {
      switch ((op >> 16) & 0x1f)
      {
      case 1: d[i] = currentMIPS->rng.R32(); break;  // vrndi - TODO: copy bits instead?
      case 2: d[i] = 1.0f + ((float)currentMIPS->rng.R32() / 0xFFFFFFFF); break; // vrndf1   TODO: make more accurate
      case 3: d[i] = 2.0f + 2 * ((float)currentMIPS->rng.R32() / 0xFFFFFFFF); break; // vrndf2   TODO: make more accurate
      case 4: d[i] = 0.0f;  // Should not get here
      }
    }
    ApplyPrefixD(d, sz);
    WriteVector(d, sz, vd);
    PC += 4;
    EatPrefixes();
  }

  // Generates one line of a rotation matrix around one of the three axes
	void Int_Vrot(u32 op)
	{
		int vd = _VD;
		int vs = _VS;
		int imm = (op >> 16) & 0x1f;
		VectorSize sz = GetVecSize(op);
		float angle = V(vs) * (float)M_PI * 0.5f;
		bool negSin = (imm & 0x10) ? true : false;
		float sine = sinf(angle);
    float cosine = cosf(angle);
		if (negSin)
      sine = -sine;
		float d[4] = {0};
		if (((imm >> 2) & 3) == (imm & 3))
		{
			for (int i = 0; i < 4; i++)
				d[i] = sine;
		}
		d[(imm >> 2) & 3] = sine;
		d[imm & 3] = cosine;
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_Vtfm(u32 op)
	{
		int vd = _VD;
		int vs = _VS;
		int vt = _VT;
		int ins = (op >> 23) & 7;

		VectorSize sz = GetVecSize(op);
		MatrixSize msz = GetMtxSize(op);
    int n = GetNumVectorElements(sz);

    bool homogenous = false;
    if (n == ins)
    {
      n++;
      sz = (VectorSize)((int)(sz) + 1);
      msz = (MatrixSize)((int)(msz) + 1);
      homogenous = true;
    }
    
		float s[16];
		ReadMatrix(s, msz, vs);
		float t[4];
		ReadVector(t, sz, vt);
		float d[4];
    
		if (homogenous)
		{
			for (int i = 0; i < n; i++)
			{
				d[i] = 0.0f;
				for (int k = 0; k < n; k++)
				{
					d[i] += (k == n-1) ? s[i*4+k] : (s[i*4+k] * t[k]);
				}
			}
		}
		else if (n == ins+1)
		{
			for (int i = 0; i < n; i++)
			{
				d[i] = 0.0f;
				for (int k = 0; k < n; k++)
				{
					d[i] += s[i*4+k] * t[k];
				}
			}
		}
		else
		{
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted (BADVTFM)");
		}
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}
  
	void Int_SV(u32 op)
	{
		s32 imm = (signed short)(op&0xFFFC);
		int vt = ((op >> 16) & 0x1f) | ((op & 3) << 5);
		int rs = (op >> 21) & 0x1f;
		u32 addr = R(rs) + imm;

		switch (op >> 26)
		{
		case 50: //lv.s
      VI(vt) = Memory::Read_U32(addr);
			break;
		case 58: //sv.s
			Memory::Write_U32(VI(vt), addr);
			break;
		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
			break;
		}
		PC += 4;
	}


	void Int_Mftv(u32 op)
	{
		int vd = _VD;
		int rt = _RT;
		switch ((op >> 21) & 0x1f)
		{
		case 3://mfv
			R(rt) = VI(vd);
			break;
		case 7: //mtv
			VI(vd) = R(rt);
			break;
		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
			break;
		}
		PC += 4;
	}

#undef max

	void Int_Vcst(u32 op)
	{
		static const float constants[32] = 
		{
			0,
			std::numeric_limits<float>::infinity(),  // or max() ??   pspautotests seem to indicate inf
			sqrtf(2.0f),
			sqrtf(0.5f),
			2.0f/sqrtf((float)M_PI),
			2.0f/(float)M_PI,
			1.0f/(float)M_PI,
			(float)M_PI/4,
			(float)M_PI/2,
			(float)M_PI,
			(float)M_E,
			(float)M_LOG2E,
			(float)M_LOG10E,
			(float)M_LN2,
			(float)M_LN10,
			2*(float)M_PI,
			(float)M_PI/6,
			log10(2.0f), 
			log(10.0f)/log(2.0f), //"Log2(10)",
			sqrt(3.0f)/2.0f, //"Sqrt(3)/2"
		};

		int conNum = (op >> 16) & 0x1f;
		int vd = _VD;

		VectorSize sz = GetVecSize(op);
		float c = constants[conNum];
		float temp[4] = {c,c,c,c};
		WriteVector(temp, sz, vd);	
		PC += 4;
		EatPrefixes();
	}

	enum VCondition 
	{
		VC_FL,
		VC_EQ,
		VC_LT,
		VC_LE,
		VC_TR,
		VC_NE,
		VC_GE,
		VC_GT,
		VC_EZ,
		VC_EN,
		VC_EI,
		VC_ES,
		VC_NZ,
		VC_NN,
		VC_NI,
		VC_NS
	};

	void Int_Vcmp(u32 op)
	{
		int vt = _VT;
		int vs = _VS;
		int cond = op&15;
		VectorSize sz = GetVecSize(op);
		int n = GetNumVectorElements(sz);
		float s[4];
		float t[4];
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		ReadVector(t, sz, vt);
		ApplySwizzleT(t, sz);
		int cc = 0;
		int or_val = 0;
		int and_val = 1;
		for (int i = 0; i < n; i++)
		{
			int c;
			switch (cond)
			{
			case VC_EZ: c = s[i] == 0.0f || s[i] == -0.0f; break;
			case VC_LT: c = s[i] < t[i]; break;
			case VC_LE: c = s[i] <= t[i]; break;
			case VC_TR: c = 1; break;
			case VC_FL: c = 0; break;
			case VC_NE: c = s[i] != t[i]; break;
			case VC_GT: c = s[i] > t[i]; break;
			case VC_GE: c = s[i] >= t[i]; break;
			case VC_NZ: c = s[i] != 0; break;
			default:
				_dbg_assert_msg_(CPU,0,"Unsupported vcmp condition code"); //, cond);
				return;
			}
			cc |= (c<<i);
			or_val |= c;
			and_val &= c;
		}
		currentMIPS->vfpuCtrl[VFPU_CTRL_CC] = cc | (or_val<<4) | (and_val << 5);
		PC += 4;
		EatPrefixes();
	}


	void Int_Vcmov(u32 op)
	{
		int vs = _VS;
		int vd = _VD; 
		int tf = (op >> 19)&3;
		int imm3 = (op >> 16)&7;
		VectorSize sz = GetVecSize(op);
		int n = GetNumVectorElements(sz);
		float s[4];
		float t[4];
		float d[4];
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		ReadVector(t,sz,vd); //Yes!
		ApplySwizzleT(t, sz);
		for (int i = 0; i < n; i++)
			d[i] = t[i];

		int CC = currentMIPS->vfpuCtrl[VFPU_CTRL_CC];

		if (imm3<6)
		{
			if (((CC>>imm3)&1) == !tf)
			{
				for (int i = 0; i < n; i++)
					d[i] = s[i];
			}
		}
		else if (imm3 == 6)
		{
			for (int i = 0; i < n; i++)
			{
				if (((CC>>i)&1)  == !tf)
					d[i] = s[i];
			}
		}
		else 
		{
			_dbg_assert_msg_(CPU,0,"Bad Imm3 in cmov");
		}
		ApplyPrefixD(d, sz);
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

	void Int_VecDo3(u32 op)
	{
		int vd = _VD;
		int vs = _VS;
		int vt = _VT;
		VectorSize sz = GetVecSize(op);
		float s[4];
		float t[4];
		float d[4];
		ReadVector(s, sz, vs);
		ApplySwizzleS(s, sz);
		ReadVector(t, sz, vt);
		ApplySwizzleT(t, sz);
		for (int i = 0; i < GetNumVectorElements(sz); i++)
		{
			switch(op >> 26)
			{
			case 24: //VFPU0
				switch ((op >> 23)&7)
				{
				case 0: d[i] = s[i] + t[i]; break; //vadd
				case 1: d[i] = s[i] - t[i]; break; //vsub
				case 7: d[i] = s[i] / t[i]; break; //vdiv
				default: goto bad;
				}
				break;
			case 25: //VFPU1
				switch ((op >> 23)&7)
				{
				case 0: d[i] = s[i] * t[i]; break; //vmul
				default: goto bad;
				}
				break;
				/*
			case 27: //VFPU3
				switch ((op >> 23)&7)
				{
				case 0: d[i] = s[i] * t[i]; break; //vmul
				}
				break;*/
			default:
bad:
				_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
				break;
			}
		}
		ApplyPrefixD(d, sz);
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}


	void Int_CrossQuat(u32 op)
	{
		int vd = _VD;
		int vs = _VS;
		int vt = _VT;
		VectorSize sz = GetVecSize(op);
		float s[4];
		float t[4];
		float d[4];
		ReadVector(s, sz, vs);
		ReadVector(t, sz, vt);
		switch (sz)
		{
		case V_Triple:
			d[0] = s[1]*t[2] - s[2]*t[1];
			d[1] = s[2]*t[0] - s[0]*t[2];
			d[2] = s[0]*t[1] - s[1]*t[0];
			//cross
			break;
		//case V_Quad:
			//quat
		//	break;
		default:
			_dbg_assert_msg_(CPU,0,"Trying to interpret instruction that can't be interpreted");
			break;
		}
		WriteVector(d, sz, vd);
		PC += 4;
		EatPrefixes();
	}

}
