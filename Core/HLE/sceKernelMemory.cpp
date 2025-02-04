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
#include "../System.h"
#include "../MIPS/MIPS.h"
#include "../MemMap.h"

#include "sceKernel.h"
#include "sceKernelThread.h"
#include "sceKernelMemory.h"


//////////////////////////////////////////////////////////////////////////
// STATE BEGIN
BlockAllocator userMemory;
BlockAllocator kernelMemory;
// STATE END
//////////////////////////////////////////////////////////////////////////


struct NativeFPL
{
	u32 size;
	char name[KERNELOBJECT_MAX_NAME_LENGTH+1];
	SceUID mpid;
	u32 attr;
	int blocksize;
	int numBlocks;
	int numFreeBlocks;
	int numWaitThreads;
};

//FPL - Fixed Length Dynamic Memory Pool - every item has the same length
struct FPL : KernelObject
{
	const char *GetName() {return nf.name;}
	const char *GetTypeName() {return "FPL";}
	static u32 GetMissingErrorCode() { return SCE_KERNEL_ERROR_UNKNOWN_FPLID; }
	int GetIDType() const { return SCE_KERNEL_TMID_Fpl; }
	NativeFPL nf;
	bool *freeBlocks;
	u32 address;
};

struct SceKernelVplInfo
{
	SceSize size;
	char name[KERNELOBJECT_MAX_NAME_LENGTH+1];
	SceUInt attr;
	int poolSize;
	int freeSize;
	int numWaitThreads;

};

struct VPL : KernelObject
{
	const char *GetName() {return nv.name;}
	const char *GetTypeName() {return "VPL";}
	static u32 GetMissingErrorCode() { return SCE_KERNEL_ERROR_UNKNOWN_VPLID; }
	int GetIDType() const { return SCE_KERNEL_TMID_Vpl; }
	SceKernelVplInfo nv;
	u32 size;
	bool *freeBlocks;
	u32 address;
	BlockAllocator alloc;
};

void __KernelMemoryInit()
{
	kernelMemory.Init(PSP_GetKernelMemoryBase(), PSP_GetKernelMemoryEnd()-PSP_GetKernelMemoryBase());
	userMemory.Init(PSP_GetUserMemoryBase(), PSP_GetUserMemoryEnd()-PSP_GetUserMemoryBase());
	INFO_LOG(HLE, "Kernel and user memory pools initialized");
}

void __KernelMemoryShutdown()
{
	INFO_LOG(HLE,"Shutting down user memory pool: ");
	userMemory.ListBlocks();
	userMemory.Shutdown();
	INFO_LOG(HLE,"Shutting down \"kernel\" memory pool: ");
	kernelMemory.ListBlocks();
	kernelMemory.Shutdown();
}

//sceKernelCreateFpl(const char *name, SceUID mpid, SceUint attr, SceSize blocksize, int numBlocks, optparam)
void sceKernelCreateFpl()
{
	const char *name = Memory::GetCharPointer(PARAM(0));

	u32 mpid = PARAM(1);
	u32 attr = PARAM(2);
	u32 blockSize = PARAM(3);
	u32 numBlocks = PARAM(4);

	u32 totalSize = blockSize * numBlocks;

	u32 address = userMemory.Alloc(totalSize, false, "FPL");
	if (address == (u32)-1)
	{
		DEBUG_LOG(HLE,"sceKernelCreateFpl(\"%s\", partition=%i, attr=%i, bsize=%i, nb=%i) FAILED - out of ram", 
			name, mpid, attr, blockSize, numBlocks);
		RETURN(SCE_KERNEL_ERROR_NO_MEMORY);
		return;
	}

	FPL *fpl = new FPL;
	SceUID id = kernelObjects.Create(fpl);
	strncpy(fpl->nf.name, name, 32);

	fpl->nf.size = sizeof(fpl->nf);
	fpl->nf.mpid = mpid; //partition
	fpl->nf.attr = attr;
	fpl->nf.blocksize = blockSize;
	fpl->nf.numBlocks = numBlocks;
	fpl->nf.numWaitThreads = 0;
	fpl->freeBlocks = new bool[fpl->nf.numBlocks];
		
	fpl->address = address;

	memset(fpl->freeBlocks, 0, fpl->nf.numBlocks * sizeof(bool));
	DEBUG_LOG(HLE,"%i=sceKernelCreateFpl(\"%s\", partition=%i, attr=%i, bsize=%i, nb=%i)", 
		id, name, mpid, attr, blockSize, numBlocks);

	RETURN(id);
}

