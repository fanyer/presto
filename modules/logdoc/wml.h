/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _WML_H_
#define _WML_H_

#ifdef _WML_SUPPORT_

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/simset.h"
#include "modules/logdoc/elementref.h"

class HTML_Element;
class FormObject;
class DocumentManager;
class FramesDocument;

class WML_Context;

//-----------------------------------------------------------
// tables for the WML elements
//-----------------------------------------------------------

#include "modules/logdoc/wmlenum.h"

/// \enum enum for the WML event codes
enum WML_EventType
{
    WEVT_ONPICK = 128,
    WEVT_ONTIMER,
    WEVT_ONENTERFORWARD,
    WEVT_ONENTERBACKWARD,
    WEVT_ONCLICK, ///< mock event for clicking on URLs
    WEVT_UNKNOWN = 511
};

/// constants for the WML status fields, manual bit-thingy
/// DO NOT CHANGE THE VALUES TO ANYTHING OTHER THAN 2^n

#define WS_AOK			0x00000000 ///< all ok, zero state
#define WS_ERROR		0x00000001 ///< a general error
#define WS_NOACCESS		0x00000002 ///< access to the card is denied
#define WS_GO			0x00000004 ///< a GO-navigation is in progress
#define WS_ENTERBACK	0x00000008 ///< a backward navigation going on
#define WS_REFRESH		0x00000010 ///< a refresh navigation going on
#define WS_NOOP         0x00000020 ///< a task is set to do no navigation
#define WS_TIMING		0x00000040 ///< if the timer is ticking
#define WS_USERINIT		0x00000080 ///< user initiated navigation
#define WS_CLEANHISTORY	0x00000100 ///< clean up the WML history

// internal status
#define WS_FIRSTCARD	0x00000200 ///< not started parsing second card yet
#define WS_INVAR		0x00000400 ///< inside a variable
#define WS_NEWCONTEXT	0x00000800 ///< the WML context must be reset
#define WS_ODDTIME		0x00001000 ///< used by timer to even out time fractions
#define WS_INRIGHTCARD	0x00002000 ///< the addressed card is found
#define WS_NOSECWARN	0x00004000 ///< suppress form-warning if <go>-element have no <postfield>/<setvar>
#define WS_SENDREF		0x00008000 ///< send referer only when this flag is set
#define WS_ILLEGALFORM	0x00010000 ///< set when the form input is not valid

#define WS_CHECKTIMER	0x00020000 ///< first card timer might be wrong
#define WS_CHECKFORWARD	0x00040000 ///< first card onenterforward might be wrong
#define WS_CHECKBACK	0x00080000 ///< first card onenterbackward might be wrong


//-----------------------------------------------------------
// Various defines
//-----------------------------------------------------------

///\enum handler indices
enum
{
	WH_TIMER	= 0,
	WH_FORWARD	= 1,
	WH_BACKWARD	= 2,
	WH_ABSOLUTLY_LAST_ENUM = 3
};

///\enum WML variable expansion constants
enum
{
	WML_CONV_NONE	= 0,
	WML_CONV_ESC	= 1,
	WML_CONV_UNESC	= 2
};

///\enum values for the wrap attribute
enum
{
	WATTR_VALUE_nowrap	= 0,
	WATTR_VALUE_wrap	= 1
};

///\enum values for the cache attribute
enum
{
	WATTR_VALUE_nocache	= 0,
	WATTR_VALUE_cache	= 1
};

/// Class for storing WML variables.
/// Variables can be set explicitly by SETVAR and POSTVAR or implicitly by a TIMER 
/// or form control during the execution of a task.
/// Variables can be inserted into the WML document by using the $varname notation.
class WMLVariableElm
  : public ListElement<WMLVariableElm>
{
private:
    uni_char *m_name;  ///< Variable name
    uni_char *m_value; ///< Variable value

public:
					WMLVariableElm() : m_name(NULL), m_value(NULL) {}
                    ~WMLVariableElm();

    OP_STATUS       SetVal(const uni_char *new_value, int new_len); ///< Set the value, replacing existing value
    OP_STATUS       SetName(const uni_char *new_name, int new_len); ///< Set the name, replacing existing name

    const uni_char* GetValue() { return m_value; }
    const uni_char* GetName() { return m_name; }

	/// Compare the name of the variable with equal_name.
	///\param[in] equal_name. String containing name, need not be nul terminated
	///\param[in] n_len. Length of equal_name
    BOOL            IsNamed(const uni_char *equal_name, unsigned int n_len);

	/// Copies the variable to the existing variable src_var.
	///\param[out] src_var. Preallocated variable that will get the values of this variable
    OP_STATUS       Copy(WMLVariableElm *src_var);
};

