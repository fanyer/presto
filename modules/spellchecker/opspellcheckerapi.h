/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_SPELL_CHECKER_API_H
#define OP_SPELL_CHECKER_API_H

#include "modules/hardcore/opera/module.h"
#include "modules/util/simset.h"
#include "modules/util/opstrlst.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/spellchecker/src/opspellchecker.h"
#include "modules/windowcommander/OpWindowCommander.h"

#define SPC_DICTIONARY_LOADING_SLICE_MS 10 ///< Milliseconds for each time-slice of dictionary-loading.
#define SPC_SPELL_CHECKING_SLICE_NORMAL_MS 10 ///< Milliseconds for each time-slice of dictionary-lookup when SpellCheckPriority::NORMAL.
#define SPC_SPELL_CHECKING_SLICE_BACKGROUND_MS 10 ///< Milliseconds for each time-slice of dictionary-lookup when SpellCheckPriority::BACKGROUND.
#define SPC_DICTIONARY_LOADING_DELAY_MS 0 ///< Time between each dictionary-loading time-slice.
#define SPC_SPELL_CHECKING_DELAY_NORMAL_MS 0 ///< Time between each dictionary lookup time-slice when SpellCheckerPriority::NORMAL.
#define SPC_SPELL_CHECKING_DELAY_BACKGROUND_MS 20 ///< Time between each dictionary lookup time-slice when SpellCheckerPriority::BACKGROUND.
#define SPC_SPELL_UPDATE_MS 10 ///< Milliseconds for each time-slice of "background updating" (scan through all text after add/remove/ignore)
#define SPC_SPELL_UPDATE_DELAY_MS 0 ///< Time between each time-slice of "background updating" (scan through all text after add/remove/ignore)
#define SPC_REPLACEMENT_SEARCH_MAX_DEFAULT_MS 100 ///< Default maximum total time for a replace-suggestion search.

class OpSpellCheckerManager;
class OpSpellCheckerSession;
class OpSpellCheckerWordIterator;
class OpSpellCheckerListener;
class OpSpellCheckerLanguage;
class OpSpellUiSessionImpl;
class SpellUIHandler;
class OpSpellChecker;

/** This class encapsulates a "language", which more or less is an OpSpellChecker instance for the dictionary for that
	language. It also keeps a list of OpSpellCheckerSession objects using the dictionary. When there are no sessions left
	which uses the dictionary will OpSpellCheckerManager::OnLanguageHasNoSessions() be called. This function currently
	"caches" one dictionary which has no sessions (for avoiding loading the dictionary again). */
class OpSpellCheckerLanguage : public Link
{
public:
	OpSpellCheckerLanguage();
	~OpSpellCheckerLanguage();

	/** Returns the OpSpellChecker used by this language (might be NULL). */
	OpSpellChecker* GetSpellChecker() { return m_spell_checker; }
	
	/** Deletes the OpSpellChecker, if there are no sessions using it (will assert otherwise). */
	void DeleteSpellChecker();
	
	/** Return the "language string" for this language, that is now simply the filename of the dictionary-files without
		extension, such as "en_US" or "sv_SE". */
	const uni_char* GetLanguageString() { return (const uni_char*)m_language; }

#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
    /** Return the full language name for this language, this name is found in the
        Opera dictionary meta data file togheter with the version number. */
    const uni_char* GetLanguageName() { return (const uni_char*)m_language_name; }
#endif // WIC_SPELL_DICTIONARY_FULL_NAME
    
	/** Gets the version number of the dictionary used for this language.
	 *  The version number is read from the Opera dictionary meta data file.
	 *  Valid version numbers are integers greater than 0.
	 *
	 *  @return
	 *    The version number if it can be determined.
	 *    0 if no metadata file exists (e.g. the dictionary doesn't come from Opera).
	 *    -1 on failure;
	 */
	int GetVersion();

    /** Will open the license file if there is one, and copy the text into the string.
     *  If there is no license file, or it can't be read, the function will return an
     *  empty string.
     *
     *  @return
     *  @license[out] Should be a reference to an empthy string. It will return with the
     *                license text for this language, or en empty string if non is found.
     *                The string will be null terminated if found. 
     *
     *  @return
     *    It should be enough to check for sucsess or error. But if the path to the license
     *    can't be determined, the function will return ERR_FILE_NOT_FOUND explicitly.
     *                
     */
	OP_STATUS GetLicense(OpString& license);

	/** Returns the index for this language among the OpSpellCheckerManager's list of valid languages. Valid lanugages
		are languages that have at least a .dic file. Lanugages are only invalid in the special case that a dictionary
		has been loaded and still remains in memory, but the .dic file has been deleted (or renamed) - and the list of
		installed languages has been "refreshed". */
	int GetIndex() { return m_index; }
	

	/** Marks the language as available/unavailable. An unavailable language won't be used by new spellchecker sessions, but might remain in use by older sessions. */
	void SetAvailable(BOOL available);

	/** Checks if the language is still available from the same dictionary or not */
	BOOL IsAvailable();


	/** Compare the paths used by the dictionary to the arguments. Returns TRUE if they are all the same */
	BOOL SamePaths(const uni_char *dictionary_file_path, const uni_char *affix_file_path, const uni_char *own_file_path, const uni_char *metadata_file_path, const uni_char* license_file_path);

	/** Sets the paths to the dictionary-files.
		@param dictionary_file_path Absolute path to the .dic file, can be NULL but then will the dictionary be invalid.
		@param affix_file_path Absolute path to the .aff file, can be NULL.
		@param own_file_path Absolute path to the .oow file, can be NULL.
		@param own_file_path Absolute path to the metadata file, can be NULL. */
	OP_STATUS SetPaths(const uni_char *dictionary_file_path, const uni_char *affix_file_path, const uni_char *own_file_path, const uni_char* metadata_file_path, const uni_char* license_file_path);
	
