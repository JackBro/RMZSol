#include "Driver.h"

typedef struct _BINARYWRITER
{
	UINT64 currentPosition;
	PUINT8 buffer;
	UINT64 bufferSize;
} BINARYWRITER, *PBINARYWRITER;

void RmzBwInit(PBINARYWRITER bw, PVOID buffer, UINT32 size);
void RmzBwWriteUInt64(PBINARYWRITER bw, UINT64 val);
void RmzBwWriteUInt32(PBINARYWRITER bw, UINT32 val);
void RmzBwWriteUInt16(PBINARYWRITER bw, UINT16 val);
void RmzBwWriteUInt8(PBINARYWRITER bw, UINT8 val);
void RmzBwWriteBuffer(PBINARYWRITER bw, UINT64 length, PVOID source);
