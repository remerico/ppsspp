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

#ifdef _WIN32
#include <windows.h>
#include <WindowsX.h>
#else
#include <unistd.h>
#endif

#include <cstring>
#include <algorithm>

#include "../../Globals.h"
#include "../../Core/MemMap.h"
#include "SymbolMap.h"

#include <vector>
#include <map>

SymbolMap symbolMap;

//need improvement
static u32 hasher(u32 last, u32 value)
{
	return _rotl(last,3) ^ value;
}

//#define BWLINKS

// TODO: This should ignore immediates of many instructions, in order to be less sensitive. If it did,
// this could work okay.
static u32 ComputeHash(u32 start, u32 size)
{
	u32 hash=0;
	for (unsigned int i=start; i<start+size; i+=4)
		hash = hasher(hash, Memory::Read_Instruction(i));			
	return hash;
}

void SymbolMap::SortSymbols()
{
  std::sort(entries, entries + numEntries);
}

void SymbolMap::AnalyzeBackwards()
{
#ifndef BWLINKS
	return;
#else
	for (int i=0; i<numEntries; i++)
	{
		u32 ptr = entries[i].vaddress;
		if (ptr)
		{
			if (entries[i].type == ST_FUNCTION)
			{
				for (int a = 0; a<entries[i].size/4; a++)
				{
					u32 inst = CMemory::ReadUncheckedu32(ptr);

					switch (inst>>26)
					{
					case 18:
						{
							if (LK) //LK
							{
								u32 addr;
								if(AA)
									addr = SignExt26(LI << 2);
								else
									addr = ptr + SignExt26(LI << 2);

								int funNum = SymbolMap::GetSymbolNum(addr);
								if (funNum>=0) 
									entries[funNum].backwardLinks.push_back(ptr);
							}
							break;
						}
					default:
						;
					}

					ptr+=4;
				}
			}
		}
	}
#endif
}


void SymbolMap::ResetSymbolMap()
{
#ifdef BWLINKS
	for (int i=0; i<numEntries; i++)
	{
		entries[i].backwardLinks.clear();
	}
#endif
	numEntries=0;
}


void SymbolMap::AddSymbol(const char *symbolname, unsigned int vaddress, size_t size, SymbolType st)
{
	MapEntry &e = entries[numEntries++];
	strcpy(e.name, symbolname);
	e.vaddress = vaddress;
	e.size = size;
	e.type=st;
}

bool SymbolMap::LoadSymbolMap(const char *filename)
{
	SymbolMap::ResetSymbolMap();
	numEntries=0;
	FILE *f = fopen(filename,"r");
	if (!f)
		return false;
	//char temp[256];
	//fgets(temp,255,f); //.text section layout
	//fgets(temp,255,f); //  Starting        Virtual
	//fgets(temp,255,f); //  address  Size   address
	//fgets(temp,255,f); //  -----------------------

	bool started=false;

	while (!feof(f))
	{
		char line[512],temp[256];
		fgets(line,511,f);
		if (strlen(line)<4)
			continue;

		sscanf(line,"%s",temp);
		if (strcmp(temp,"UNUSED")==0) continue;
		if (strcmp(temp,".text")==0)  {started=true;continue;};
		if (strcmp(temp,".init")==0)  {started=true;continue;};
		if (strcmp(temp,"Starting")==0) continue;
		if (strcmp(temp,"extab")==0) continue;
		if (strcmp(temp,".ctors")==0) break;
		if (strcmp(temp,".dtors")==0) break;
		if (strcmp(temp,".rodata")==0) continue;
		if (strcmp(temp,".data")==0) continue;
		if (strcmp(temp,".sbss")==0) continue;
		if (strcmp(temp,".sdata")==0) continue;
		if (strcmp(temp,".sdata2")==0) continue;
		if (strcmp(temp,"address")==0)  continue;
		if (strcmp(temp,"-----------------------")==0)  continue;
		if (strcmp(temp,".sbss2")==0) break;
		if (temp[1]==']') continue;

		if (!started) continue;
		MapEntry &e = entries[numEntries++];
		sscanf(line,"%08x %08x %08x %i %s",&e.address,&e.size,&e.vaddress,(int*)&e.type,e.name);
		
		if (e.type == ST_DATA && e.size==0)
			e.size=4;
		//e.vaddress|=0x80000000;
		if (strcmp(e.name,".text")==0 || strcmp(e.name,".init")==0 || strlen(e.name)<=1)
			numEntries--;

	}
	for (int i = 0; i < numEntries; i++)
		entries[i].UndecorateName();
	fclose(f);
	SymbolMap::SortSymbols();
//	SymbolMap::AnalyzeBackwards();
	return true;
}