void sceKernelDeleteFpl()
{
	SceUID id = PARAM(0);
	u32 error;
	FPL *fpl = kernelObjects.Get<FPL>(id, error);
	if (fpl)
	{
		userMemory.Free(fpl->address);
		DEBUG_LOG(HLE,"sceKernelDeleteFpl(%i)", id);
		RETURN(kernelObjects.Destroy<FPL>(id));
	}
	else
	{
		RETURN(error);
	}
}

void sceKernelAllocateFpl()
{
	SceUID id = PARAM(0);
	u32 error;
	FPL *fpl = kernelObjects.Get<FPL>(id, error);
	if (fpl)
	{
		u32 blockPtrAddr = PARAM(1);
		int timeOut = PARAM(2);
		DEBUG_LOG(HLE,"FAKEY sceKernelAllocateFpl(%i, %08x, %i)", id, PARAM(1), timeOut);
		Memory::Write_U32(fpl->address, blockPtrAddr);
		RETURN(0);
	}
	else
	{
		RETURN(error);
	}
}

void sceKernelAllocateFplCB()
{
	SceUID id = PARAM(0);
	DEBUG_LOG(HLE,"UNIMPL: sceKernelAllocateFplCB(%i)", id);
	u32 error;
	FPL *fpl = kernelObjects.Get<FPL>(id, error);
	if (fpl)
	{
		RETURN(0);
	}
	else
	{
		RETURN(error);
	}
}

void sceKernelTryAllocateFpl()
{
	SceUID id = PARAM(0);
	DEBUG_LOG(HLE,"BAD sceKernelTryAllocateFpl(%i)", id);
	u32 error;
	FPL *fpl = kernelObjects.Get<FPL>(id, error);
	if (fpl)
	{
		u32 blockPtrAddr = PARAM(1);
		DEBUG_LOG(HLE,"sceKernelTryAllocateFpl(%i, %08x)", id, PARAM(1));
		Memory::Write_U32(fpl->address, blockPtrAddr);
		RETURN(0);
	}
	else
	{
		RETURN(error);
	}
}

void sceKernelFreeFpl()
{
	SceUID id = PARAM(0);
	ERROR_LOG(HLE,"UNIMPL: sceKernelFreeFpl(%i)", id);
	u32 error;
	FPL *fpl = kernelObjects.Get<FPL>(id, error);
	if (fpl)
	{
		RETURN(0);
	}
	else
	{
		RETURN(error);
	}
}

void sceKernelCancelFpl()
{
	SceUID id = PARAM(0);
	DEBUG_LOG(HLE,"UNIMPL: sceKernelCancelFpl(%i)", id);
	u32 error;
	FPL *fpl = kernelObjects.Get<FPL>(id, error);
	if (fpl)
	{
		RETURN(0);
	}
	else
	{
		RETURN(error);
	}
}

void sceKernelReferFplStatus()
{
	SceUID id = PARAM(0);
	u32 statusAddr = PARAM(1);
	DEBUG_LOG(HLE,"sceKernelReferFplStatus(%i, %08x)", id, statusAddr);
	u32 error;
	FPL *fpl = kernelObjects.Get<FPL>(id, error);
	if (fpl)
	{
		Memory::WriteStruct(statusAddr, &fpl->nf);
		RETURN(0);
	}
	else
	{
		RETURN(error);
	}
}