/// The class for storing WML tasks.
/// Tasks in WML are essentially navigations between cards or internally within a card.
/// Tasks are GO (to another card), PREV (the previous card), REFRESH (the same card)
/// or NOOP (do nothing)
/// Tasks can be bound to links like for A, DO or ANCHOR elements or to events like 
/// ONPICK or ONENTERFORWARD.
/// When performing a task the WML context will be updated with the new variables 
/// associated with the task in form of a SETVAR or POSTVAR element or intrinsic
/// variables like values of form controls.
class WMLNewTaskElm
	: public ListElement<WMLNewTaskElm>
	, public ElementRef
{
private:
	/// Searching the logical tree for the next variable associated with the task.
	///\param[out] last_found Iterator used by subsequent searches for variables
	///\param[out] out_name The name of the next variable found, NULL if none found
	///\param[out] value_name The value of the next variable found
	///\param[in] find_first Must be TRUE if there has been no previous searches
	///\param[in] internal Must be TRUE if searching for SETVAR variablees, else it will search for POSTVAR variables
    void    LocalGetNextVariable(HTML_Element **last_found, const uni_char **out_name, const uni_char **out_value, BOOL find_first, BOOL internal = TRUE);

public:
			WMLNewTaskElm(HTML_Element *new_he) : ElementRef(new_he) {}
			WMLNewTaskElm(WMLNewTaskElm *src_task); ///< Copy constructor
            ~WMLNewTaskElm() {}

    BOOL    IsNamed(const uni_char *equal_name); ///< Return TRUE if the taskname matches equal_name \param[in] equal_name

	/// Searching for the first internal (SETVAR) variable for the task.
	///\param[out] last_found is used as an iterator for subsequent searches
	///\param[out] out_name is the name of the next variable found, NULL if none found
	///\param[out] value_name is the value of the next variable found
    void    GetFirstIntVar(HTML_Element **last_found, const uni_char **out_name, const uni_char **out_value) { LocalGetNextVariable(last_found, out_name, out_value, TRUE, TRUE); }

	/// Searching for the next internal (SETVAR) variable for the task.
	///\param[out] last_found is an iterator returned by GetFirstIntVar()
	///\param[out] out_name is the name of the next variable found, NULL if none found
	///\param[out] value_name is the value of the next variable found
    void    GetNextIntVar(HTML_Element **last_found, const uni_char **out_name, const uni_char **out_value) { LocalGetNextVariable(last_found, out_name, out_value, FALSE, TRUE); }

	/// Searching for the first external (POSTVAR) variable for the task.
	///\param[out] last_found is used as an iterator for subsequent searches
	///\param[out] out_name is the name of the next variable found, NULL if none found
	///\param[out] value_name is the value of the next variable found
    void    GetFirstExtVar(HTML_Element **last_found, const uni_char **out_name, const uni_char **out_value) { LocalGetNextVariable(last_found, out_name, out_value, TRUE, FALSE); }

	/// Searching for the next external (POSTVAR) variable for the task.
	///\param[out] last_found is an iterator returned by GetFirstExtVar()
	///\param[out] out_name is the name of the next variable found, NULL if none found
	///\param[out] value_name is the value of the next variable found
    void    GetNextExtVar(HTML_Element **last_found, const uni_char **out_name, const uni_char **out_value) { LocalGetNextVariable(last_found, out_name, out_value, FALSE, FALSE); }

    const uni_char* GetName();

	/// Returns the element associated with the task.
    HTML_Element*   GetHElm() { return GetElm(); }

	/// @See ElementRef::OnDelete().
	virtual	void	OnDelete(FramesDocument *document);

	/// @See ElementRef::OnRemove().
	virtual	void	OnRemove(FramesDocument *document) { OnDelete(document); }
};

