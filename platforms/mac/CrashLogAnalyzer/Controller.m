#import "Controller.h"

@implementation Controller

- (void)textDidChange:(NSNotification *)aNotification
{
	if(hasReadXMap)
	{
		NSString *crashLogText = [crashLogView string];
		if(crashLogText && [crashLogText length] > 0)
		{
			DechipherCrashLog([crashLogText cString], outLogView);
		}
	}
}

- (IBAction)selectXMAP:(id)sender
{
	NSOpenPanel* panel = [NSOpenPanel openPanel];
	if ([panel runModalForDirectory:nil file:nil types:nil] == NSOKButton)
	{
		NSString *filename = [[panel filenames] objectAtIndex:0];
		if(filename)
		{
			FILE* fp = fopen([filename UTF8String], "r");
			if(fp)
			{
				if(ReadXMapFile(fp))
				{
					[xmapTextField setStringValue:filename];
					hasReadXMap = true;
					[crashLogView setEditable:TRUE];
				}

				fclose(fp);
			}
		}
	}
}

@end

// The crashlog stack
struct CrashLogInfo	gCrashStack[kMaxStack];
int 			gNumStack;					// Number of items on the stack right now

// A bunch of functions that we read from the xMAP file.
struct FunctionInfo 	gFunctions[kMaxFunctions];
long 			gNumFunctions;				// Number of functions that have been put into the gFunctions array

bool ReadXMapFile(FILE *fp)
{
	bool foundStart = false;
	int assignedValues = 0;
	char buffer[256];
	long hexAddress;
	char linebuffer[1024];
	char type[32];

	gNumFunctions = 0;

	while ( fgets(linebuffer, 1024, fp) != NULL)
    {
    	// Read past the header junk
    	if(strstr(linebuffer, "section[1]"))
    	{
    		// Ah, there we go.
    		foundStart = true;
    		continue;
    	}
        if(foundStart && (3 == sscanf(linebuffer, "%x %s %s", &hexAddress, type, buffer)))
        {
        	gFunctions[gNumFunctions].startAddress = hexAddress;
			gFunctions[gNumFunctions].endAddress = 0;
			if(gNumFunctions >= 1)
			{
				gFunctions[gNumFunctions-1].endAddress = hexAddress;
			}
			strcpy(gFunctions[gNumFunctions].name, buffer);

			if(gNumFunctions == (kMaxFunctions-1))
				return false;

			gNumFunctions++;
        }
    }

    return (gNumFunctions > 0);
}

bool DechipherCrashLog(const char* text, NSTextView* outLogView)
{
	bool foundStart = false;
	int i = 0;
	gNumStack = 0;
	const char* ptr = text;
	size_t len = 0;

	if(!text)
		return false;

	len = strlen(text);
	NSMutableString *str = [NSMutableString stringWithCapacity:len];

	while(ptr && *ptr)
	{
		while(text[i] != '\n' && text[i] != '\r' && text[i] != 0)
		{
			i++;
		}

		text[i] = 0;

		int foundNum = sscanf(	ptr, "%d %31c %x %127c",
								&gCrashStack[gNumStack].index,
								gCrashStack[gNumStack].binary,
								&gCrashStack[gNumStack].hexAddress,
								gCrashStack[gNumStack].rest);

    	if(4 == foundNum)
    	{
    		if(gNumStack == (kMaxStack-1))
    		{
				return false;
    		}

			gNumStack++;
    	}

		i++;
		ptr = &text[i];
	}

	if(gNumStack)
	{
		int j = 0;
		int k = 0;

		for(j = 0; j < gNumStack; j++)
		{
			for(k = 0; k < gNumFunctions; k++)
			{
				if(	gFunctions[k].startAddress > gCrashStack[j].hexAddress )
				{
					[str appendFormat:@"%-4d 0x%x - 0x%x: %s\n", j, gFunctions[k-1].startAddress, gFunctions[k-1].endAddress, gFunctions[k-1].name];
					break;
				}
			}
		}

		[str appendString:@"\n"];

		[outLogView setString:str];
	}

	return (gNumStack > 0);
}

