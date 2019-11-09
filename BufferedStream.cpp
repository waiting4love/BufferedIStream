#include <Windows.h>
#include <Ole2.h>
#include "BufferedStream.h"

BufferedStream::BufferedStream(IStream* stm, ULONG bufferSize)
	:m_nRef(1), m_pStm(stm), m_nBufferSize(bufferSize)
{
	m_nStmPos = 0;
	m_nBufStartPos = 0;
	m_nBufEndPos = 0;
	m_bModified = false;
	m_pStm->AddRef();
	m_Buffer = new char[bufferSize];
	m_bBuffered = true;
	SyncBuffer(true);
}

HRESULT BufferedStream::SyncBuffer(bool reRead)
{
	HRESULT hres = S_OK;
	// 如果有改动，就写入
	if (m_bModified) {
		ULARGE_INTEGER tmp1;
		ULONG tmp2;
		hres = m_pStm->Seek(*(PLARGE_INTEGER)&m_nBufStartPos, STREAM_SEEK_SET, &tmp1);
		if (FAILED(hres)) return hres;
		hres = m_pStm->Write(m_Buffer, ULONG(m_nBufEndPos - m_nBufStartPos), &tmp2);
		if (FAILED(hres)) return hres;
		m_bModified = false;
	}
	// 按参数决定读入数据到内存
	if (reRead) {
		ULONG read = 0;
		hres = m_pStm->Seek(*(PLARGE_INTEGER)&m_nStmPos, STREAM_SEEK_SET, (PULARGE_INTEGER)&m_nBufStartPos);
		if (FAILED(hres)) return hres;
		hres = m_pStm->Read(m_Buffer, m_nBufferSize, &read);
		if (FAILED(hres)) return hres;
		m_nBufEndPos = m_nBufStartPos + read;
	}
	else {
		ULARGE_INTEGER tmp;
		hres = m_pStm->Seek(*(PLARGE_INTEGER)&m_nStmPos, STREAM_SEEK_SET, &tmp);
		if (FAILED(hres)) return hres;
		m_nBufEndPos = m_nBufStartPos;
	}
	return hres;
}

BufferedStream::~BufferedStream()
{
	SyncBuffer(false);
	delete[]m_Buffer;
	m_pStm->Release();
}

void BufferedStream::Create(IStream* pstmSource, IStream** ppstmBuffered, ULONG bufferSize)
{
	*ppstmBuffered = new BufferedStream(pstmSource, bufferSize);
}

HRESULT __stdcall BufferedStream::QueryInterface(REFIID riid, void** ppvObject)
{
	if (riid == IID_IUnknown || riid == IID_IStream || riid == IID_ISequentialStream) {
		*ppvObject = this;
		AddRef();
		return S_OK;
	}
	return E_NOINTERFACE;
}

ULONG __stdcall BufferedStream::AddRef(void)
{
	return InterlockedIncrement(&m_nRef);
}

ULONG __stdcall BufferedStream::Release(void)
{
	auto x = InterlockedDecrement(&m_nRef);
	if ( x == 0)
	{
		delete this;
	}
	return x;
}

HRESULT __stdcall BufferedStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
	if (pcbRead) *pcbRead = 0;
	ULONG result = 0;

	HRESULT hres = S_OK;

	// 太大，放弃缓存
	if (cb >= m_nBufferSize) {
		hres = SyncBuffer(false);
		if (FAILED(hres)) return hres;
		hres = m_pStm->Read(pv, cb, &result);
		if (FAILED(hres)) return hres;
	}
	else {
		// 如果当前缓存不包含，则读入缓存
		if (m_nBufStartPos > m_nStmPos || m_nStmPos + cb > m_nBufEndPos) {
			hres = SyncBuffer(true);
			if (FAILED(hres)) return hres;
		}

		// 得到读出的数据量
		result = cb < ULONG(m_nBufEndPos - m_nStmPos)? cb : ULONG(m_nBufEndPos - m_nStmPos);
		
		// 输出的缓存对应位置
		auto pSrc = m_Buffer + (m_nStmPos - m_nBufStartPos);
		// 写入，考虑对齐
		switch (result)
		{
			case sizeof(BYTE) :
				*(BYTE*)pv = *(BYTE*)pSrc;
				break;
			case sizeof(WORD) :
				*(WORD*)pv = *(WORD*)pSrc;
				break;
			case sizeof(DWORD) :
				*(DWORD*)pv = *(DWORD*)pSrc;
				break;
			case sizeof(ULONGLONG):
				*(ULONGLONG*)pv = *(ULONGLONG*)pSrc;
				break;
			default:
				memcpy(pv, pSrc, result);
				break;
		}
	}
	m_nStmPos += result;
	if (pcbRead) *pcbRead = result;
	return hres;
}