/// The class for mapping elements to WML tasks.
/// Several elements can map to the same task. For instance all the child elements in a DO element
/// will trigger the same task.
class WMLTaskMapElm
	: public ListElement<WMLTaskMapElm>
	, public ElementRef
{
private:
    WMLNewTaskElm   *m_task;  ///< The associated WML task

public:
    WMLTaskMapElm(WMLNewTaskElm *new_task, HTML_Element *new_he) : ElementRef(new_he), m_task(new_task) {}
	WMLTaskMapElm(WMLTaskMapElm *src_task_map); ///< Copy constructor
    ~WMLTaskMapElm() {}

	/// Returns TRUE if this WMLTaskMapElm is associated with the element he. \param[in] he Element to check
    BOOL			BelongsTo(HTML_Element *he) { return he == GetElm(); }
	HTML_Element*	GetHElm() { return GetElm(); }
    WMLNewTaskElm*  GetTask() { return m_task; } ///< Returns the associated WML task
    void            SetTask(WMLNewTaskElm *new_task) { m_task = new_task; }

	/// @See ElementRef::OnDelete().
	virtual	void	OnDelete(FramesDocument *document);

	/// @See ElementRef::OnRemove().
	virtual	void	OnRemove(FramesDocument *document) { OnDelete(document); }
};

/// The WML lexical lookup tool.
/// Used primarily by the parser to convert between strings and internal codes for elements and attributes.
class WML_Lex
{
public:
	/// Returns the numeric code corresponding to the event evt.
	///\param[in] evt Nul terminated name of event
    static WML_EventType   GetEventType(const uni_char *evt);
};

//-----------------------------------------------------------
// class for easy manipulation of internal lists and status
//-----------------------------------------------------------

class WMLStats
{
public:
	WMLStats(int first_in_stssion);
	~WMLStats();

	/// Copies the contents of src_stats to this stat. Usually without the tasks
	///\param[in] src_stats. Stat block that this stat will receive the contents of
	///\param[in] merge_active_vars. Merge the variables of the active task to this task
	///\param[in] include_tasks. Copy the tasks from src_stats as well
	OP_STATUS	Copy(WMLStats *src_stats, BOOL merge_active_vars, BOOL include_tasks);

	void RemoveReferencesToTask(WMLNewTaskElm *task, WML_Context *context);

    UINT32				m_status; ///< WML status bitfield. Uses values from the WS_* values
	BOOL				m_variables_changed; ///< TRUE if any variables have changed
	int					m_first_in_session; ///< History number of the first WML page on the current site

    AutoDeleteList<WMLVariableElm>*	m_var_list; ///< List of the current WML variables.
	AutoDeleteList<WMLVariableElm>*	m_active_vars; ///< List of variables that will be merged when navigation is successful
    AutoDeleteList<WMLNewTaskElm>*	m_task_list; ///< List of the current WML tasks
    AutoDeleteList<WMLTaskMapElm>*	m_task_map_list; ///< List of bindings between HTML_Elements and tasks in the task list

    WMLNewTaskElm**		m_event_handlers; ///< List of event handler tasks

    WMLVariableElm*		m_timer_val; ///< The WML variable holding the timer value
};


//-----------------------------------------------------------
// the WML context class
//-----------------------------------------------------------

class WML_Context : public MessageObject
{
private:
	WMLStats*			m_old_stats; ///< stats of the previous card during navigation to another card
	WMLStats*			m_tmp_stats; ///< store stats before first card if we haven't found target yet
	WMLStats*			m_current_stats; // stats of the current card

	BOOL				m_preparse_called; ///< TRUE if PreParse() has been called since the last call to PostParse()
	BOOL				m_postparse_called; ///< TRUE if PostParse() has been called since the last call to PreParse()
    DocumentManager*    m_doc_manager;

	uni_char*			m_substitute_buffer; ///< buffer used to store results of a variable substitution
	int					m_substitute_buffer_len; ///< length of the buffer above

	AutoNullElementRef	m_pending_current_card; ///< Card that will be current if navigation succeeds

	UINT				m_refs; ///< number of references to this context. can be deleted only when 0