void SymbolMap::SaveSymbolMap(const char *filename)
{
	FILE *f = fopen(filename,"w");
	if (!f)
		return;
	fprintf(f,".text\n");
	for (int i=0; i<numEntries; i++)
	{
		MapEntry &e = entries[i];
		fprintf(f,"%08x %08x %08x %i %s\n",e.address,e.size,e.vaddress,e.type,e.name);
	}
	fclose(f);
}

int SymbolMap::GetSymbolNum(unsigned int address, SymbolType symmask)
{
	int start=0;

	for (int i = 0; i < numEntries; i += 100)
	{
		if (address >= entries[i].vaddress)
			start = i;
		else
			break;
	}
	
	if (start<0) start=0;

	for (int i=start; i<numEntries; i++)
	{
		unsigned int addr = entries[i].vaddress;
		if ((address >= addr))
		{
			if (address < addr+entries[i].size)
			{
				if (entries[i].type & symmask)
					return i;
				else
					return -1;
			}
		}
		else
			break;
	}
	return -1;
}


char temp[256];

char *SymbolMap::GetDescription(unsigned int address)
{
	int fun = SymbolMap::GetSymbolNum(address);
	//if (address == entries[fun].vaddress)
	//{
	if (fun!=-1)
		return entries[fun].name;
	else
	{
		sprintf(temp, "(%08x)", address);
		return temp;
	}
	//}
	//else
	//	return "";
}

#ifdef _WIN32
void SymbolMap::FillSymbolListBox(HWND listbox,SymbolType symmask)
{
	ShowWindow(listbox,SW_HIDE);
	ListBox_ResetContent(listbox);

	//int style = GetWindowLong(listbox,GWL_STYLE);

	ListBox_AddString(listbox,"(0x80000000)");
	ListBox_SetItemData(listbox,0,0x80000000);

	//ListBox_AddString(listbox,"(0x80002000)");
	//ListBox_SetItemData(listbox,1,0x80002000);

	for (int i=0; i<numEntries; i++)
	{
		if (entries[i].type & symmask)
		{
			char temp[256];
			sprintf(temp,"%s (%d)",entries[i].name,entries[i].size);
			int index = ListBox_AddString(listbox,temp);
			ListBox_SetItemData(listbox,index,entries[i].vaddress);
		}
	}

	ShowWindow(listbox,SW_SHOW);
}

void SymbolMap::FillSymbolComboBox(HWND listbox,SymbolType symmask)
{
	ShowWindow(listbox,SW_HIDE);
	ComboBox_ResetContent(listbox);

	//int style = GetWindowLong(listbox,GWL_STYLE);

	ComboBox_AddString(listbox,"(0x02000000)");
	ComboBox_SetItemData(listbox,0,0x02000000);

	//ListBox_AddString(listbox,"(0x80002000)");
	//ListBox_SetItemData(listbox,1,0x80002000);

	for (int i=0; i<numEntries; i++)
	{
		if (entries[i].type & symmask)
		{
			char temp[256];
			sprintf(temp,"%s (%d)",entries[i].name,entries[i].size);
			int index = ComboBox_AddString(listbox,temp);
			ComboBox_SetItemData(listbox,index,entries[i].vaddress);
		}
	}
	
	ShowWindow(listbox,SW_SHOW);
}

void SymbolMap::FillListBoxBLinks(HWND listbox, int num)
{	
	ListBox_ResetContent(listbox);
		
	int style = GetWindowLong(listbox,GWL_STYLE);

	MapEntry &e = entries[num];
#ifdef BWLINKS
	for (int i=0; i<e.backwardLinks.size(); i++)
	{
		u32 addr = e.backwardLinks[i];
		int index = ListBox_AddString(listbox,SymbolMap::GetSymbolName(SymbolMap::GetSymbolNum(addr)));
		ListBox_SetItemData(listbox,index,addr);
	}
#endif
}
#endif

int SymbolMap::GetNumSymbols()
{
	return numEntries;
}
char *SymbolMap::GetSymbolName(int i)
{
	return entries[i].name;
}
void SymbolMap::SetSymbolName(int i, const char *newname)
{
	strcpy(entries[i].name, newname);
}

u32 SymbolMap::GetSymbolAddr(int i)
{
	return entries[i].vaddress;
}

u32 SymbolMap::GetSymbolSize(int i)
{
	return entries[i].size;
}

int SymbolMap::FindSymbol(const char *name)
{
	for (int i=0; i<numEntries; i++)
		if (strcmp(entries[i].name,name)==0)
			return i;
	return -1;
}

u32 SymbolMap::GetAddress(int num)
{
	return entries[num].vaddress;
}