	/** Sets the absolute path of the .oow file. This function is used for the special purpose of letting the OpSpellChecker
		set the .oow if it just has created it. */
	OP_STATUS SetOwnFilePath(const uni_char *own_file_path);
	
	/** Sets the language-string that should be returned by GetLanugageString(). */
	OP_STATUS SetLanguage(const uni_char *language);

#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
	/** Sets the language name that should be returned by GetLanugageName(). */
	OP_STATUS SetLanguageName();
#endif // WIC_SPELL_DICTIONARY_FULL_NAME
	
	/** If has_failed is set to TRUE, this should indicate that we've tried to load the dictionary, but that the loading
		of some reason failed. This is used for not setting dictionaries which can't be loaded as the default dictionary,
		this prevents the situation that the user enables spellcheck repeatly and each time is the default-dictionary
		selected for which the loading fails, so that spellchecking is disabled immediately. */
	void SetHasFailed(BOOL has_failed = TRUE) { m_has_failed = has_failed; }
	
	/** Returns TRUE if the loading of the dictinonary has failed, see SetHasFailed(). */
	BOOL GetHasFailed() { return m_has_failed; }

	/** This function starts or continues the loading of the dictionary.
		@param time_out Deadline for when this function must return back to the message-loop, 0.0 means -> return when finished.
		@param progress_continue Will be set to TRUE if the operation timed-out before the dictionary was fully loaded,
		In that case must this function be called again later in order to continue the loading. */
	void LoadDictionary(double time_out, BOOL &progress_continue);

	/** Adds an OpSpellCheckerSession that uses this dictionary. */
	void AddSession(OpSpellCheckerSession *session);
	
	/** Called when an OpSpellCheckerSession stops using the dictionary. If, after this, there are no sessions left and
		the dictionary has been fully loaded into memory - then will OpSpellCheckerManager::OnLanguageHasNoSessions() be
		called. */
	void RemoveSession(OpSpellCheckerSession *session);
	
	/** Returns the linked list of sessions that uses the dictionary. */
	Head* GetSessions() { return &m_sessions; }

	/** Returns true if any OpSpellCheckerSession objects uses this dictinonary. */
	BOOL HasSessions() { return !m_sessions.Empty(); }
	
	/** Returns TRUE if the paths to the dictionary-files are valid, this means just to check if the paths to the .dic and .aff
		files are "valid" (not NULL). */
	BOOL HasValidPaths() { return !!m_dic_path && !!m_affix_path; }
	
	/** Returns TRUE if this language has a dictionary that has been fully loaded. */
	BOOL HasDictionary() { return m_spell_checker && m_spell_checker->HasDictionary(); }
	
	/** Returns TRUE if this is the last language for which the dictinoary was successfully loaded. */
	BOOL IsLastUsedInPrefs();
	
	/** Set this language to be be the last for which the dictionary was successfully loaded. */
	OP_STATUS SetLastUsedInPrefs();

protected:
	
	/** Sets the index for this language among the OpSpellCheckerManager's list of valid lanugages, see GetIndex(). */
	void SetIndex(int index) { m_index = index; }

private:
	OpSpellChecker *m_spell_checker; ///< The OpSpellChecker with the dictionary for this language, can be NULL.
	BOOL m_has_failed; ///< TRUE if loading of the dictionary for this language has failed at the last occation.
	uni_char *m_language; ///< The language string, see GetLanugageString().
	uni_char *m_language_name; ///< The full language name, see GetLanguageName().
	int m_version; //< The dictionary version as specified in the Opera dicitionary metadata file.
	uni_char *m_affix_path; ///< The absolute path to the .aff file, can be NULL.
	uni_char *m_dic_path; ///< The absolute path to the .dic file, can be NULL - if the language is "invalid".
	uni_char *m_own_path; ///< The absolute path to the .oow file, can be NULL.
	uni_char *m_metadata_path; ///< The absolute path to the metadata file, can be NULL.
	uni_char *m_license_path; ///< The absolute path to the license file, can be NULL.
	Head m_sessions; ///< The linked list of OpSpellCheckerSession objects using this lanugage.
	int m_index; ///< The index among the OpSpellCheckerManager's list of valid languages, or -1 if the language is "invalid".
	BOOL m_available; ///< Whether the language is available (i.e. still on disk and not shadowed by another dictionary for the same language in another folder).

	friend class OpSpellCheckerManager;
};

/** A session object used by the user of a dictionary, or more pricisely - a OpSpellCheckerLanguage which in turn uses an
	OpSpellChecker object that contains the actual dictionary. This session object should be used as a "proxy" by the
	users of dictionaries, such as OpDocumentEdit and OpTextCollection. */
class OpSpellCheckerSession : public Link
{
public:
	
	/** The current state of the session. */
	enum SessionState
	{
		IDLE = 0, ///< No activity at the moment, at this moment can CheckText be used for spellchecking.
		LOADING_DICTIONARY, ///< The dictionary is currently loading, so we'll have to wait util the loading finishes.
		SPELL_CHECKING, ///< This session currently performs spellchecking.
		HAS_FAILED ///< This state indicates that an error has occured, either during dictionary-loading or during spellchecking.
	};

	/** The priority for spellchecking (dictionary lookup possibly including replacement-suggestion search). */
	enum SpellCheckPriority
	{
		NORMAL = 0, ///< The spellchecking time-slices and delays between these slices are defined by SPC_SPELL_CHECKING_SLICE_NORMAL_MS and SPC_SPELL_CHECKING_DELAY_NORMAL_MS.
		BACKGROUND	///< The spellchecking time-slices and delays between these slices are defined by SPC_SPELL_CHECKING_SLICE_BACKGROUND_MS and SPC_SPELL_CHECKING_DELAY_BACKGROUND_MS.
	};
	
