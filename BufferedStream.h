#pragma once
void CreateBufferedStream(IStream* pstmSource, IStream** ppstmBuffered, ULONG bufferSize = 32 * 1024);