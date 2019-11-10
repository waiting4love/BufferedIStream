// reference from reactos/dll/win32/shlwapi/istream.c
// thanks to reactos project
// see https://github.com/reactos/reactos

#include <Windows.h>
#include <Ole2.h>
#include "PipeStream.h"

class HandleStream : public IStream
{
private:
	HandleStream(HANDLE);
	volatile LONG ref;
private:
	HANDLE handle;
public:
	~HandleStream();
	static void Create(HANDLE handle, IStream** ppstm);
	// Í¨¹ý IStream ¼Ì³Ð
	virtual HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObject) override;
	virtual ULONG __stdcall AddRef(void) override;
	virtual ULONG __stdcall Release(void) override;
	virtual HRESULT __stdcall Read(void* pv, ULONG cb, ULONG* pcbRead) override;
	virtual HRESULT __stdcall Write(const void* pv, ULONG cb, ULONG* pcbWritten) override;
	virtual HRESULT __stdcall Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override;
	virtual HRESULT __stdcall SetSize(ULARGE_INTEGER libNewSize) override;
	virtual HRESULT __stdcall CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override;
	virtual HRESULT __stdcall Commit(DWORD grfCommitFlags) override;
	virtual HRESULT __stdcall Revert(void) override;
	virtual HRESULT __stdcall LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
	virtual HRESULT __stdcall UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
	virtual HRESULT __stdcall Stat(STATSTG* pstatstg, DWORD grfStatFlag) override;
	virtual HRESULT __stdcall Clone(IStream** ppstm) override;
};

HandleStream::HandleStream(HANDLE hFile)
	:handle(hFile), ref(1)
{

}

HandleStream::~HandleStream()
{
	Commit(0); /* If ever buffered, this will be needed */
	CloseHandle(handle);
}
void HandleStream::Create(HANDLE handle, IStream** ppstm)
{
	*ppstm = new HandleStream(handle);
}
void CreateStreamFromHandle(HANDLE handle, IStream** ppStm)
{
	HandleStream::Create(handle, ppStm);
}
/**************************************************************************
*  HandleStream::QueryInterface
*/
HRESULT WINAPI HandleStream::QueryInterface(REFIID riid, LPVOID* ppvObj)
{
	*ppvObj = NULL;

	if (IsEqualIID(riid, IID_IUnknown) ||
		IsEqualIID(riid, IID_IStream))
	{
		AddRef();
		*ppvObj = this;
		return S_OK;
	}
	return E_NOINTERFACE;
}

/**************************************************************************
*  HandleStream::AddRef
*/
ULONG WINAPI HandleStream::AddRef()
{
	ULONG refCount = InterlockedIncrement(&ref);
	return refCount;
}

/**************************************************************************
*  HandleStream::Release
*/
ULONG WINAPI HandleStream::Release()
{
	ULONG refCount = InterlockedDecrement(&ref);

	if (!refCount)
	{
		delete this;
	}

	return refCount;
}

/**************************************************************************
 * HandleStream::Read
 */
HRESULT WINAPI HandleStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
	DWORD dwRead = 0;

	if (!ReadFile(handle, pv, cb, &dwRead, NULL))
	{
		return S_FALSE;
	}
	if (pcbRead)
		*pcbRead = dwRead;
	return dwRead == cb ? S_OK : S_FALSE;
}

/**************************************************************************
 * HandleStream::Write
 */
HRESULT WINAPI HandleStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
{
	DWORD dwWritten = 0;

	if (!WriteFile(handle, pv, cb, &dwWritten, NULL))
		return HRESULT_FROM_WIN32(GetLastError());

	if (pcbWritten)
		*pcbWritten = dwWritten;
	return S_OK;
}

/**************************************************************************
 *  HandleStream::Seek
 */
HRESULT WINAPI HandleStream::Seek(LARGE_INTEGER dlibMove,
	DWORD dwOrigin, ULARGE_INTEGER* pNewPos)
{
	DWORD dwPos;

	Commit(0); /* If ever buffered, this will be needed */
	dwPos = SetFilePointer(handle, dlibMove.u.LowPart, NULL, dwOrigin);
	if (dwPos == INVALID_SET_FILE_POINTER)
		return HRESULT_FROM_WIN32(GetLastError());

	if (pNewPos)
	{
		pNewPos->u.HighPart = 0;
		pNewPos->u.LowPart = dwPos;
	}
	return S_OK;
}

