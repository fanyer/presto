#import <AppKit/AppKit.h>

@interface Controller : NSObject {
    IBOutlet NSTextView *crashLogView;
    IBOutlet NSTextView *outLogView;
    IBOutlet NSTextField *xmapTextField;

	bool hasReadXMap;
}

- (void)textDidChange:(NSNotification *)aNotification;
- (IBAction)selectXMAP:(id)sender;

@end

#define kMaxFunctions 100000
#define kMaxStack 32

/**
* The info from the xMAP file goes into this struct:
*/
struct FunctionInfo
{
	long startAddress;		// Start address
	long endAddress;		// End address (= start address of next function)
	char name[256];			// Name of function
};

struct CrashLogInfo
{
	long index;
	char binary[32];
	long hexAddress;
	char rest[128];
};

bool ReadXMapFile(FILE *fp);
bool DechipherCrashLog(const char* text, NSTextView* outLogView);