//////////////////////////////////////////////////////////////////////////
// ALLOCATIONS
//////////////////////////////////////////////////////////////////////////
//00:49:12 <TyRaNiD> ector, well the partitions are 1 = kernel, 2 = user, 3 = me, 4 = kernel mirror :)


class PartitionMemoryBlock : public KernelObject
{
public:
	const char *GetName() {return name;}
	const char *GetTypeName() {return "MemoryPart";}
	void GetQuickInfo(char *ptr, int size)
	{
		int sz = alloc->GetBlockSizeFromAddress(address);
		sprintf(ptr, "MemPart: %08x - %08x	size: %08x", address, address + sz, sz);
	}
	static u32 GetMissingErrorCode() { return SCE_KERNEL_ERROR_UNKNOWN_MPPID; }	/// ????
	int GetIDType() const { return 0; }

	PartitionMemoryBlock(BlockAllocator *_alloc, u32 size, bool fromEnd)
	{
		alloc = _alloc;
		address = alloc->Alloc(size, fromEnd);
		alloc->ListBlocks();
	}
	~PartitionMemoryBlock()
	{
		alloc->Free(address);
	}
	bool IsValid() {return address != (u32)-1;}
	BlockAllocator *alloc;
	u32 address;
	char name[32];
};


void sceKernelMaxFreeMemSize() 
{
	// TODO: Fudge factor improvement
	u32 retVal = userMemory.GetLargestFreeBlockSize()-0x8000;
	DEBUG_LOG(HLE,"%08x (dec %i)=sceKernelMaxFreeMemSize",retVal,retVal);
	RETURN(retVal);
}
void sceKernelTotalFreeMemSize()
{
	u32 retVal = userMemory.GetLargestFreeBlockSize()-0x8000;
	DEBUG_LOG(HLE,"%08x (dec %i)=sceKernelTotalFreeMemSize",retVal,retVal);
	RETURN(retVal);
}

void sceKernelAllocPartitionMemory()
{
	int partid = PARAM(0);
	const char *name = Memory::GetCharPointer(PARAM(1));
	int type = PARAM(2);
	u32 size = PARAM(3);
	int addr = PARAM(4); //only if type == addr

	PartitionMemoryBlock *block = new PartitionMemoryBlock(&userMemory, size, type==1);
	if (!block->IsValid())
	{
		ERROR_LOG(HLE, "ARGH! sceKernelAllocPartMem failed");
		RETURN(-1);
	}
	SceUID id = kernelObjects.Create(block);
	strncpy(block->name, name, 32);

	DEBUG_LOG(HLE,"%i = sceKernelAllocPartMem(partition = %i, %s, type= %i, size= %i, addr= %08x)",
		id, partid,name,type,size,addr);
	if (type == 2)
		ERROR_LOG(HLE, "ARGH! sceKernelAllocPartMem wants a specific address");

	RETURN(id); //for now
}

void sceKernelFreePartitionMemory()
{
	SceUID id = PARAM(0);
	DEBUG_LOG(HLE,"sceKernelFreePartitionMemory(%d)",id);

	RETURN(kernelObjects.Destroy<PartitionMemoryBlock>(id));
}

void sceKernelGetBlockHeadAddr()
{
	SceUID id = PARAM(0);

	u32 error;
	PartitionMemoryBlock *block = kernelObjects.Get<PartitionMemoryBlock>(id, error);
	if (block)
	{
		DEBUG_LOG(HLE,"%08x = sceKernelGetBlockHeadAddr(%i)", block->address, id);
		RETURN(block->address);
	}
	else
	{
		ERROR_LOG(HLE,"sceKernelGetBlockHeadAddr failed(%i)", id);
		RETURN(error);
	}
}


void sceKernelPrintf()
{
	ERROR_LOG(HLE,"UNIMPL sceKernelPrintf(%08x, %08x, %08x, %08x)", PARAM(0),PARAM(1),PARAM(2),PARAM(3));
	RETURN(0);
}