	/** Constructor
		@param manager The OpSpellCheckerManager, in fact, this should always be the global OpSpellCheckerManager g_internal_spellcheck.
		@param language The OpSpellCheckerLanguage for this session.
		@param listener The OpSpellCheckerListener that should receive callbacks about events, such as when a misspelled
		word is found or when the loading of the dictionary has finished, see description of OpSpellCheckerListener.
		@param synchronous If TRUE will the session be synchronous. This means that loading and spellchecking will not
		be "sliced" using the message-loop, instead will loading and spellchecking be blocking operations.
		@param ignore_words This is sort of a "hack" for sharing the collection of ignored words between sessions. The
		collection will not be deleted until no session is using it anymore. The reason is that we might want more then
		one "identical" sessions - e.g. if spellchecking is performed an a large document and the user right-clicks on
		a word. Then we want to continue the spellchecking of the rest of the document while we make a "special spellcheck"
		of the clicked word. */
	OpSpellCheckerSession(OpSpellCheckerManager *manager, OpSpellCheckerLanguage *language, OpSpellCheckerListener *listener, BOOL synchronous, SortedStringCollection *ignore_words);
	~OpSpellCheckerSession();

	/** Returns the language string of the OpSpellCheckerLanguage for this session, see desctiption of
		OpSpellCheckerLanguge::GetLanguageString(). */
	const uni_char *GetLanguageString() { return m_language->GetLanguageString(); }
	
	/** Returns TRUE if the dictionary for this session consider ch to be a token that separates word from each other,
		such as whitespace (in general). */
	BOOL IsWordSeparator(uni_char ch) { return m_language->GetSpellChecker() && m_language->GetSpellChecker()->IsWordSeparator(ch); }
	
	/** Returns the collection of word that should be ignored even though they are misspelled. */
	SortedStringCollection *GetIgnoreWords() { return m_ignore_words; }
	
	/** Checks spelling of a text, the OpSpellCheckerListener for this session will receive callbacks for all events
		that occurs, such as when a misspelled word is found is - and when the checking has finished. See description
		of OpSpellCheckerListener.
		@param start The first word that should be checked, see description of the interface OpSpellCheckerWordIterator.
		It's ok if start is in some kind of "init-state" - as start->GetCurrentWord() is always allowed to return NULL
		or "". This will always howerver result in a callback to OpSpellCheckerListener::OnCorrectSpellingFound().
		@param find_replacements If TRUE will a replacement-suggestion search be performed for each misspelled word
		that is found and OpSpellCheckerListener::OnMisspellingFound will receive the replacement-suggestions. */
	OP_STATUS CheckText(OpSpellCheckerWordIterator *start, BOOL find_replacements);
	
	/** This function adds the word new_word to the dictionary. */
	OP_STATUS AddWord(const uni_char* new_word);
	
	/** This function adds the word new_word to the words that should be ignored, e.g. it shouldn't be considered
		misspelled even though it isn't in the dictionary. */
	OP_STATUS IgnoreWord(const uni_char* new_word);
	
	/** This function deletes the word new_word from the dictionary. */
	OP_STATUS DeleteWord(const uni_char* new_word);
	
	/** Returns TRUE if spellchecking can be performed at all on the current word, FALSE will be returned for very long
		words (>200 characters for 8-bit encoded dictionaries and >50 characters for UTF-8 dictionaries) and for numbers
		etc.*/
	BOOL CanSpellCheck(const uni_char *word) { return m_language->GetSpellChecker() && m_language->GetSpellChecker()->CanSpellCheck(word, word+uni_strlen(word)); }
	
	/** Returns TRUE if it's possible to add word to the dictionary, FALSE will be returned if CanSpellCheck() would 
		return FALSE or if the word contains character that can't be represented with the current dictionary-encoding. */
	BOOL CanAddWord(const uni_char *word) { return m_language->GetSpellChecker() && m_language->GetSpellChecker()->CanAddWord(word); }
	
	/** Sets the priority for spellchecking, see SpellCheckPriority. */
	void SetPriority(SpellCheckPriority priority) { m_priority = priority; }
	
	/** Returns the priority for spellchecking, see SpellCheckPriority. */
	SpellCheckPriority GetPriority() { return m_priority; }
	
	/** Returns the state of the session, see SessionState. */
	SessionState GetState() { return m_state; }
	
	/** If spellchecking is currently performed will this function stop the spellchecking. Notice that
		OpSpellCheckerListener::OnCheckingComplete will NOT be called due to this. */
	void StopCurrentSpellChecking();
	
	/** Set whether this session should be synchronous (blocking) or not, see description of the constructor. */
	void SetSynchronous(BOOL synchronous) { m_synchronous = synchronous; }
	
	/** Returns the OpSpellCheckerLanguage object that's used by this session. */
	OpSpellCheckerLanguage* GetLanguage() { return m_language; }

protected:
	
	/** Sets the session state, see description of SessionState. */
	void SetState(SessionState state) { m_state = state; }
	
	/** Continues the "progress" for this session. This is either the progress of loading the dictionary or the progress
		of spellchecking.
		@time_out Deadline for when the progress should stop and return back to the message-loop, this argument will be
		0.0 for synchronous sessions, meaning that the progress should continue until either the loading or the
		spellchecking has completed. */
	void Progress(double time_out, BOOL &progress_continue);
	
	/** Returns the OpSpellCheckerListener that receives events by this session, see description of OpSpellCheckerListener. */
	OpSpellCheckerListener* GetListener() { return m_listener; }
	
