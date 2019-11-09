// BufferedIStream.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <tchar.h>
#include <Windows.h>
#include <Ole2.h>
#include <Shlwapi.h>
#include <atlbase.h>
#include <iostream>
#include "BufferedStream.h"

#include <chrono>

int main()
{
	if (SUCCEEDED(::OleInitialize(NULL)))
	{
		auto file1 = _T(R"(C:\Qt\Qt5.12.2\InstallationLog.txt)");
		auto file2 = _T(R"(C:\usr\tmp\tmp.txt)");
		auto file = file2;
		// TEST BUFFERED
		{
			std::cout << "Buffered===================" << std::endl;
			CComPtr<IStream> stmBuffered;
			{
				CComPtr<IStream> stm;
				SHCreateStreamOnFile(file, STGM_READ, &stm);
				BufferedStream::Create(stm, &stmBuffered);
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
	}
	::OleUninitialize();
	return 0;
}
