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

#include "FileSystem.h"

class MetaFileSystem : public IHandleAllocator, public IFileSystem
{
public:
	MetaFileSystem()
	{
		current = 6;  // what?
	}

	void Mount(std::string prefix, IFileSystem *system);
	void Unmount(IFileSystem *system);

	// Effectively "Shutdown".
	void UnmountAll();

	u32 GetNewHandle() {return current++;}
	void FreeHandle(u32 handle) {}

	IFileSystem *GetHandleOwner(u32 handle);
	bool MapFilePath(std::string inpath, std::string &outpath, IFileSystem **system);

	std::vector<PSPFileInfo> GetDirListing(std::string path);
	u32      OpenFile(std::string filename, FileAccess access);
	void     CloseFile(u32 handle);
	size_t   ReadFile(u32 handle, u8 *pointer, s64 size);
	size_t   WriteFile(u32 handle, const u8 *pointer, s64 size);
	size_t   SeekFile(u32 handle, s32 position, FileMove type);
	PSPFileInfo GetFileInfo(std::string filename);
	bool     OwnsHandle(u32 handle) {return false;}
	inline size_t GetSeekPos(u32 handle)
	{
		return SeekFile(handle, 0, FILEMOVE_CURRENT);
	}

	virtual void ChDir(std::string dir) {currentDirectory = dir;}

	virtual bool MkDir(const std::string &dirname);
	virtual bool RmDir(const std::string &dirname);
	virtual bool DeleteFile(const std::string &filename);

	// TODO: void IoCtl(...)

	void SetCurrentDirectory(const std::string &dir) {
		currentDirectory = dir;
	}
private:
	u32 current;
	struct System
	{
		std::string prefix;
		IFileSystem *system;
	};
	std::vector<System> fileSystems;

	std::string currentDirectory;
};