	/** Returns the word-iterator used for spellchecking, see description of OpSpelCheckerWordIterator and CheckText(). */
	OpSpellCheckerWordIterator* GetWordIterator() { return m_lookup_state.word_iterator; }
	
	/** Returns whether this session is synchronous (blocking) or not, see description of the constructor. */
	BOOL IsSynchronous() { return m_synchronous; }

private:
	/** Takes proper actions and returns TRUE if spellchecking has been aborted inside Progress(). */
	BOOL HandleAbortInProgress();

private:
	OpSpellCheckerLookupState m_lookup_state; ///< The "lookup-state", used for implementing "interruptable" replacement-suggestion search.
	OpSpellCheckerManager *m_manager; ///< The OpSpellCheckerManager that "controlls" all spellchecking, should allways be the global g_internal_spellcheck.
	OpSpellCheckerLanguage *m_language; ///< The OpSpellCheckerLanguage used by this session.
	OpSpellCheckerListener *m_listener; ///< The listener that is notified on spellchecking events.
	BOOL m_synchronous; ///< TRUE if this session is "synchronous" (blocking).
	SpellCheckPriority m_priority; ///< The priority for spellchecking (normal/background).
	SessionState m_state; ///< The current state of the session, see description of SessionState.
	BOOL m_find_replacements; ///< TRUE if replacement-suggestion search should be performed for misspelled words.
	BOOL m_stopped; ///< Special construction for handling the case when spellchecking is aborted inside Progress().
	SortedStringCollection *m_ignore_words; ///< Collection of words that should be ignored (considered correct even though they're not in the dictionary).
	UINT32 m_replacement_search_max_ms; ///< Default maximum total time for replacement-suggestion search.
	double m_replacement_timeout; ///< Current deadline for when the current replacement-suggestion search must finish (not just go back to the message-loop).

	friend class OpSpellCheckerManager;
	friend class OpSpellCheckerLanguage;
};

/** Simple interface implemented by the users of the spellchecker for allowing the OpSpellCheckerSession to iterate through
	words during spellchecking. 
	
	TODO: This interface has some limitations, as now it's the user's (e.g. OpDocumentEdit)	responsibility to decide what 
	should be considers to be a word, by using OpSpellCheckerSession::IsWordSeparator() for single characters. 
	This makes it tricky for e.g. implementing avoiding of spellchecking URLs. If for example '/' is considered to be a
	word separator, then will the URL be split up and the spellchecker will just spellcheck the individual parts - and if 
	characters in URLs such as '/' is never	considered to be word-separator - then the spellchecker will report misspellings
	for "words" like "correct/correct".
	So it would be better to let this iterator just iterate through text-content "blindly" and let the spellchecker decide
	what should be words and not, something like this:
	
	class OpSpellCheckerWordIterator
	{
	public:
		virtual const uni_char *GetCurrentText() = 0;
		
		// is_text_content_separator should be set to TRUE if the next text is separated from the previous due to a 
		// new-line, an image inside the text or similar.
		virtual BOOL NextText(BOOL &is_text_content_separator) = 0; 
		virtual BOOL PrevText(BOOL &is_text_content_separator) = 0;
		virtual void MarkAsBeginningOfWord(int ofs); // will save the position somehow
		virtual void MarkAsEndOfWord(int ofs); // dito
	};
	*/
class OpSpellCheckerWordIterator
{
public:
	virtual ~OpSpellCheckerWordIterator();

	/** Returns the current word, this may be NULL or "", then however will OpSpellCheckerListener::OnCorrectSpellingFound
		be receiving a callback for this (with this iterator as argument). */
	virtual const uni_char *GetCurrentWord() = 0;
	
	/** Continue to the next word, if FALSE is returned does this mean that we've reached the end of the text that should
		be spellchecked - and OpSpellCheckerListener::OnCheckingComplete() will be called. */
	virtual BOOL ContinueIteration() = 0;
};

/** This is an iterface for receiving event about what an OpSpellCheckerSession is up to. Each OpSpellCheckerSession
	must have an OpSpellCheckerListener. */
class OpSpellCheckerListener
{
public:
	virtual ~OpSpellCheckerListener();
	
	/** This callback will be triggered when session is ready, that means that the dictionary has been successfully loaded. */
	virtual void OnSessionReady(OpSpellCheckerSession *session) {}
	
	/** Will be triggered when an error has occured, either during dictionary-loading or spellchecking. error_string
		might contain a textual description of the error, however this is currently not implemented and will always
		be NULL. */
	virtual void OnError(OpSpellCheckerSession *session, OP_STATUS error_status, const uni_char *error_string) {}
	
	/** Callback for then a misspelled word has been found, word contains the misspelled word and replacements is a
		NULL-terminated array of replacement-suggestions. If OpSpellCheckerSession::CheckText was called with
		find_replacements == FALSE will this array always be empty. An empty array will be an array with a single
		NULL element. Notice: the strings will be deleted immediately after this callback, so don't try to save any 
		pointers to them. */
	virtual void OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements) {}
	
	/** Callback for when the correctly spelled word word has been spellchecked. */
	virtual void OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word) {}
	
	/** Callback for when spellchecking has completed, that is when OpSpellCheckerWordIterator::ContinueIteration()
		returned FALSE for the word-iterator used for spellchecking (given as argument to OpSpellCheckerSession::CheckText()). */
	virtual void OnCheckingComplete(OpSpellCheckerSession *session) {}
	
	/** Callback for when the spellchecking times-out and (temporary) returns back to the message-loop. (This is used for
		an "optimization" in OpDocumentEdit.) */
	virtual void OnCheckingTakesABreak(OpSpellCheckerSession *session) {}
};

#define REPLACEMENT_CHAR_BUF_SIZE (SPC_MAX_REPLACEMENTS*20)

