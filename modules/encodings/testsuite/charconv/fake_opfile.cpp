// Dummy OpFile for test suite

#include "core/pch.h"
#include <stdio.h>

OP_STATUS OpFile::Construct(const char *filename, OpFileFolder)
{
	f = fopen(filename, "rb");
	return f ? OpStatus::OK : OpStatus::ERR_FILE_NOT_FOUND;
}

bool OpFile::IsOpen()
{
	return f != NULL;
}

OP_STATUS OpFile::Read(void *p, OpFileLength len, OpFileLength *bytes_read)
{
	*bytes_read = fread(p, 1, len, f);
	return *bytes_read == len ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS OpFile::ReadBinByte(unsigned char &p)
{
	OpFileLength ignored;
	return Read(&p, 1, &ignored);
}

OP_STATUS OpFile::GetFilePos(OpFileLength &pos)
{
	pos = ftell(f);
	return OpStatus::OK;
}

OpFile::~OpFile()
{
	if (f) fclose(f);
}

void OpFile::SetFilePos(OpFileLength pos)
{
	fseek(f, static_cast<long>(pos), SEEK_SET);
}
