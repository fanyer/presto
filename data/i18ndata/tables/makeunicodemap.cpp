// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int UINT32;

class CharSetFlags
{
private:
	UINT32 flags[2];

public:
	CharSetFlags()
		{
			flags[0] = flags[1] = 0;
		}

	CharSetFlags(UINT32 lo)
		{
			flags[0] = 0;
			flags[1] = lo;
		}

	CharSetFlags(UINT32 hi, UINT32 lo)
		{
			flags[0] = hi;
			flags[1] = lo;
		}

	CharSetFlags operator|(const CharSetFlags &f) const
		{
			return CharSetFlags(flags[0] | f.flags[0], flags[1] | f.flags[1]);
		}
};

// Must be makde using "./makexmapping.pl -c > fil.h"
#include "fil.h"


int main(void)
{
	FILE *fp = fopen("unicode.dat", "w" );
	if( !fp )
	{
		perror("Could not open file");
		exit(1);
	}

	int mapOffset = 0;
	for( int i=0; xcharsetmapping[i].xcharsetname; i++ )
	{
		mapOffset += strlen( xcharsetmapping[i].xcharsetname ) + 1;
		mapOffset += sizeof(CharSetFlags);
	}

	fwrite( &mapOffset, sizeof(UINT32), 1, fp );
	for( int i=0; xcharsetmapping[i].xcharsetname; i++ )
	{
		xcharsets &xc = xcharsetmapping[i];

		fwrite( xc.xcharsetname, strlen(xc.xcharsetname)+1, 1, fp );
		fwrite( &xc.mappingbit, sizeof(xc.mappingbit), 1, fp );
	}

	fwrite( charsets, sizeof(charsets), 1, fp );
	fclose(fp);
	return 0;
}