/** This class contains "spellchecking-information" about a word that has been spellchecked, such as the replacement
	suggestions if the word was misspelled. Currently only one such object is created during runtime, the global
	g_spell_ui_data. The point is that a user should be able to right-click on a word, which will cause data to be
	filled in this buffer (the OpSpellCheckerWordIterator for the word, the replacement-suggestions, etc.) - and
	stored, so that later when the user takes some action - like choosing to replace the misspelled word - will the
	data necessary for performing that operation be kept in here.
	It might also happen that the data is not associated with any particular word but only with a SpellUIHandler, such
	as an OpDocumentEdit instance. This happens e.g. if the user clicks in an editable area where there is no word.
	Then it's not possible to replace any word, etc. But it might still be possible to enable/disable spellchecking
	for the SpellUIHandler or to choose a different language. */
class SpellUIData : public OpSpellCheckerListener
{
public:
	SpellUIData() { m_current_id = 0; InvalidateData(); }
	virtual ~SpellUIData() {}
	
	/** Returns the SpellUIHandler associated with the "spellcheck-event", for example - an OpDocumentEdit instance. */
	SpellUIHandler *GetSpellUIHandler() { return m_handler; }
	
	/** Returns the word-iterator that contains the word associated with the data. */
	OpSpellCheckerWordIterator *GetWordIterator() { return m_word_iterator; }
	
	/** Makes the content in the buffer invalid. This might happen if the SpellUIHandler is deleted, if the data is
		used to perform and operation, like replacing a word, etc. */
	void InvalidateData();
	
	/** Returns TRUE if the current word associated with the data is correct. */
	BOOL IsCorrect() { return m_is_correct; }
	
	/** Returns TRUE if the data is valid, that is, if InvalidateData() hasn't been called (and no new data has been added). */
	BOOL IsValid() { return m_is_valid; }
	
	/** Returns TRUE if the data is associated with a particular word in the context of the SpellUIHandler. */
	BOOL HasWord() { return m_word_iterator && m_word_iterator->GetCurrentWord() && *m_word_iterator->GetCurrentWord(); }
	
	/** Returns the text in the current word-iterator (might be NULL). */
	const uni_char* GetWord() { return m_word_iterator ? m_word_iterator->GetCurrentWord() : NULL; }
	
	/** Returns the number of replacement-suggestions. */
	int GetReplacementCount() { return m_replacement_count; }
	/** Returns the replacement-suggestion at index. */
	const uni_char *GetReplacement(int index) { return index < m_replacement_count ? m_replacements[index] : NULL; }

	/** Returns the current "ID". Each "version" of the data stored in this object gets a new ID. This is used for
		ensuring that the data haven't been overwritten by a new version - by SpellUiSessionImpl. */
	int GetCurrentID() { return m_is_valid ? m_current_id : 0; }
	
	/** This function "invalidates" the old data stored and create a new ID associated with the SpellUIHandler handler. */
	int CreateIdFor(SpellUIHandler *handler);

	// This class implements OpSpellCheckerListener. The idea is that an OpSpellCheckerSession should be created with
	// this object as the listener, a word should be checked - and the data in the buffer (replacement-suggestions) 
	// will be "filled in" when the callbacks below are triggered.
	
	// <<<OpSpellCheckerListener>>>
	virtual void OnMisspellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word, const uni_char **replacements);
	virtual void OnCorrectSpellingFound(OpSpellCheckerSession *session, OpSpellCheckerWordIterator *word);
	virtual void OnError(OpSpellCheckerSession *session, OP_STATUS error_status, const uni_char *error_string) { InvalidateData(); };

private:
	SpellUIHandler *m_handler; ///< The SpellUIHandler currently associated with this buffer (as set by CreateIdFor()).
	OpSpellCheckerWordIterator *m_word_iterator; ///< The OpSpellCheckerWordIterator for the word associated with this buffer.
	BOOL m_is_correct; ///< TRUE if the current word is correctly spelled.
	BOOL m_is_valid; ///< TRUE if this buffer contains valid data.
	uni_char m_replacement_char_buf[REPLACEMENT_CHAR_BUF_SIZE]; // ARRAY OK 2009-04-23 jb ///< Buffer for storing replacement-suggestions.
	uni_char *m_replacements[SPC_MAX_REPLACEMENTS]; ///< Pointers to the replacement-suggestions.
	int m_replacement_count; ///< Number of replacement-suggestions.
	int m_current_id; ///< The current ID for the current "version" of the data in the buffer.
};

#define SPC_WORD_SEPARATOR_LOOKUP_CODEPOINTS 512

/** One OpSpellCheckermanager should exist during runtime, that is g_internal_spellcheck. This is the class that manages
	all the installed languages. It scans the directory folder on disk and creates OpSpellCheckerLanguage objects for
	the dictionaries it finds which it manages.
	CreateSession() is the starting-point for all spellchecking-activities as it creates an OpSpellCheckerSession for
	a spcified langage. There are also functions for obtaining all installed languages, etc. */
class OpSpellCheckerManager : public MessageObject
{
public:
	OpSpellCheckerManager();
	~OpSpellCheckerManager();
	
	OP_STATUS Init();
	
	/** Will delete the "cached" dictionary which is not used by any session, if there are any such. It Will also call
		OpSpellChecker::FreeCachedData for the rest of the dictionaries in memory. */
	BOOL FreeCachedData(BOOL toplevel_context);
	
	/** Will return g_spell_ui_data. */
	SpellUIData *GetSpellUIData() { return &m_spell_ui_data; }

	/** Handles the progress of OpSpellCheckerSession objects, loading of dictionaries, spellchecking, and
		"background updating" - when words has been added, removed or ignored. */
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	/** Handles "background updating". This happens when a word is added/removed/ignored, then will this function
		iterate through words in the context where the change occured using a OpSpellCheckerWordIterator and make
		callbacks to the SpellUIHandler for that context for marking words as correct/misspelled for the words
		which's "correctness" has changed. */
	void ProcessBackgroundUpdates();

