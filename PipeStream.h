#pragma once

void CreatePipeStream(IStream** ppStmRead, IStream** ppStmWrite, DWORD size = 0);
void CreateStreamFromHandle(HANDLE handle, IStream** ppStm);