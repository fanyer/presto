/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DISPLAY_WRITING_SYSTEM_HEURISTIC_H
#define DISPLAY_WRITING_SYSTEM_HEURISTIC_H

# ifdef DISPLAY_WRITINGSYSTEM_HEURISTIC

/** @short Heuristic for inferring a likely script from textual data.
 *
 * A heuristic that tries to infer a likely script from textual data (or, in
 * case of mixed scripts, tries to find the "main" script). It is useful e.g.
 * for picking a suitable font when the 'lang' attribute has not been set.
 *
 * Roughly, each character is associated with a script, and the script with
 * most characters from the text in it wins. The fact that some characters
 * appear in multiple scripts complicates things a bit though.
 */
class WritingSystemHeuristic
{
public:
    WritingSystemHeuristic();

	/**
	 * Analyzes the charaters from 'text'; multiple calls have a cumulative
	 * effect. If MAX_ANALYZE_LENGTH characters have already been analyzed,
	 * this is a no-op.
	 *
	 * @param text Text to analyze.
	 * @param text_length The number of uni_char's in 'text', or -1 to have
	 *        the length determined using uni_strlen().
	 */
	void Analyze(const uni_char *text, int text_length = -1);

	/**
	 * Returns TRUE if less than MAX_ANALYZE_LENGTH characters have been
	 * analyzed; otherwise, returns FALSE.
	 */
	BOOL WantsMoreText() const;

	/**
	 * Returns a guess for the (main) script being used for the text analyzed
	 * so far.
	 */
	WritingSystem::Script GuessScript() const;

	/**
	 * Resets the heuristic to a pristine state, as if no text had been
	 * analyzed.
	 */
	void Reset();

private:
	enum { // The maximum number of characters to analyze
		   MAX_ANALYZE_LENGTH       = 2000,
		   // The number of characters from non Latin scripts
		   // that need to appear before a page with a latin script is
		   // considered to use one of those scripts. Perhaps a percentage
		   // would be better here.
		   NON_LATIN_THRESHOLD = 20 };

	size_t m_scorecard[WritingSystem::NumScripts];
	size_t m_analyzed_count;

	void AnalyzeInternal(const uni_char *text, size_t text_length);

	void AnalyzeHan(uni_char up);
	void AnalyzeArabic(uni_char up);
	static int CompareChars(const void* kp, const void* ep);

	static WritingSystem::Script ScriptFromLocale();
};
# endif // DISPLAY_WRITINGSYSTEM_HEURISTIC

#endif // !DISPLAY_WRITING_SYSTEM_HEURISTIC_H