	/** Returns TRUE if spellchecking should be enabled by default. */
	BOOL SpellcheckEnabledByDefault();
	
	/** Sets whether spellchecking should be enabled by default or not. */
	void SetSpellcheckEnabledByDefault(BOOL by_default);
	
	/** Returns an OpString_list with language-identifiers for all installed languages, such as "en_US" or "sv_SE". */
	OP_STATUS GetInstalledLanguages(OpString_list &language_list);
	
	/** Updates the list of installed languages. This should be used if a dictionary has been installed/uninstalled
		during runtime. */
	OP_STATUS RefreshInstalledLanguages();
	
	/** Returns an identifier (such as "en_US") for the current default language, which will be used e.g. when
		OpSpellUiSession::EnableSpellcheck() is called. */
	const uni_char *GetDefaultLanguage();
	
	/** Returns TRUE if there is at least one installed languge. */
	BOOL HasInstalledLanguages();
	
	/** Sets the default language to the language identified by lang (such as "en_US"), FALSE will be returned upon
		failure, e.g. if the language didn't exist. */
	BOOL SetDefaultLanguage(const uni_char *lang);
	
	/** Returns the number of installed languages. */
	int GetInstalledLanguageCount();
	
	/** Returns the OpSpellCheckerLanguage object for the language at index. */
	OpSpellCheckerLanguage* GetInstalledLanguageAt(int index);
	
	/** Returns the string identifier (such as "en_US") for the language at index. */
	const uni_char* GetInstalledLanguageStringAt(int index);
	
	/** Returns the OpSpellCheckerLanguage object for the language specified by the identifier lang (such as "en_us"),
		NULL will be returned if no matching language where found. */
	OpSpellCheckerLanguage* GetOpSpellCheckerLanguage(const uni_char *lang);

	/** This function creates an OpSpellCheckerSession for the specified language. 
		@param lang The string identifier for the language we wants to create a session for (e.g. "en_US").
		@param listener The OpSpellCheckerListener that should receive events for what happens with the session, when
		the session is ready, when a misspelled word has been found, etc. See description of OpSpellCheckerLanugage.
		@param session The OpSpellCheckerSession object created, will be set to NULL upon failure.
		@synchronous If TRUE will the OpSpellCheckerSession be synchronous (blocking) which implies that the loading
		will finish and listener->OnSessionReady() will be called (if we succeded, that is) - before this function
		call returns. */
	OP_STATUS CreateSession(const uni_char *lang, OpSpellCheckerListener *listener, OpSpellCheckerSession *&session ,BOOL synchronous = FALSE);
	
	/** Callback for when an OpSpellCheckerListener has been deleted (from ~OpSpellCheckerListener()). */
	void OnSpellCheckerListenerDeleted(OpSpellCheckerListener *listener);
	
	/** Callback for when an OpSpellCheckerWordIterator has been deleted (from ~OpSpellCheckerWordIterator()). */
	void OnWordIteratorDeleted(OpSpellCheckerWordIterator *word_iterator);

	/** Callback for when an OpSpellSession has been deleted (from ~OpSpellCheckerSession()). */
	void OnLanguageHasNoSessions(OpSpellCheckerLanguage *language);
	
	/** Callback for when an OpSpellCheckerLanguage has been deleted (from ~OpSpellCheckerLanguage()). */
	void OnLanguageDeleted(OpSpellCheckerLanguage *language);
	
	/** Callback when loading of a dictionary has finished, either successfully (succeeded==TRUE) or unsuccessfully,
		when succeeded==FALSE. */
	void OnLanguageLoadingFinished(OpSpellCheckerLanguage *language, BOOL succeeded);

protected:
	
	/** Called with deleted==TRUE when a OpSpellCheckerSession that currently is in OpSpellCheckerSession::Progress()
		is deleted. */
	void SetSessionInProgressDeleted(BOOL deleted) { m_session_in_progress_deleted = deleted; }
	
	/** Returns TRUE if a OpSpellCheckerSession that is in OpSpellCheckerSession::Progress() has been deleted. */
	BOOL GetSessionInProgressDeleted() { return m_session_in_progress_deleted; }
	
	/** Sets session to be the session that is currently running OpSpellCheckerSession::Progress(). */
	void SetSessionInProgress(OpSpellCheckerSession *session) { m_session_in_progress = session; }
	
	/** Returns the current session that is in OpSpellCheckerSession::Progresss(). */
	OpSpellCheckerSession *GetSessionInProgress() { return m_session_in_progress; }
	
	/** Starts a "background update" for the SpellUIHandler handler.
		@param handler The SpellUIHandeler for which we should mark all words matching str as correct or misspelled.
		The OpSpellCheckerWordIterator that will be used for this is handler->GetAllContentIterator().
		@param str We should mark words as correct or misspelled when they are equal to str.
		@param mark_ok If TRUE will we call handler->OnCorrectSpellingFound() for matching words, if FALSE will we call
		handler->OnMisspellingFound(). */
	OP_STATUS StartBackgroundUpdate(SpellUIHandler *handler, const uni_char *str, BOOL mark_ok);
	
	/** Returns TRUE if c is a "default-separator". That is, if WORDCHARS and BREAK statements in any dictionary is not
		considered, would this be a word-separator? (such as whitespace) */
	BOOL IsWordSeparator(uni_char c);

private:

	/** Sets file path to the full path to a file named the same as the dictionary file, but with another extension, if it exists. Empties file_path (i.e makes file_path.CStr() == NULL) if the file does not exist or if an error occurs. */
	void DeterminePathFromExtension(const uni_char *dic_full_path, const uni_char* ext, OpString& file_path);

