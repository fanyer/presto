#import <Cocoa/Cocoa.h>

#include "platforms/mac/spellcheck/spellcheck_lib.h"

void StartupSpelling()
{
	NSApplicationLoad();
}

void* StartSession()
{
	NSAutoreleasePool * pool = [NSAutoreleasePool new];
	void* session = (void*)[NSSpellChecker uniqueSpellDocumentTag];
	[pool release];
	return session;
}

void EndSession(void* session)
{
	NSAutoreleasePool * pool = [NSAutoreleasePool new];
	[[NSSpellChecker sharedSpellChecker] closeSpellDocumentWithTag:(int)session];
	[pool release];
}

int CheckSpelling(const unsigned short* text, int len, int* err_start, int* err_length)
{
	NSAutoreleasePool * pool = [NSAutoreleasePool new];
	NSString * str = [[NSString alloc] initWithCharacters:text length:len];
	NSRange range = [[NSSpellChecker sharedSpellChecker] checkSpellingOfString:str startingAt:0];
	[str release];
	[pool release];

	if (err_start)
		* err_start = range.location;
	if (err_length)
		* err_length = range.length;
	return 0;
}

unsigned short** SuggestAlternativesForWord(const unsigned short* text, int len)
{
	unsigned short** alternatives = NULL;
	int i = 0;
	int alt_count = 0;
	NSAutoreleasePool * pool = [NSAutoreleasePool new];
	NSString * str = [[NSString alloc] initWithCharacters:text length:len];
	NSArray *array = [[[NSSpellChecker sharedSpellChecker] guessesForWord:str] retain];
	[str release];
	alt_count = [array count];
	alternatives = (unsigned short**)AllocMem(sizeof(unsigned short*) * (alt_count + 1));
	for (i = 0; i < alt_count; i++)
	{
		NSString * str = (NSString*) [array objectAtIndex:i];
		int str_len = [str length];
		alternatives[i] = (unsigned short*)AllocMem(sizeof(unsigned short) * (str_len + 1));
		[str getCharacters:alternatives[i]];
		alternatives[i][str_len] = 0;
	}
	[array release];
	[pool release];

	alternatives[alt_count] = 0;
	return alternatives;
}

char* GetLanguage()
{
	char* ret_lang = 0;
	NSAutoreleasePool * pool = [NSAutoreleasePool new];
	NSString * lang = [[[NSSpellChecker sharedSpellChecker] language] retain];
	const char* utf8lang = [lang UTF8String];
	ret_lang = AllocMem(strlen(utf8lang) + 1);
	if (ret_lang) {
		strcpy(ret_lang, utf8lang);
	}
	[lang release];
	[pool release];

	return ret_lang;
}