void SymbolMap::IncreaseRunCount(int num)
{
	entries[num].runCount++;
}

unsigned int SymbolMap::GetRunCount(int num)
{
	if (num>=0)
		return entries[num].runCount;
	else
		return 0;
}

// Load an elf with symbols, use SymbolMap::compilefuncsignaturesfile 
// to make a symbol map, load a dol or somethin without symbols, then apply 
// the map with SymbolMap::usefuncsignaturesfile.

void SymbolMap::CompileFuncSignaturesFile(const char *filename)
{
	// Store name,length,first instruction,hash into file
	FILE *f = fopen(filename, "w");
	fprintf(f,"00000000\n");
	int count=0;
	for (int i=0; i<numEntries; i++)
	{
		int size = entries[i].size;
		if (size >= 16 && entries[i].type == ST_FUNCTION)
		{
			u32 inst = Memory::Read_Instruction(entries[i].vaddress); //try to make a bigger number of different vals sometime
			if (inst != 0)
			{
				char temp[64];
				strncpy(temp,entries[i].name,63);
				fprintf(f, "%08x\t%08x\t%08x\t%s\n", inst, size, ComputeHash(entries[i].vaddress,size), temp);    
				count++;
			}
		}
	}
	fseek(f,0,SEEK_SET);
	fprintf(f,"%08x",count);
	fclose(f);
}


struct Sig
{
	u32 inst;
	u32 size;
	u32 hash;
	char name[64];
	Sig(){}
	Sig(u32 _inst, u32 _size, u32 _hash, char *_name) : inst(_inst), size(_size), hash(_hash)
	{
		strncpy(name,_name,63);
	}
	
};

int compare (const void *s1, const void *s2)
{
	return ((Sig *)s2)->inst - ((Sig *)s1)->inst;
}

#define MAXSIGS 65536*2
Sig sigs[MAXSIGS];
int numSigs;

typedef std::map <u32,Sig *> Sigmap;
Sigmap sigmap;

void SymbolMap::UseFuncSignaturesFile(const char *filename, u32 maxAddress)
{
	numSigs=0;
	//SymbolMap::ResetSymbolMap();
	//#1: Read the signature file and put them in a fast data structure
	FILE *f = fopen(filename, "r");
	int count;
	fscanf(f,"%08x\n",&count);
	u32 inst,size,hash;
	char name[256];
	for (int a=0; a<count; a++)
	{
		if (fscanf(f,"%08x\t%08x\t%08x\t%s\n",&inst,&size,&hash,name)!=EOF)
			sigs[numSigs++]=Sig(inst,size,hash,name);
		else
			break;
	}
	
	fclose(f);
	qsort(sigs, numSigs,sizeof(Sig),compare);

	f = fopen("C:\\mojs.txt", "w");
	fprintf(f,"00000000\n");
	for (int j=0; j<numSigs; j++)
		fprintf(f, "%08x\t%08x\t%08x\t%s\n", sigs[j].inst, sigs[j].size, sigs[j].hash, sigs[j].name);    
	fseek(f,0,SEEK_SET);
	fprintf(f,"%08x",numSigs);
	fclose(f);

	u32 last = 0xc0d3babe;
	int i;
	for (i=0; i<numSigs; i++)
  {
		if (sigs[i].inst != last)
		{
			sigmap.insert(Sigmap::value_type(sigs[i].inst, sigs+i));
			last = sigs[i].inst;
		}
  }

	sigs[i].inst=0;

	//#2: Scan/hash the memory and locate functions
	char temp[256];
	u32 lastAddr=0;
	for (u32 addr = 0x80000000; addr<maxAddress; addr+=4)
	{
		if ((addr&0xFFFF0000) != (lastAddr&0xFFFF0000))
		{
			sprintf(temp,"Scanning: %08x",addr);
			lastAddr=addr;
		}
		u32 inst = Memory::Read_Instruction(addr);
		if (!inst) 
			continue;

		Sigmap::iterator iter = sigmap.find(inst);
		if (iter != sigmap.end())
		{
			Sig *sig = iter->second;
			while (true)
			{
				if (sig->inst != inst)
					break;

				u32 hash = ComputeHash(addr,sig->size);				
				if (hash==sig->hash)
				{
					//MATCH!!!!
					MapEntry &e = entries[numEntries++];
					e.address=addr;
					e.size= sig->size;
					e.vaddress = addr;
					e.type=ST_FUNCTION;
					strcpy(e.name,sig->name);
					addr+=sig->size-4; //don't need to check function interior
					break;
				}
				sig++;
			}
		}
	}
	//ensure code coloring even if symbols were loaded before
	SymbolMap::SortSymbols();
}
