typedef int OP_STATUS;
typedef unsigned short uni_char;
#include "adjunct/spellcheck/spellcheckAPI.h"
#include "platforms/mac/spellcheck/spellcheck_lib.h"
#include "platforms/mac/util/systemcapabilities.h"

#include <Carbon/Carbon.h>

extern "C" {
int get_spellcheck_glue(SpellcheckExternalAPI*& outSpellCheck);
int delete_spellcheck_glue();
}

class SpellcheckLibAPI : public SpellcheckExternalAPI
{

public:
	SpellcheckLibAPI() { m_internal_api_ptr = 0; m_default_language_utf8 = 0; m_default_encoding_utf8 = 0; }
	~SpellcheckLibAPI() { Terminate(); }

public:
	//Functions called at startup/exit of Opera
	virtual SPELL_STATUS Init(/*IN*/ const SpellcheckInternalAPI* internal_api_ptr,
							  /*IN*/ const char* default_language_utf8,
							  /*IN*/ const char* default_encoding_utf8,
							  /*IN*/ const char* default_extra_parameters_utf8=NULL //Parse and use it as you like
							  )
								{
									SInt32 macOSVersion;
									OSStatus err = Gestalt(gestaltSystemVersion, &macOSVersion);
									if ((err == noErr) && (macOSVersion >= 0x1020))
									{
										m_internal_api_ptr = internal_api_ptr;
										m_default_language_utf8 = default_language_utf8;
										m_default_encoding_utf8 = default_encoding_utf8;
										::StartupSpelling();
										return SpellStatus::OK;
									}
									else
										return SpellStatus::ERR;
								}

	virtual SPELL_STATUS Terminate() { return SpellStatus::OK; }


	//Autodetect supported features
	virtual bool		 SupportFeature(SPELL_FEATURE feature) const {
							switch (feature)
							{
								case SpellFeature::ADD_WORD:
								{
									SInt32 macOSVersion;
									OSStatus err = Gestalt(gestaltSystemVersion, &macOSVersion);
									if (err == noErr && macOSVersion >= 0x1030)
										return true;
								}
							}
							return false; }


	//Language
	virtual SPELL_STATUS AddLanguage(/*IN*/ const char* language_utf8)
	{
		return SpellStatus::OK;
	}

	virtual SPELL_STATUS DeleteLanguage(/*IN*/ const char* language_utf8)
	{
		return SpellStatus::OK;
	}

	virtual SPELL_STATUS GetLanguageList(/*OUT*/ char**& language_list_utf8)
	{
		language_list_utf8 = (char**)m_internal_api_ptr->AllocMem((2) * sizeof(char*));
		if (language_list_utf8)
		{
			language_list_utf8[0] = ::GetLanguage();
			language_list_utf8[1] = 0;
		}
		return SpellStatus::OK;
	}

	virtual SPELL_STATUS GetCurrentLanguage(/*OUT*/ char*& language_utf8)
	{

		language_utf8 = ::GetLanguage();
		return SpellStatus::OK;
	}


	//Functions called once for every text spellchecked
	virtual SPELL_STATUS StartSession(/*OUT*/ SpellcheckSession*& session, //Return a pointer to a session-object or something to uniquely identify this session
									  /*OUT*/ uni_char*& error_text, //If starting session failed, please give an informative text here. Will be freed by Opera
									  /*IN*/ const char* language_utf8=NULL, //Language to use for this session (NULL will use the default, given in Init())
									  /*IN*/ const char* encoding_utf8=NULL, //Encoding to use for this session (NULL will use the default, given in Init())
									  /*IN*/ const char* extra_parameters_utf8=NULL //Extra parameters. (NULL will use the default, given in Init())
									  )
										{
											session = ::StartSession();
											error_text = 0;
											return SpellStatus::OK;
										}

	virtual SPELL_STATUS EndSession(const SpellcheckSession* session)
										{
											::EndSession((void*) session);
											return SpellStatus::OK;
										}


	//The spell check itself
	virtual SPELL_STATUS CheckText(/*IN*/ const SpellcheckSession* session,
								   /*IN*/ const uni_char* text_start,
								   /*IN*/ int text_length,
								   /*IN/OUT*/ int& last_misspelled_pos, //This will be -1 first time function is called, and MUST be set to -1 when the whole text is checked
								   /*OUT*/ int& misspelled_length,
								   /*OUT*/ uni_char**& replacement_text //Array of AllocMem(...)-allocated strings (will be freed by Opera). Last item must be set to NULL
								   )
								   	{
//								   		last_misspelled_pos++;
								   		int start, len;
								   		if (last_misspelled_pos < 0)
								   			last_misspelled_pos = 0;
								   		int err = ::CheckSpelling(text_start + last_misspelled_pos, text_length - last_misspelled_pos, &start, &len);
								   		if (err == 0) {
									   		if (len > 0) {
									   			last_misspelled_pos += start;
									   			misspelled_length = len;
										   		replacement_text = ::SuggestAlternativesForWord(text_start + last_misspelled_pos, misspelled_length);
										   	} else {
									   			last_misspelled_pos = -1;
									   			misspelled_length = 0;
									   			replacement_text = 0;
									   		}
									   		return SpellStatus::OK;
									   	} else {
								   			last_misspelled_pos = -1;
								   			misspelled_length = 0;
								   			replacement_text = 0;
									   		return SpellStatus::ERR;
									   	}
								   	}