/**************************************************************************
 * HandleStream::SetSize
 */
HRESULT WINAPI HandleStream::SetSize(ULARGE_INTEGER libNewSize)
{
	Commit(0); /* If ever buffered, this will be needed */
	if (!SetFilePointer(handle, libNewSize.LowPart, (PLONG)&libNewSize.HighPart, FILE_BEGIN))
		return E_FAIL;

	if (!SetEndOfFile(handle))
		return E_FAIL;

	return S_OK;
}

/**************************************************************************
 * HandleStream::CopyTo
 */
HRESULT WINAPI HandleStream::CopyTo(IStream* pstm, ULARGE_INTEGER cb,
	ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
{
	char copyBuff[1024];
	ULONGLONG ulSize;
	HRESULT hRet = S_OK;

	if (pcbRead)
		pcbRead->QuadPart = 0;
	if (pcbWritten)
		pcbWritten->QuadPart = 0;

	if (!pstm)
		return S_OK;

	Commit(0); /* If ever buffered, this will be needed */

	/* Copy data */
	ulSize = cb.QuadPart;
	while (ulSize)
	{
		ULONG ulLeft, ulRead, ulWritten;

		ulLeft = ULONG(ulSize > sizeof(copyBuff) ? sizeof(copyBuff) : ulSize);

		/* Read */
		hRet = Read(copyBuff, ulLeft, &ulRead);
		if (FAILED(hRet) || ulRead == 0)
			break;
		if (pcbRead)
			pcbRead->QuadPart += ulRead;

		/* Write */
		hRet = pstm->Write(copyBuff, ulRead, &ulWritten);
		if (pcbWritten)
			pcbWritten->QuadPart += ulWritten;
		if (FAILED(hRet) || ulWritten != ulLeft)
			break;

		ulSize -= ulLeft;
	}
	return hRet;
}

/**************************************************************************
 * HandleStream::Commit
 */
HRESULT WINAPI HandleStream::Commit(DWORD grfCommitFlags)
{
	/* Currently unbuffered: This function is not needed */
	return S_OK;
}

/**************************************************************************
 * HandleStream::Revert
 */
HRESULT WINAPI HandleStream::Revert()
{
	return E_NOTIMPL;
}

/*************************************************************************
 * HandleStream::Stat
 */
HRESULT WINAPI HandleStream::Stat(STATSTG* lpStat,
	DWORD grfStatFlag)
{
	BY_HANDLE_FILE_INFORMATION fi;

	if (!lpStat)
		return STG_E_INVALIDPOINTER;

	memset(&fi, 0, sizeof(fi));
	GetFileInformationByHandle(handle, &fi);

	lpStat->pwcsName = NULL;
	lpStat->type = 0;
	lpStat->cbSize.u.LowPart = fi.nFileSizeLow;
	lpStat->cbSize.u.HighPart = fi.nFileSizeHigh;
	lpStat->mtime = fi.ftLastWriteTime;
	lpStat->ctime = fi.ftCreationTime;
	lpStat->atime = fi.ftLastAccessTime;
	lpStat->grfMode = 0;
	lpStat->grfLocksSupported = 0;
	memcpy(&lpStat->clsid, &IID_IStream, sizeof(CLSID));
	lpStat->grfStateBits = 0;
	lpStat->reserved = 0;

	return S_OK;
}

/*************************************************************************
 * HandleStream::Clone
 */
HRESULT WINAPI HandleStream::Clone(IStream** ppstm)
{
	if (ppstm)
		*ppstm = NULL;
	return E_NOTIMPL;
}

HRESULT __stdcall HandleStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return E_NOTIMPL;
}

HRESULT __stdcall HandleStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return E_NOTIMPL;
}


void CreatePipeStream(IStream** ppStmRead, IStream** ppStmWrite, DWORD size)
{
	HANDLE hRead = NULL, hWrite = NULL;
	::CreatePipe(&hRead, &hWrite, NULL, size);
	CreateStreamFromHandle(hRead, ppStmRead);
	CreateStreamFromHandle(hWrite, ppStmWrite);
}