	/** Sets own path to the full path to the own file if it exists. Empties own_path (i.e makes own_path.CStr() == NULL) if the file does not exist or if an error occurs. */
	void DetermineOwnPath(const uni_char *dic_full_path, OpString& own_path);

	/** Sets license_path if the license exists next to the .dic file. If not, it sets it to an empthy string **/
	void DetermineLincensePath(const uni_char *dic_full_path, OpString& license_path);

	/** Marks all spellchecker languages as unavailable. A language marked as unavailable won't be available to new sessions, but might remain in use by sessions that were using it when it was made unavailable.
	 *  Some common causes for a language to be unavailable is if it has been removed from disk between refreshes or if another dictionary for the same language has been installed since the last refresh.
	 */
	void MakeLanguagesUnavailable();

	/** Updates the OpSpellCheckerLanguage with identifier lang (which could be something like "en_US") with the paths
		to the .dic, .aff and .oow files specified by dic_path, affix_path and own_path. If no matching language exists
		will a new OpSpellCheckerLanguage be created which has this paths. */
	OP_STATUS AddOrRefreshLanguage(const uni_char *lang, const uni_char *dic_path, const uni_char *affix_path, const uni_char *own_path, const uni_char* metadata_path, const uni_char* license_path);
	
	/** Return the OpSpellCheckerLanguage object for the language whichs dictionary was most recently successfully loaded
		according to the prefs (might return NULL). */
	OpSpellCheckerLanguage *GetLastLanguageInPrefs();

private:
	OpSpellCheckerSession *m_session_in_progress; ///< The current OpSpellCheckerSession that is in OpSpellCheckerSession::Progress().
	BOOL m_session_in_progress_deleted; ///< TRUE if the OpSpellCheckerSession that is currently in OpSpellCheckerSession::Progress() has been deleted.
	BOOL m_has_refreshed_languages; ///< Returns TRUE if RefreshInstalledLanguages has been called at least once.
	Head m_languages; ///< Linked list of OpSpellCheckerLanguage objects for the installed languages - and languages that are loaded but has been removed since they where loaded.
	OpSpellCheckerLanguage *m_default_language; ///< The current default language.
	OpSpellCheckerLanguage *m_language_with_no_sessions; ///< The "cache" for one OpSpellChecker belonging to an OpSpellCheckerLanguage that has no OpSpellCheckerSession objects.
	ExpandingBuffer<OpSpellCheckerLanguage*> m_valid_languages; ///< List of "valid" languages, that is, all except of languages that has been loaded, has sessions, but has been deleted since.
	Head m_background_updates; ///< The linked list of SpellUpdateObject objects for "background updating" of words that have been added/removed/ignored.
	SpellUIData m_spell_ui_data; ///< The SpellUIData that is pointed out by g_spell_ui_data.
	UINT8 m_word_separators[SPC_WORD_SEPARATOR_LOOKUP_CODEPOINTS/8]; ///< Bit-field telling which unicode-characters < SPC_WORD_SEPARATOR_LOOKUP_CODEPOINTS that are "default word-separators".

	friend class OpSpellCheckerSession;
	friend class OpSpellUiSessionImpl;
	friend class OpSpellChecker;
};

/** Simple implementation of a OpSpellCheckerWordIterator which keeps only one word.
	See description of OpSpellCheckerWordIterator. */
class SingleWordIterator : public OpSpellCheckerWordIterator
{
public:	
	SingleWordIterator(const uni_char *word) { m_word = word; };
	virtual ~SingleWordIterator() {};
	virtual const uni_char *GetCurrentWord() { return m_word; }
	virtual BOOL ContinueIteration() { return FALSE; }

private:
	const uni_char *m_word;
};

/** "Informs" listener of the result of the spellchecking of lookup_state->word_iterator. This implies calling either
	listener->OnCorrectSpellingFound() or listener->OnMisspellingFound(). */
OP_STATUS InformListenerOfLookupResult(OpSpellCheckerSession *session, OpSpellCheckerLookupState *lookup_state, OpSpellCheckerListener *listener);

/** This class is an implementation of OpSpellUiSession declared in windowcommander/OpWindowCommander.h. See that class
	for descriptions of the class and the functions. This implementation uses g_spell_ui_data as the buffer for keeping
	all the relevant data. The content of this object is just an ID that is used for ensuring that the content of
	g_spell_ui_data is still the content associated with the ID. */
class OpSpellUiSessionImpl : public OpSpellUiSession
{
public:
	OpSpellUiSessionImpl(int id) { SetID(id); }
	OpSpellUiSessionImpl() { m_id = 0; }
	virtual ~OpSpellUiSessionImpl() { }

	int GetID() { return m_id; }
	void SetID(int id) { m_id = id; }

	virtual BOOL IsValid();
	virtual int GetSelectedLanguageIndex();
	virtual int GetInstalledLanguageCount() { return g_internal_spellcheck->GetInstalledLanguageCount(); }
	virtual const uni_char* GetInstalledLanguageStringAt(int index);
#ifdef WIC_SPELL_DICTIONARY_FULL_NAME
	virtual const uni_char* GetInstalledLanguageNameAt(int index);
#endif // WIC_SPELL_DICTIONARY_FULL_NAME
	virtual int GetInstalledLanguageVersionAt(int index);
	virtual OP_STATUS GetInstalledLanguageLicenceAt(int index, OpString& license);
	virtual BOOL SetLanguage(const uni_char *lang);

	virtual OP_STATUS GetReplacementString(int replace_index, OpString &value);
	virtual int GetReplacementStringCount();
	