	virtual SPELL_STATUS CheckWord(/*IN*/ const SpellcheckSession* session,
								   /*IN*/ const uni_char* word_start,
								   /*IN*/ int word_length, //Can be -1 if the word is zero-terminated
								   /*OUT*/ bool& correctly_spelt,
								   /*OUT*/ uni_char**& replacement_words //Array of AllocMem(...)-allocated strings (will be freed by Opera). Last item must be set to NULL
								   )
								   	{
								   		int start, len;
								   		int err = ::CheckSpelling(word_start, word_length, &start, &len);
								   		if (err == 0) {
									   		if (len > 0) {
									   			correctly_spelt = false;
										   		replacement_words = ::SuggestAlternativesForWord(word_start + start, len);
										   	}
									   		else
									   			correctly_spelt = true;
									   		return SpellStatus::OK;
									   	} else {
									   		return SpellStatus::ERR;
									   	}
								   	}

	virtual SPELL_STATUS AddWord(/*IN*/ const SpellcheckSession* session,
								 /*IN*/ const uni_char* new_word,
								 /*IN*/ int new_word_length
								 )  {
									SInt32 macOSVersion;
									OSStatus err = Gestalt(gestaltSystemVersion, &macOSVersion);
									if (err == noErr && macOSVersion >= 0x1030)
										return SpellStatus::ERR;
									else
									{
										static HIViewRef sTextView = NULL;
										static TXNObject sTextObject = NULL;
										if (!sTextView) {
											HIRect bounds = {{0,0},{0,0}};
											HITextViewCreate(&bounds, 0, 0, &sTextView);
											if (sTextView) {
												sTextObject = HITextViewGetTXNObject(sTextView);
												if (sTextObject) {
													TXNSetCommandEventSupport(sTextObject, kTXNSupportSpellCheckCommandProcessing);
												}
											}
										}
										if (sTextObject) {
											TXNSetData(sTextObject, kTXNUnicodeTextData, new_word, new_word_length*sizeof(uni_char), kTXNStartOffset, kTXNEndOffset);
											TXNSetSelection(sTextObject, kTXNStartOffset, kTXNEndOffset);
											EventRef event = NULL;
											MacCreateEvent(kCFAllocatorDefault, kEventClassCommand, kEventCommandProcess, 0, 0, &event);
											if (event)
											{
												HICommand command;
												command.attributes = 0;
												command.commandID = kHICommandLearnWord;
												SetEventParameter(event, kEventParamDirectObject, typeHICommand, sizeof(command), &command);
												SendEventToHIObject(event, (HIObjectRef)sTextView);
												ReleaseEvent(event);
											}
										}
										return SpellStatus::OK;
									 }
								 }

	virtual SPELL_STATUS DeleteWord(/*IN*/ const SpellcheckSession* session,
									/*IN*/ const uni_char* new_word,
									/*IN*/ int new_word_length) { return SpellStatus::ERR; }

	virtual SPELL_STATUS AddReplacement(/*IN*/ const SpellcheckSession* session,
										/*IN*/ const uni_char* wrong_word,
										/*IN*/ int wrong_word_length,
										/*IN*/ const uni_char* replacement_word,
										/*IN*/ int replacement_word_length
										) { return SpellStatus::ERR; }

	const SpellcheckInternalAPI* GetInternalAPI() { return m_internal_api_ptr; }

private:
	const SpellcheckInternalAPI* m_internal_api_ptr;
	const char* m_default_language_utf8;
	const char* m_default_encoding_utf8;
};


SpellcheckLibAPI *gSpellCheckAPI = 0;


extern "C" {

void * AllocMem(int len)
{
	if (gSpellCheckAPI)
	{
		const SpellcheckInternalAPI* host = gSpellCheckAPI->GetInternalAPI();
		if (host)
		{
			return host->AllocMem(len);
		}
	}
	return NULL;
}

void Free(void * buf)
{
	if (gSpellCheckAPI)
	{
		const SpellcheckInternalAPI* host = gSpellCheckAPI->GetInternalAPI();
		if (host)
		{
			host->FreeMem(buf);
		}
	}
}

#ifdef __MWERKS__
#pragma export on
#endif

int get_spellcheck_glue(SpellcheckExternalAPI*& outSpellCheck)
{
	if (!gSpellCheckAPI)
		gSpellCheckAPI = new SpellcheckLibAPI();
	outSpellCheck = gSpellCheckAPI;
	return SpellStatus::OK;
}

int delete_spellcheck_glue()
{
	return SpellStatus::OK;
}

#ifdef __MWERKS__
#pragma export off
#endif

} // extern "C"