void sceKernelSetCompiledSdkVersion()
{
	//pretty sure this only takes one arg
	ERROR_LOG(HLE,"UNIMPL sceKernelSetCompiledSdkVersion(%08x)", PARAM(0));
	RETURN(0);
}

void sceKernelSetCompilerVersion()
{
	ERROR_LOG(HLE,"UNIMPL sceKernelSetCompilerVersion(%08x, %08x, %08x, %08x)", PARAM(0),PARAM(1),PARAM(2),PARAM(3));
	RETURN(0);
}

// VPL = variable length memory pool


void sceKernelCreateVpl()
{
	const char *name = Memory::GetCharPointer(PARAM(0));
	VPL *vpl = new VPL;
	SceUID id = kernelObjects.Create(vpl);

	strncpy(vpl->nv.name, name, 32);
	//vpl->nv.mpid = PARAM(1); //seems to be the standard memory partition (user, kernel etc)
	vpl->nv.attr = PARAM(2);
	vpl->size = PARAM(3);
	vpl->nv.poolSize = vpl->size;
	vpl->nv.size = sizeof(vpl->nv);
	vpl->nv.numWaitThreads = 0;
	vpl->nv.freeSize = vpl->nv.poolSize;
		
	vpl->address = userMemory.Alloc(vpl->size, false, "VPL");
	vpl->alloc.Init(vpl->address, vpl->size);

	DEBUG_LOG(HLE,"sceKernelCreateVpl(\"%s\", block=%i, attr=%i, size=%i)", 
		name, PARAM(1), vpl->nv.attr, vpl->size);

	RETURN(id);
}

void sceKernelDeleteVpl()
{
	SceUID id = PARAM(0);
	DEBUG_LOG(HLE,"sceKernelDeleteVpl(%i)", id);
	u32 error;
	VPL *vpl = kernelObjects.Get<VPL>(id, error);
	if (vpl)
	{
		userMemory.Free(vpl->address);
		kernelObjects.Destroy<VPL>(id);
		RETURN(0);
	}
	else
	{
		RETURN(error);
	}
}

void sceKernelAllocateVpl()
{
	SceUID id = PARAM(0);
	DEBUG_LOG(HLE,"sceKernelAllocateVpl()");
	u32 error;
	VPL *vpl = kernelObjects.Get<VPL>(id, error);
	if (vpl)
	{
		u32 size = PARAM(1);
		u32 *blockPtrPtr = (u32 *)Memory::GetPointer(PARAM(2));
		int timeOut = PARAM(2);
		DEBUG_LOG(HLE,"sceKernelAllocateVpl(vpl=%i, size=%i, ptrout= %08x , timeout=%i)", id, size, PARAM(2), timeOut);
		u32 addr = vpl->alloc.Alloc(size);
		if (addr != (u32)-1)
		{
			*blockPtrPtr = addr;
			RETURN(0);
		}
		else
		{
			ERROR_LOG(HLE, "FAILURE");
			RETURN(-1);
		}
	}
	else
	{
		RETURN(error);
	}
	RETURN(0);
}

void sceKernelAllocateVplCB()
{
	SceUID id = PARAM(0);
	u32 error;
	VPL *vpl = kernelObjects.Get<VPL>(id, error);
	if (vpl)
	{
		u32 size = PARAM(1);
		u32 *blockPtrPtr = (u32 *)Memory::GetPointer(PARAM(2));
		int timeOut = PARAM(2);
		DEBUG_LOG(HLE,"sceKernelAllocateVplCB(vpl=%i, size=%i, ptrout= %08x , timeout=%i)", id, size, PARAM(2), timeOut);
		u32 addr = vpl->alloc.Alloc(size);
		if (addr != (u32)-1)
		{
			*blockPtrPtr = addr;
			RETURN(0);
		}
		else
		{
			ERROR_LOG(HLE, "sceKernelAllocateVplCB FAILURE");
			RETURN(-1);
		}
	}
	else
	{
		RETURN(error);
	}
	RETURN(0);
}