HRESULT __stdcall BufferedStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
{
	if (pcbWritten) *pcbWritten = 0;
	HRESULT hres = S_OK;
	ULONG result = 0;
	if (cb >= m_nBufferSize) {
		hres = SyncBuffer(false);
		if (FAILED(hres)) return hres;
		hres = m_pStm->Write(pv, cb, &result);
		if (FAILED(hres)) return hres;
		m_nStmPos += result;
	}
	else {
		if (m_nBufStartPos > m_nStmPos || m_nStmPos + cb > m_nBufStartPos + m_nBufferSize) {
			hres = SyncBuffer(true);
			if (FAILED(hres)) return hres;
		}
		result = cb;
		auto pDest = m_Buffer + (m_nStmPos - m_nBufStartPos);
		// 写入，考虑对齐
		switch (result)
		{
			case sizeof(BYTE) :
				*(BYTE*)pDest = *(BYTE*)pv;
				break;
			case sizeof(WORD) :
				*(WORD*)pDest = *(WORD*)pv;
				break;
			case sizeof(DWORD) :
				*(DWORD*)pDest = *(DWORD*)pv;
				break;
			case sizeof(ULONGLONG) :
				*(ULONGLONG*)pDest = *(ULONGLONG*)pv;
				break;
			default:
				memcpy(pDest, pv, result);
				break;
		}
		m_bModified = true;
		m_nStmPos += result;
		if (m_nStmPos > m_nBufEndPos) m_nBufEndPos = m_nStmPos;
	}
	return hres;
}

HRESULT __stdcall BufferedStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
{
	HRESULT hres = S_OK;
	if (!m_bBuffered) {
		hres = m_pStm->Seek(dlibMove, dwOrigin, (ULARGE_INTEGER*)&m_nStmPos);
	}
	else {
		switch (dwOrigin)
		{
		case STREAM_SEEK_SET:
			if ((ULONGLONG)dlibMove.QuadPart < m_nBufStartPos || (ULONGLONG)dlibMove.QuadPart > m_nBufEndPos)
				hres = SyncBuffer(false);
			if(SUCCEEDED(hres)) m_nStmPos = (ULONGLONG)dlibMove.QuadPart;
			break;
		case STREAM_SEEK_CUR:
			if (m_nStmPos + dlibMove.QuadPart < m_nBufStartPos || m_nStmPos + dlibMove.QuadPart > m_nBufEndPos)
				hres = SyncBuffer(false);
			if (SUCCEEDED(hres)) m_nStmPos += dlibMove.QuadPart;
			break;
		case STREAM_SEEK_END:
			SyncBuffer(false);
			hres = m_pStm->Seek(dlibMove, dwOrigin, (ULARGE_INTEGER*)&m_nStmPos);
			break;
		default:
			break;
		}
	}
	if (plibNewPosition) plibNewPosition->QuadPart = m_nStmPos;
	return hres;
}

HRESULT __stdcall BufferedStream::SetSize(ULARGE_INTEGER libNewSize)
{
	HRESULT hres = S_OK;
	if (libNewSize.QuadPart < m_nBufEndPos)
	{
		hres = SyncBuffer(false);
	}
	if (SUCCEEDED(hres))
	{
		m_bBuffered = false;
		hres = m_pStm->SetSize(libNewSize);
		m_bBuffered = true;
	}
	return hres;
}

HRESULT __stdcall BufferedStream::CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
{
	// 以4K为单位复制，可以利用到缓存机制
	const ULONG BUFSIZE = 4096;
	char buff[BUFSIZE];
	ULONGLONG total_read = 0, total_written = 0;
	while (total_read < cb.QuadPart)
	{
		ULONG cur_read = 0, cur_written = 0;
		ULONG rcb = (cb.QuadPart - total_read) < BUFSIZE ? ULONG(cb.QuadPart - total_read) : BUFSIZE;
		
		if(FAILED(Read(buff, rcb, &cur_read))) break;
		if (cur_read == 0) break;
		total_read += cur_read;

		if(FAILED(pstm->Write(buff, cur_read, &cur_written))) break;
		if (cur_written == 0) break;
		total_written += cur_written;
	}

	if (pcbRead) pcbRead->QuadPart = total_read;
	if (pcbWritten) pcbWritten->QuadPart = total_written;

	return S_OK;
}

HRESULT __stdcall BufferedStream::Commit(DWORD grfCommitFlags)
{
	HRESULT hres = SyncBuffer(false);
	if (FAILED(hres)) return hres;
	return m_pStm->Commit(grfCommitFlags);
}

HRESULT __stdcall BufferedStream::Revert(void)
{
	HRESULT hres = m_pStm->Revert();
	if (SUCCEEDED(hres))
	{
		m_bModified = false;
		hres = SyncBuffer(false);
	}
	return hres;
}

HRESULT __stdcall BufferedStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return m_pStm->LockRegion(libOffset, cb, dwLockType);
}

HRESULT __stdcall BufferedStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return m_pStm->UnlockRegion(libOffset, cb, dwLockType);
}

HRESULT __stdcall BufferedStream::Stat(STATSTG* pstatstg, DWORD grfStatFlag)
{
	return m_pStm->Stat(pstatstg, grfStatFlag);
}

HRESULT __stdcall BufferedStream::Clone(IStream** ppstm)
{
	IStream* pstm = NULL;
	HRESULT hres = m_pStm->Clone(&pstm);
	if (FAILED(hres)) return hres;
	Create(pstm, ppstm, m_nBufferSize);
	pstm->Release();
	return S_OK;
}