	OP_STATUS	SetCurrentCard(); ///< Prepare the current card for display

public:
            WML_Context(DocumentManager *dm = NULL);
            ~WML_Context();

	OP_STATUS	Init(); ///< Secondary constructor

	static BOOL IsHtmlElement(WML_ElementType type);
	static BOOL IsWmlAttribute(WML_AttrType type);
	static BOOL IsHtmlAttribute(WML_AttrType type);

	static BOOL	NeedSubstitution(const uni_char *s, size_t s_len);
    int			SubstituteVars(const uni_char *s, int s_len,
					uni_char *&tmp_buf, int len_tb,
					BOOL esc_by_default = FALSE,
					OutputConverter *converter = NULL);
	OP_STATUS	GrowSubstituteBufferIfNeeded(uni_char*& buffer, int& length, int wanted_index);

	int			GetFirstInSession() { return m_current_stats->m_first_in_session; }
	void		SetFirstInSession(int new_first) { m_current_stats->m_first_in_session = new_first; }

    OP_STATUS	SetNewContext(int new_first = -1);

	void		HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

    AutoDeleteList<WMLVariableElm>*
				GetVarList() { return m_current_stats->m_var_list; }

    void		RemoveTimer(BOOL delete_task);
    void		StartTimer(BOOL start = TRUE);
    OP_STATUS   SetTimer(const uni_char *name, const uni_char *time);

    OP_STATUS   Copy(WML_Context *src_wc, DocumentManager* doc_man = NULL);

	OP_STATUS	DenyAccess();

    OP_STATUS   PreParse();
    OP_STATUS   PostParse();
	BOOL        IsParsing() { return m_preparse_called && !m_postparse_called; }
	void		ScrapTmpCurrentCard();
	OP_STATUS	PushTmpCurrentCard();
	OP_STATUS	RestartParsing();

    AutoDeleteList<WMLVariableElm>*
					GetActiveVars() { return m_current_stats->m_active_vars; }
    OP_STATUS       SetActiveTask(WMLNewTaskElm *new_active_task);

    WMLNewTaskElm*  GetTaskByElement(HTML_Element *he);
    OP_STATUS       SetTaskByElement(WMLNewTaskElm *new_task, HTML_Element *he);

    WMLNewTaskElm*  GetEventHandler(WML_EventType event);
    OP_STATUS       SetEventHandler(WML_EventType event, WMLNewTaskElm *handler);

    WMLNewTaskElm*  NewTask(HTML_Element *task_he);
    void			DeleteTask(WMLNewTaskElm *task);
	WMLNewTaskElm*  GetTask(const uni_char *task_name);
    OP_STATUS       PerformTask(WMLNewTaskElm *task, BOOL& noop, BOOL is_user_requested, WML_EventType event = WEVT_UNKNOWN);

    UINT32			GetStatus() { return m_current_stats->m_status; };
    BOOL            IsSet(UINT32 mask) { return (m_current_stats->m_status & mask) == mask; }
    void            SetStatus(UINT32 new_status) { m_current_stats->m_status = new_status; }
    void            SetStatusOn(UINT32 mask) { m_current_stats->m_status |= mask; }
    void            SetStatusOff(UINT32 mask) { m_current_stats->m_status &= ~mask; }

    WMLVariableElm*	SetVariable(const uni_char *name, const uni_char *value);
    const uni_char*	GetVariable(const uni_char *name, int n_len = -1);

	URL				GetWmlUrl(HTML_Element* he, UINT32* action = NULL, WML_EventType event = WEVT_UNKNOWN);

	OP_STATUS		UpdateWmlSelection(HTML_Element* he, BOOL use_value);
	OP_STATUS		UpdateWmlInput(HTML_Element* he, FormObject* form_obj);
	void			SetInitialSelectValue(HTML_Element *select_elm);

    DocumentManager*    GetDocManager() { return m_doc_manager; }

	HTML_Element*	GetPendingCurrentCard() { return m_pending_current_card.GetElm(); }
	void			SetPendingCurrentCard(HTML_Element *card) { m_pending_current_card.SetElm(card); }

	void			IncRef() { m_refs++; }
	void			DecRef();
};
#endif // _WML_SUPPORT_
#endif // _WML_H