	virtual BOOL CanEnableSpellcheck();
	virtual BOOL CanDisableSpellcheck();
	virtual BOOL CanAddWord();
	virtual BOOL CanIgnoreWord();
	virtual BOOL CanRemoveWord();
	
	virtual BOOL EnableSpellcheck();
	virtual BOOL DisableSpellcheck();
	virtual OP_STATUS AddWord() { return UpdateContent(SPC_UPDATE_ADD); }
	virtual OP_STATUS IgnoreWord() { return UpdateContent(SPC_UPDATE_IGNORE); }
	virtual OP_STATUS RemoveWord() { return UpdateContent(SPC_UPDATE_REMOVE); }
	virtual OP_STATUS ReplaceWord(int replace_index);

	virtual BOOL HasWord();
	virtual OP_STATUS GetCurrentWord(OpString &word);

private:
	enum SpellContentUpdateType
	{
		SPC_UPDATE_ADD = 0,
		SPC_UPDATE_IGNORE,
		SPC_UPDATE_REMOVE
	};
	OP_STATUS UpdateContent(SpellContentUpdateType type);

private:
	int m_id;
};

/** This object keeps a word which's "correctness-status" should be updated in the context of a SpellUISession. This
	objects are used for "background updating" of words that has been targets for add, remove or ignore operations.
	Then we'll update the "correctenss" for those words in the context of the SpellUISession by calling 
	SpellUISession::OnCorrectSpellingFound() and SpellUISession::OnMisspellingFound for words that matches the word
	this object keeps, this is possible because	SpellUISession implements OpSpellCheckerListener. */
class SpellUpdateObject : public Link
{
public:
	/** Constructor handler defines the context in which we should make the update (such as a OpDocumentEditInstance),
		if mark_ok == TRUE does this mean we should call handler->OnCorrectSpellingFound() on the affected word, and
		if mark_ok == FALSE should we call handler->OnMisspellingFound(). */
	SpellUpdateObject(SpellUIHandler *handler, BOOL mark_ok) : Link() { m_handler = handler; m_mark_ok = mark_ok; m_word_iterator = NULL; }
	~SpellUpdateObject() { Out(); }
	
	/** Sets the word we should update the "correctness-status" for. */
	OP_STATUS SetWord(const uni_char *word) { return m_word.Set(word); }
	
	/** Returns the word we should update the "correctness-status" for. */
	const uni_char *GetWord() { return m_word.IsEmpty() ? UNI_L("") : m_word.CStr(); }
	
	/** Sets the word-iterator we should use for iterating through the text in the context of the SpellUIHandler. */
	void SetWordIterator(OpSpellCheckerWordIterator *word_iterator) { m_word_iterator = word_iterator; }
	
	/** Gets the word-iterator we should use for iterating through the text in the context of the SpellUIHandler. */
	OpSpellCheckerWordIterator* GetWordIterator() { return m_word_iterator; }
	
	/** Returns the SpellUIHandler in which's context we are doing the update, this can be e.g. a OpDocumentEdit instance. */
	SpellUIHandler* GetUIHandler() { return m_handler; }
	
	/** Returns TRUE if we should mark word as correct, or FALSE, if we should mark them as misspelled. */
	BOOL MarkOK() { return m_mark_ok; }

private:
	SpellUIHandler *m_handler; ///< The SpellUIHandler we're doing the updating for.
	OpSpellCheckerWordIterator *m_word_iterator; ///< The word-iterator we use to iterate words in the context of m_handler.
	BOOL m_mark_ok; ///< If we should mark words as correct (TRUE) or misspelled (FALSE).
	OpString m_word; ///< The word we should do updating on when we find.
};

/** This is an interface that should be implemented by classes that whiches that this module should handle spellchecking
	for it's text-content. This is called SpellUIHandler, but maybe it would be better to call it just "handler" as there
	is nothing that has to do directly with any UI feature with this interface. In practice it this the case though. The
	classes that implements this class is currently OpDocumentEdit, OpTextCollection and OpEditSpellchecker which is used
	bu OpEdit. Also, it's intentded to be used together with OpSpellCheckerUiSession which in it's "nature" is something
	that should be used by UI features of the platform.
	Notice that this interface inherits OpSpellCheckerListener which should be implemented as well. 
	TODO: Perhaps this should be more of an abstract class which keeps track of the OpSpellCheckerSession itself, etc. */
class SpellUIHandler : public OpSpellCheckerListener
{
public:
	virtual ~SpellUIHandler() {	if (g_spell_ui_data && g_spell_ui_data->GetSpellUIHandler() == this) g_spell_ui_data->InvalidateData(); }
	
	/** Returns the OpSpellCheckerSession for this handler. */
	virtual OpSpellCheckerSession* GetSpellCheckerSession() { return NULL; }
	
	/** Sets the spellchecker language to the string lang (such as "en_US"). Should return FALSE upon failure. */
	virtual BOOL SetSpellCheckLanguage(const uni_char *lang) { return FALSE; }
	
	/** Disable spellchecking. */
	virtual void DisableSpellcheck() {}
	
	/** Should return an OpSpellCheckerWordIterator that can be used for iterating though all the words in the context
		of this SpellUIHandler. */
	virtual OpSpellCheckerWordIterator* GetAllContentIterator() { return NULL; }
	
	/** Should replace the word defined by word_iterator with the string str. */
	virtual OP_STATUS ReplaceWord(OpSpellCheckerWordIterator *word_iterator, const uni_char *str) { return OpStatus::OK; }
	
	/** If TRUE is returned should preferred_lang containg a language that is preferred as default-language. This will
		override the current default-language. The idea is that the handler should be able to implement some kind of
		language-detection. */
	virtual BOOL GetPreferredLanguage(OpString &preferred_lang) { return FALSE; }
};

#endif // OP_SPELL_CHECKER_API_H