void sceKernelTryAllocateVpl()
{
	SceUID id = PARAM(0);
	u32 error;
	VPL *vpl = kernelObjects.Get<VPL>(id, error);
	if (vpl)
	{
		u32 size = PARAM(1);
		u32 *blockPtrPtr = (u32 *)Memory::GetPointer(PARAM(2));
		int timeOut = PARAM(2);
		DEBUG_LOG(HLE,"sceKernelAllocateVplCB(vpl=%i, size=%i, ptrout= %08x , timeout=%i)", id, size, PARAM(2), timeOut);
		u32 addr = vpl->alloc.Alloc(size);
		if (addr != (u32)-1)
		{
			*blockPtrPtr = addr;
			RETURN(0);
		}
		else
		{
			ERROR_LOG(HLE, "sceKernelTryAllocateVpl FAILURE");
			RETURN(-1);
		}
	}
	else
	{
		RETURN(error);
	}
	RETURN(0);
}

void sceKernelFreeVpl()
{
	ERROR_LOG(HLE,"UNIMPL: sceKernelFreeVpl()");
	RETURN(0);
}

void sceKernelCancelVpl()
{
	ERROR_LOG(HLE,"UNIMPL: sceKernelCancelVpl()");
	RETURN(0);
}

void sceKernelReferVplStatus()
{
	SceUID id = PARAM(0);
	u32 error;
	VPL *v = kernelObjects.Get<VPL>(id, error);
	if (v)
	{
		DEBUG_LOG(HLE,"sceKernelReferVplStatus(%i, %08x)", id, PARAM(1));
		SceKernelVplInfo *info = (SceKernelVplInfo*)Memory::GetPointer(PARAM(1));
		v->nv.freeSize = v->alloc.GetTotalFreeBytes();
		*info = v->nv;
	}
	else
	{
		ERROR_LOG(HLE,"Error %08x", error);
		RETURN(error);
	}
	RETURN(0);
}


const HLEFunction SysMemUserForUser[] = 
{
	{0xA291F107,sceKernelMaxFreeMemSize,	"sceKernelMaxFreeMemSize"},
	{0xF919F628,sceKernelTotalFreeMemSize,"sceKernelTotalFreeMemSize"},
	{0x3FC9AE6A,sceKernelDevkitVersion,	 "sceKernelDevkitVersion"},	
	{0x237DBD4F,sceKernelAllocPartitionMemory,"sceKernelAllocPartitionMemory"},	//(int size) ?
	{0xB6D61D02,sceKernelFreePartitionMemory,"sceKernelFreePartitionMemory"},	 //(void *ptr) ?
	{0x9D9A5BA1,sceKernelGetBlockHeadAddr,"sceKernelGetBlockHeadAddr"},			//(void *ptr) ?
	{0x13a5abef,sceKernelPrintf,"sceKernelPrintf 0x13a5abef"},
	{0x7591c7db,sceKernelSetCompiledSdkVersion,"sceKernelSetCompiledSdkVersion"},
	{0x342061E5,0,"sceKernelSetCompiledSdkVersion370"},
	{0x315AD3A0,0,"sceKernelSetCompiledSdkVersion380_390"},
	{0xEBD5C3E6,0,"sceKernelSetCompiledSdkVersion395"},
	{0xf77d77cb,sceKernelSetCompilerVersion,"sceKernelSetCompilerVersion"},
	{0x35669d4c,0,"SysMemUserForUser_35669d4c"},
	{0x1b4217bc,0,"SysMemUserForUser_1b4217bc"},
};


void Register_SysMemUserForUser()
{
	RegisterModule("SysMemUserForUser", ARRAY_SIZE(SysMemUserForUser), SysMemUserForUser);
}
