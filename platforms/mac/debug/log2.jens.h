#ifndef _LOG_2_JENS_H_
#define _LOG_2_JENS_H_
#include <stdio.h>

class Log
{

private:
	bool		enabled;
	FILE		*fp;
	char		name[64];
	char		*spacePtr;
	Ptr			notifierLog[2];					// flip-flop buffer
	SInt8		currLog;						// which buffer is active

public:
				Log(char *filename);
				~Log();
	int			Open();
	void		Close();
	void		PutI(int id);
	void		PutO(int id);
	void		Put(char *format, ...);
	inline void	IncIndent(){ spacePtr--; };		// yes it sounds odd.
	inline void	DecIndent(){ spacePtr++; };
};

extern char		*gSpaces;
#ifdef _MAC_DEBUG
extern SInt32	gNotifierCount;
#endif
#endif
