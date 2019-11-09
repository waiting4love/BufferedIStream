# BufferedIStream

参考Delphi的TBufferedFileStream实现，做了IStream的Buffered版本。

对文件小块连续读写性能有很大提升，对于大块读写和随机读写帮助不大。

用法：调用静态方法生成IStream即可。

```c++
static void BufferedStream::Create(IStream* pstmSource, IStream** ppstmBuffered, ULONG bufferSize=32*1024);
```

见下例：

```c++
#include "BufferedStream.h"

CComPtr<IStream> stmBuffered;
{
	CComPtr<IStream> stm;
	SHCreateStreamOnFile(file, STGM_READ, &stm);
	BufferedStream::Create(stm, &stmBuffered);
}

//...正常使用stmBuffered 
```

