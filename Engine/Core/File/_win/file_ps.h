/****************************************************************************
//	Usagi Engine, Copyright © Vitei, Inc. 2013
//	Description: Platform specific file data
*****************************************************************************/
#ifndef _USG_FILE_PS_H_
#define _USG_FILE_PS_H_
#include <stdio.h>

namespace usg {

class File_ps
{
public:
	File_ps(void);
	File_ps(const char* szFileName, FILE_ACCESS_MODE eMode, FILE_TYPE eFileType);
	~File_ps(void);

	static FILE_INIT_RESULT InitFileSystem();
	static void FinalizeFileSystem() {}

	bool Open(const char* szFileName, FILE_ACCESS_MODE eMode, FILE_TYPE eFileType);
	void Close();

	bool IsOpen();
	memsize Read(memsize uSize, void* pDst);
	memsize Write(memsize uSize, const void* pSrc);
	uint8 ReadByte();
	void SeekPos(memsize uPos);
	void AdvanceBytes(memsize uPos);
	void SeekEnd();
	memsize GetPos();
	memsize GetSize();

	static FILE_STATUS FileStatus(const char* szName, const FILE_TYPE eFileType = FILE_TYPE_RESOURCE);
	static bool CreateFileDirectory(const char* szName, FILE_TYPE eFileType);
	static bool Delete(const char* szName, FILE_TYPE eFileType);
	// Note: The 'maximumFileCount' parameter is ignored on this platform
	static size_t NumberOfFilesInDirectory(const char* szDirName, FILE_TYPE eFileType,
	                                       size_t maximumFileCount);

	static void Mount(FILE_TYPE eMode) {  }
	static void Unmount(FILE_TYPE eMode, bool bSave) {  }

private:
	File_ps(const File_ps& File) {};
	const File_ps& operator=(const File_ps& src);
	FILE*	m_pFile;

};

}

#endif
