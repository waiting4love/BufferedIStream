# StreamFun

## BufferedStream

参考Delphi的TBufferedFileStream实现，做了IStream的Buffered版本。

对文件小块连续读写性能有很大提升，对于大块读写和随机读写帮助不大。

函数：

```c++
void CreateBufferedStream(IStream* pstmSource, IStream** ppstmBuffered, ULONG bufferSize = 32 * 1024);
```

用法：

```c++
#include "BufferedStream.h"

CComPtr<IStream> stmBuffered;
{
	CComPtr<IStream> stm;
	SHCreateStreamOnFile(file, STGM_READ, &stm);
	CreateBufferedStream(stm, &stmBuffered);
}

//...正常使用stmBuffered 
```



## HandleStream

参考[reactos](https://github.com/reactos/reactos)项目里的代码做的IStream实现，从HANDLE生成IStream。

函数：

```c++
void CreateStreamFromHandle(HANDLE handle, IStream** ppStm);
```

## PipeStream

PipeStream成对产生，一个只读一个只写。向只写的IStream写入数据后，可以从只读的IStream中读出数据。读取时如果数据不足会等待直到数据数量满足或只写流被关闭。同样写数据时如果缓存满时会等待直到被读取。

函数：

```c++
void CreatePipeStream(IStream** ppStmRead, IStream** ppStmWrite, DWORD size = 0);
```

>  参数 size 表示缓存大小，设为0时表示按系统默认大小。

用法：

```c++
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
```

