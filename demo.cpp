#include <tchar.h>
#include <Windows.h>
#include <Ole2.h>
#include <Shlwapi.h>
#include "BufferedStream.h"
#include "PipeStream.h"

#include <iostream>
#include <atlbase.h>
#include <thread>
#include <chrono>

auto file1 = _T(R"(C:\Qt\Qt5.12.2\InstallationLog.txt)");
auto file2 = _T(R"(C:\usr\tmp\tmp.txt)");
auto file = file1;

void TestBuffered()
{
	std::cout << "Buffered===================" << std::endl;
	CComPtr<IStream> stmBuffered;
	{
		CComPtr<IStream> stm;
		SHCreateStreamOnFile(file, STGM_READ, &stm);
		CreateBufferedStream(stm, &stmBuffered);
	}

	STATSTG stg = { 0 };
	stmBuffered->Stat(&stg, STATFLAG_NONAME);
	std::cout << "Size:" << stg.cbSize.QuadPart << std::endl;

	auto start = std::chrono::high_resolution_clock::now();
	ULONG total = 0;
	while (true)
	{
		BYTE c;
		ULONG r = 0;
		if (FAILED(stmBuffered->Read(&c, 1, &r))) break;
		if (r == 0) break;
		total += r;
	}
	std::chrono::duration<double> dr = std::chrono::high_resolution_clock::now() - start;

	std::cout << "Read Bytes:" << total << std::endl;
	std::cout << "speed(s):" << dr.count() << std::endl;
	std::cout << "Buffered^^^^^^^^^^^^^^^^" << std::endl;

}

// TEST NO BUFFERED
void TestNoBuffered()
{
	CComPtr<IStream> stm;
	SHCreateStreamOnFile(file, STGM_READ, &stm);

	STATSTG stg = { 0 };
	stm->Stat(&stg, STATFLAG_NONAME);
	std::cout << "Size:" << stg.cbSize.QuadPart << std::endl;

	auto start = std::chrono::high_resolution_clock::now();
	ULONG total = 0;
	while (true)
	{
		BYTE c;
		ULONG r = 0;
		HRESULT hres = stm->Read(&c, 1, &r);
		if (FAILED(hres)) break;
		if (r == 0) break;
		total += r;
	}
	std::chrono::duration<double> dr = std::chrono::high_resolution_clock::now() - start;

	std::cout << "Read Bytes:" << total << std::endl;
	std::cout << "speed(s):" << dr.count() << std::endl;
}

void TestPipe()
{
	// 建立两个IStream，一个负责写，一个负责读。
	CComPtr<IStream> stmRead, stmWrite;
	CreatePipeStream(&stmRead, &stmWrite);
	// 通过线程向只写流写数据
	std::thread wrt_th{ [](IStream* stmWrite) {
		CComPtr<IStream> stm;
		SHCreateStreamOnFile(file, STGM_READ, &stm);

		while (true)
		{
			BYTE c[128];
			ULONG r = 0;
			HRESULT hres = stm->Read(c, 128, &r);
			if (FAILED(hres)) break;
			if (r == 0) break;
			if (FAILED(stmWrite->Write(c, r, &r))) break;
		}

		stmWrite->Release();
	}, stmWrite.Detach() };

	// 从只读流中读取出数据
	while (true)
	{
		char c[12];
		ULONG r = 0;
		HRESULT hres = stmRead->Read(c, 12, &r);
		if (FAILED(hres)) break;
		if (r == 0) break;
		std::cout.write(c, r);
	}

	wrt_th.join();
}

int main()
{
	if (SUCCEEDED(::OleInitialize(NULL)))
	{
		TestPipe();
	}
	::OleUninitialize();
	return 0;
}
