#ifndef _LOG_JENS_H_	// {
#define _LOG_JENS_H_

class LockTrack
{

private:

	unsigned short	oldCount;

public:

					LockTrack();
					~LockTrack();
};


char WoFlog(char *format, ...);
OSErr FSNew(char *filename, FSSpec *fss, short *refNum);
OSErr FSDump(short refNum, void *start, size_t length);
//OSErr FSDumpToFile(Socket *socket, void *start, size_t length);

OSErr FSDumpBin(short refNum, void *start, size_t length);
//OSErr FSDumpBinToFile(Socket *socket, void *start, size_t length);

#endif	// } _LOG_JENS_H_
