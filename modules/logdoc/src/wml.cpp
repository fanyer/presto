/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _WML_SUPPORT_

#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/markup.h"
#include "modules/forms/form.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/url/url_api.h"
#include "modules/encodings/utility/opstring-encodings.h"
#include "modules/formats/uri_escape.h"
#include "modules/forms/piforms.h"

#include "modules/logdoc/wml.h"

// ----------------------------------------------------------------------
// the WMLVariableElm implementation
// ----------------------------------------------------------------------

WMLVariableElm::~WMLVariableElm()
{
	OP_DELETEA(m_value);
	OP_DELETEA(m_name);
}

// Sets the value of the variable.
// ----------------------------------------------------------------------
// IN       new_value   - the new value to be set
// IN		new_len		- the length of the new_value
OP_STATUS
WMLVariableElm::SetVal(const uni_char *new_value, int new_len)
{
	uni_char *old_value = m_value;

    if (!new_value)
    {
		new_value = UNI_L("");
		new_len = 0;
	}
	m_value = UniSetNewStrN(new_value, new_len);
	OP_DELETEA(old_value);

    return m_value ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

// Sets the name of the variable.
// ----------------------------------------------------------------------
// IN       new_name    - the new name to be set
OP_STATUS
WMLVariableElm::SetName(const uni_char *new_name, int new_len)
{
	uni_char *old_value = m_name;

    if (!new_name)
    {
		new_name = UNI_L("");
		new_len = 0;
	}
	m_name = UniSetNewStrN(new_name, new_len);
	OP_DELETEA(old_value);

    return m_name ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

BOOL
WMLVariableElm::IsNamed(const uni_char *equal_name, unsigned int n_len)
{
	if (m_name && equal_name && uni_strlen(m_name) == n_len)
		return uni_strncmp(m_name, equal_name, n_len) == 0;

	return FALSE;
}

OP_STATUS
WMLVariableElm::Copy(WMLVariableElm *src_var)
{
    if (src_var)
    {
        const uni_char *old_value = src_var->GetName();
        RETURN_IF_MEMORY_ERROR(SetName(old_value, uni_strlen(old_value)));

        old_value = src_var->GetValue();
        RETURN_IF_MEMORY_ERROR(SetVal(old_value, uni_strlen(old_value)));
    }

    return OpStatus::OK;
}

// ----------------------------------------------------------------------
// the WMLNewTaskElm implementation
// ----------------------------------------------------------------------

WMLNewTaskElm::WMLNewTaskElm(WMLNewTaskElm *src_task)
{
	SetElm(src_task->GetElm());
}

void
WMLNewTaskElm::LocalGetNextVariable(HTML_Element **last_found, const uni_char **out_name, const uni_char **out_value, BOOL find_first, BOOL internal)
{
    WML_ElementType get_type = internal ? WE_SETVAR : WE_POSTFIELD;
    HTML_Element *current_he = GetElm()->FirstChild() ? GetElm()->FirstChild() : GetElm();

	if (!find_first && *last_found)
		current_he = *last_found;
	else
	{
		while (current_he
			   && !current_he->IsMatchingType(get_type, NS_WML)
			   && current_he != GetElm())
		{
			if (!GetElm()->IsAncestorOf(current_he))
			{
				current_he = NULL;
				break;
			}
			else
				current_he = current_he->NextActual();
		}
	}

    if (current_he)
    {
        while (current_he && (current_he->Type() != get_type ||
							  current_he == *last_found))
            current_he = current_he->SucActual();

        if (current_he)
        {
            *out_name = current_he->GetWmlName();
            *out_value = current_he->GetWmlValue();
        }
    }

    *last_found = current_he;
}

BOOL
WMLNewTaskElm::IsNamed(const uni_char *equal_name)
{
	HTML_Element *element = GetElm();
    const uni_char *tmp_name = element->GetWmlName();
	if (!tmp_name && element->IsMatchingType(WE_DO, NS_WML))
		tmp_name = element->GetHtmlOrWmlStringAttr(ATTR_TYPE, WA_TYPE);

    if (tmp_name && equal_name)
        return uni_strcmp(tmp_name, equal_name) == 0;

    return !tmp_name && !equal_name;
}

const uni_char*
WMLNewTaskElm::GetName()
{
    return GetElm()->GetWmlName();
}

/*virtual*/ void
WMLNewTaskElm::OnDelete(FramesDocument *document)
{
	if (document)
	{
		WML_Context *context = document->GetDocManager()->WMLGetContext();
		OP_ASSERT(context);
		context->DeleteTask(this);
	}
}

// ----------------------------------------------------------------------
// the WMLTaskMapElm implementation
// ----------------------------------------------------------------------

WMLTaskMapElm::WMLTaskMapElm(WMLTaskMapElm *src_task_map)
{
	m_task = src_task_map->m_task;
	SetElm(src_task_map->GetElm());
}

/*virtual*/ void
WMLTaskMapElm::OnDelete(FramesDocument *document)
{
	Out();
	OP_DELETE(this);
}

// ----------------------------------------------------------------------
// the WML_Lex implementation
// ----------------------------------------------------------------------

WML_EventType
WML_Lex::GetEventType(const uni_char *evt)
{
    if (evt)
    {
        unsigned int len = uni_strlen(evt);

        if (len == 6 && uni_stri_eq(evt, "ONPICK"))
            return WEVT_ONPICK;
        else if (len == 7 && uni_stri_eq(evt, "ONTIMER"))
            return WEVT_ONTIMER;
        else if (len == 14 && uni_stri_eq(evt, "ONENTERFORWARD"))
            return WEVT_ONENTERFORWARD;
        else if (len == 15 && uni_stri_eq(evt, "ONENTERBACKWARD"))
            return WEVT_ONENTERBACKWARD;
    }

    return WEVT_UNKNOWN;
}


//-----------------------------------------------------------
// class for easy manipulation of internal lists
//-----------------------------------------------------------

WMLStats::WMLStats(int first_in_stssion) :
    m_status(WS_AOK),
	m_variables_changed(FALSE),
	m_first_in_session(first_in_stssion),
    m_var_list(NULL),
	m_active_vars(NULL),
    m_task_list(NULL),
    m_task_map_list(NULL),
    m_event_handlers(NULL),
    m_timer_val(NULL)
{
}

WMLStats::~WMLStats()
{
	OP_DELETEA(m_event_handlers);

	if (m_timer_val)
	{
		m_timer_val->Out(); // take it out of the variable list to stop it from being deleted twice
							// when the variables are deleted. If it's not in the list - whatever
		OP_DELETE(m_timer_val);
	}

	OP_DELETE(m_active_vars);
	OP_DELETE(m_var_list);
	OP_DELETE(m_task_list);
	OP_DELETE(m_task_map_list);
}

OP_STATUS
WMLStats::Copy(WMLStats *src_stats, BOOL merge_active_vars, BOOL include_tasks)
{
	m_status			= src_stats->m_status;
	m_variables_changed	= src_stats->m_variables_changed;

	m_task_list			= NULL;
	m_task_map_list		= NULL;
	m_event_handlers	= NULL;
	m_timer_val			= NULL;

	OP_STATUS oom_stat = OpStatus::OK;

	// copy the variable list
	AutoDeleteList<WMLVariableElm> *src_list = src_stats->m_var_list;
	if (src_list)
	{
		m_var_list = OP_NEW(AutoDeleteList<WMLVariableElm>, ());
		if (!m_var_list)
			return OpStatus::ERR_NO_MEMORY;

		WMLVariableElm *tmp_var = src_list->First();

		while (tmp_var)
		{
			WMLVariableElm *new_var = OP_NEW(WMLVariableElm, ());
			if (!new_var)
				oom_stat = OpStatus::ERR_NO_MEMORY;
			else if (new_var->Copy(tmp_var) == OpStatus::ERR_NO_MEMORY)
			{
				OP_DELETE(new_var);
				oom_stat = OpStatus::ERR_NO_MEMORY;
			}
			else
				new_var->Into(m_var_list);

			tmp_var = tmp_var->Suc();
		}
	}
	else
		m_var_list = NULL;

	src_list = src_stats->m_active_vars;
	if (src_list)
	{
		if (merge_active_vars)
		{
			// merge the active vars with the current vars
			WMLVariableElm *tmp_var = src_list->First();
    		m_variables_changed |= tmp_var != NULL;

			while (tmp_var) // merge the temporary list with the context vars
			{
				const uni_char *tmp_name = tmp_var->GetName();
				int n_len = uni_strlen(tmp_name);

				WMLVariableElm *tmp_tmp_var = tmp_var->Suc();

				if (m_var_list)
				{
					WMLVariableElm *tmp_elm = m_var_list->First();

					while (tmp_elm && ! tmp_elm->IsNamed(tmp_name, n_len))
						tmp_elm = tmp_elm->Suc();

					if (!tmp_elm) // no variable with name exists
					{
						tmp_var->Out(); // take out of temporary list
						tmp_var->Into(m_var_list);
					}
					else // exists, sets new value and deletes temporary
					{
						const uni_char *old_value = tmp_var->GetValue();
						if (OpStatus::IsMemoryError(tmp_elm->SetVal(old_value, uni_strlen(old_value))))
							oom_stat = OpStatus::ERR_NO_MEMORY;
						else
						{
							tmp_var->Out();
							OP_DELETE(tmp_var);
						}
					}
				}
				else
				{
					m_var_list = OP_NEW(AutoDeleteList<WMLVariableElm>, ());

					if (m_var_list)
					{
						tmp_var->Out();
						tmp_var->Into(m_var_list);
					}
					else
						oom_stat = OpStatus::ERR_NO_MEMORY;
				}

				tmp_var = tmp_tmp_var;
			}
		}
		else
		{
			// copy the active vars
			WMLVariableElm *tmp_var = NULL;
			m_active_vars = OP_NEW(AutoDeleteList<WMLVariableElm>, ());
			if (!m_active_vars)
				oom_stat = OpStatus::ERR_NO_MEMORY;
			else
				tmp_var = src_list->First();

			while (tmp_var)
			{
				WMLVariableElm *new_var = OP_NEW(WMLVariableElm, ());
				if (!new_var)
					oom_stat = OpStatus::ERR_NO_MEMORY;
				else if (new_var->Copy(tmp_var) == OpStatus::ERR_NO_MEMORY)
				{
					OP_DELETE(new_var);
					oom_stat = OpStatus::ERR_NO_MEMORY;
				}
				else
					new_var->Into(m_active_vars);

				tmp_var = tmp_var->Suc();
			}
		}
	}
	else
		m_active_vars = NULL;

	if (include_tasks)
	{
		// make event array if needed
		if (src_stats->m_event_handlers)
		{
			m_event_handlers = OP_NEWA(WMLNewTaskElm *, 3);
			if (!m_event_handlers)
				oom_stat = OpStatus::ERR_NO_MEMORY;
			else
				for (int i = 0; i < 3; i++)
					m_event_handlers[i] = NULL;
		}

		// make task map list if needed
		if (src_stats->m_task_map_list)
		{
			m_task_map_list = OP_NEW(AutoDeleteList<WMLTaskMapElm>, ());
			if (!m_task_map_list)
				oom_stat = OpStatus::ERR_NO_MEMORY;
		}

		// Copy tasks
		AutoDeleteList<WMLNewTaskElm>* src_task_list = src_stats->m_task_list;
		if (src_task_list)
		{
			WMLNewTaskElm *tmp_task = NULL;
			m_task_list = OP_NEW(AutoDeleteList<WMLNewTaskElm>, ());
			if (!m_task_list)
				oom_stat = OpStatus::ERR_NO_MEMORY;
			else
				tmp_task = src_task_list->First();

			while (tmp_task)
			{
				WMLNewTaskElm *new_task = OP_NEW(WMLNewTaskElm, (tmp_task));
				if (!new_task)
					oom_stat = OpStatus::ERR_NO_MEMORY;
				else
					new_task->Into(m_task_list);

				// Check if some of the tasks are pointed to by event handlers
				if (m_event_handlers)
				{
					for (int i = 0; i < 3; i++)
					{
						if (src_stats->m_event_handlers[i] == tmp_task)
						{
							m_event_handlers[i] = new_task;
							break;
						}
					}
				}

				// Check if some of the elements are mapped to the copied task
				if (m_task_map_list)
				{
					WMLTaskMapElm *tmp_task_map = src_stats->m_task_map_list->First();

					// go through the mapped elms and see if any of them map to the new task
					while (tmp_task_map)
					{
						if (tmp_task_map->GetTask() == tmp_task)
						{
							WMLTaskMapElm *new_task_map = OP_NEW(WMLTaskMapElm, (new_task, tmp_task_map->GetHElm()));
							if (!new_task_map)
								oom_stat = OpStatus::ERR_NO_MEMORY;
							else
								new_task_map->Into(m_task_map_list);
						}

						tmp_task_map = tmp_task_map->Suc();
					}
				}

				tmp_task = tmp_task->Suc();
			}
		}
	}

	return oom_stat;
}

void
WMLStats::RemoveReferencesToTask(WMLNewTaskElm *task, WML_Context *context)
{
	if (m_event_handlers)
	{
		if (m_event_handlers[WH_TIMER] == task)
		{
			if (context)
				context->RemoveTimer(FALSE);
			m_event_handlers[WH_TIMER] = NULL;
		}

		if (m_event_handlers[WH_FORWARD] == task)
			m_event_handlers[WH_FORWARD] = NULL;

		if (m_event_handlers[WH_BACKWARD] == task)
			m_event_handlers[WH_BACKWARD] = NULL;
	}

	if (m_task_map_list)
	{
		WMLTaskMapElm *map_elm = m_task_map_list->First();
		while (map_elm)
		{
			WMLTaskMapElm *suc_map_elm = map_elm->Suc();
			if (map_elm->GetTask() == task)
			{
				map_elm->Out();
				OP_DELETE(map_elm);
			}
			map_elm = suc_map_elm;
		}
	}
}

// ----------------------------------------------------------------------
// the WML context class
// ----------------------------------------------------------------------

OP_STATUS
WML_Context::SetCurrentCard()
{
	OP_STATUS ret_stat = OpStatus::OK;
	FramesDocument *frm_doc = m_doc_manager ? m_doc_manager->GetCurrentDoc() : NULL;

	// Reevaluate all variable substitutions
	if (m_current_stats->m_variables_changed && m_pending_current_card.GetElm() && frm_doc)
	{
		HTML_Element *iter = m_pending_current_card->FirstChild();
		HTML_Element *stop = m_pending_current_card->NextSibling();
        int subst_buf_len = UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len());
        uni_char *subst_buf = (uni_char *) g_memory_manager->GetTempBuf2();

		while (iter && iter != stop && OpStatus::IsSuccess(ret_stat))
		{
			if (iter->Type() == HE_TEXTGROUP)
			{
				const uni_char *orig_txt
					= iter->GetSpecialStringAttr(ATTR_TEXT_CONTENT,
						SpecialNs::NS_LOGDOC);

				if (orig_txt)
				{
					int new_len = SubstituteVars(orig_txt, uni_strlen(orig_txt),
						subst_buf, subst_buf_len, FALSE, NULL);

					ret_stat = iter->SetText(frm_doc, subst_buf, new_len);
				}
			}

			iter = iter->Next();
		}
	}

	if (frm_doc)
		if (HLDocProfile *hld_profile = frm_doc->GetHLDocProfile())
			hld_profile->WMLSetCurrentCard(m_pending_current_card.GetElm());

	return ret_stat;
}

WML_Context::WML_Context(DocumentManager *dm/* = NULL*/) :
	m_old_stats(NULL),
	m_tmp_stats(NULL),
	m_current_stats(NULL),
	m_preparse_called(FALSE),
	m_postparse_called(FALSE),
    m_doc_manager(dm),
	m_substitute_buffer(NULL),
	m_substitute_buffer_len(0),
	m_refs(0)
{
}

OP_STATUS
WML_Context::Init()
{
	m_current_stats = OP_NEW(WMLStats, (-1));
	if (!m_current_stats)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/*static*/ BOOL
WML_Context::IsHtmlElement(WML_ElementType type)
{
	switch (type)
	{
	case Markup::HTE_A:
	case Markup::HTE_B:
	case Markup::HTE_BIG:
	case Markup::HTE_BR:
	case Markup::HTE_DIV:
	case Markup::HTE_EM:
	case Markup::HTE_FIELDSET:
	case Markup::HTE_I:
	case Markup::HTE_IMG:
	case Markup::HTE_INPUT:
	case Markup::HTE_META:
	case Markup::HTE_OPTION:
	case Markup::HTE_P:
	case Markup::HTE_PRE:
	case Markup::HTE_SCRIPT:
	case Markup::HTE_SELECT:
	case Markup::HTE_SMALL:
	case Markup::HTE_STRONG:
	case Markup::HTE_TABLE:
	case Markup::HTE_TD:
	case Markup::HTE_TR:
	case Markup::HTE_U:
		return TRUE;
	}

	return FALSE;
}

/*static*/ BOOL
WML_Context::IsWmlAttribute(WML_AttrType type)
{
	switch (type)
	{
	case Markup::WMLA_MODE:
	case Markup::WMLA_PATH:
	case Markup::WMLA_INAME:
	case Markup::WMLA_FORUA:
	case Markup::WMLA_ONPICK:
	case Markup::WMLA_FORMAT:
	case Markup::WMLA_IVALUE:
	case Markup::WMLA_DOMAIN:
	case Markup::WMLA_EMPTYOK:
	case Markup::WMLA_ONTIMER:
	case Markup::WMLA_ORDERED:
	case Markup::WMLA_LOCALSRC:
	case Markup::WMLA_OPTIONAL:
	case Markup::WMLA_NEWCONTEXT:
	case Markup::WMLA_SENDREFERER:
	case Markup::WMLA_CACHE_CONTROL:
	case Markup::WMLA_ONENTERFORWARD:
	case Markup::WMLA_ONENTERBACKWARD:
		return TRUE;
	}

	return FALSE;
}

/*static*/ BOOL
WML_Context::IsHtmlAttribute(WML_AttrType type)
{
	switch (type)
	{
	case Markup::WMLA_ID:
	case Markup::WMLA_HREF:
	case Markup::WMLA_TYPE:
	case Markup::WMLA_NAME:
	case Markup::WMLA_MODE:
	case Markup::WMLA_PATH:
	case Markup::WMLA_CLASS:
	case Markup::WMLA_TITLE:
	case Markup::WMLA_VALUE:
	case Markup::WMLA_LABEL:
	case Markup::WMLA_INAME:
	case Markup::WMLA_FORUA:
	case Markup::WMLA_METHOD:
	case Markup::WMLA_ONPICK:
	case Markup::WMLA_FORMAT:
	case Markup::WMLA_IVALUE:
	case Markup::WMLA_DOMAIN:
	case Markup::WMLA_EMPTYOK:
	case Markup::WMLA_ONTIMER:
	case Markup::WMLA_ENCTYPE:
	case Markup::WMLA_ORDERED:
	case Markup::WMLA_TABINDEX:
	case Markup::WMLA_MULTIPLE:
	case Markup::WMLA_LOCALSRC:
	case Markup::WMLA_OPTIONAL:
	case Markup::WMLA_ACCESSKEY:
	case Markup::WMLA_NEWCONTEXT:
	case Markup::WMLA_SENDREFERER:
	case Markup::WMLA_CACHE_CONTROL:
	case Markup::WMLA_ONENTERFORWARD:
	case Markup::WMLA_ACCEPT_CHARSET:
	case Markup::WMLA_ONENTERBACKWARD:
		return FALSE;
	}

	return TRUE;
}

OP_STATUS
WML_Context::Copy(WML_Context *src_wc, DocumentManager* doc_man /* = NULL*/)
{
    if (src_wc)
    {
		if (doc_man)
			m_doc_manager = doc_man;
		else
			m_doc_manager = src_wc->GetDocManager();

		m_current_stats = OP_NEW(WMLStats, (-1));
		if (!m_current_stats)
			return OpStatus::ERR_NO_MEMORY;

		return m_current_stats->Copy(src_wc->m_current_stats, FALSE, FALSE);
    }

    return OpStatus::OK;
}

WML_Context::~WML_Context()
{
	if (m_doc_manager)
	{
		MessageHandler *mh = m_doc_manager->GetWindow()->GetMessageHandler();
		if (mh)
		{
			mh->RemoveDelayedMessage(MSG_WML_TIMER, (MH_PARAM_1)this, 0);
			mh->UnsetCallBacks(this);
		}

		if (m_pending_current_card.GetElm() && m_doc_manager->GetCurrentDoc())
			if (HLDocProfile *hld_profile = m_doc_manager->GetCurrentDoc()->GetHLDocProfile())
				if (hld_profile->WMLGetCurrentCard() == m_pending_current_card.GetElm())
					hld_profile->WMLSetCurrentCard(NULL);
	}

	OP_DELETE(m_old_stats);
	OP_DELETE(m_tmp_stats);
	OP_DELETE(m_current_stats);
	OP_DELETEA(m_substitute_buffer);
}

// Creates a new task element in the context.
// If a task with the same name exists, it is replaced
// Do not delete the returned element.
// ----------------------------------------------------------------------
WMLNewTaskElm*
WML_Context::NewTask(HTML_Element *task_he)
{
    WMLNewTaskElm *existing_task;
    const uni_char *task_name = task_he->GetWmlName();

	if (!task_name && task_he->IsMatchingType(WE_DO, NS_WML))
		task_name = task_he->GetHtmlOrWmlStringAttr(ATTR_TYPE, WA_TYPE);

    if (task_name && !uni_stri_eq(task_name, "UNKNOWN"))
        existing_task = GetTask(task_name);
    else
        existing_task = NULL;

    if (existing_task) // a task already exists, reinitialize it with new task values
    {
 		if (!m_current_stats->m_task_map_list || !m_current_stats->m_task_list)
			return NULL;

		// create new task element
        WMLNewTaskElm* new_task = OP_NEW(WMLNewTaskElm, (task_he));
        if (! new_task)
			return NULL;

		new_task->Into(m_current_stats->m_task_list);

		// remap all html_elements associated with old task to the new task
		WMLTaskMapElm *next_map_elm = m_current_stats->m_task_map_list->First();

		while (next_map_elm)
		{
			WMLTaskMapElm* elm_to_free = next_map_elm;
			next_map_elm = next_map_elm->Suc();

			if (elm_to_free->GetTask() == existing_task)
			{
				elm_to_free->Out();
				SetTaskByElement(new_task, elm_to_free->GetHElm() );
				//FIXME:OOM What if setting the task fails?
				OP_DELETE(elm_to_free);
			}
		}

		// remove old task (shadowed)
		existing_task->Out();
		OP_DELETE(existing_task);

		return new_task;
	}
	else
    {
        if (!m_current_stats->m_task_list)
            m_current_stats->m_task_list = OP_NEW(AutoDeleteList<WMLNewTaskElm>, ());

        if (m_current_stats->m_task_list)
        {
			WMLNewTaskElm *new_task = OP_NEW(WMLNewTaskElm, (task_he));
			if (new_task)
				new_task->Into(m_current_stats->m_task_list);

			return new_task;
		}
    }

    return NULL;
}

// Returns the task with name task_name, or NULL if it does not exist
WMLNewTaskElm*
WML_Context::GetTask(const uni_char *task_name)
{
    if (m_current_stats->m_task_list)
    {
        WMLNewTaskElm *tmp_elm = m_current_stats->m_task_list->First();

        while (tmp_elm && !tmp_elm->IsNamed(task_name))
            tmp_elm = tmp_elm->Suc();

        return tmp_elm;
    }

    return NULL;
}

// Resets the context.
// Clears all variables and tasks and resets the status.
// ----------------------------------------------------------------------
OP_STATUS
WML_Context::SetNewContext(int new_first/*=-1*/)
{
	if (!IsSet(WS_ENTERBACK) && !IsSet(WS_REFRESH))
	{
		UINT32 old_status = m_current_stats->m_status;

		WMLStats *new_stats = OP_NEW(WMLStats, (new_first));
		if (!new_stats)
			return OpStatus::ERR_NO_MEMORY;

		OP_DELETE(m_current_stats);

		m_current_stats = new_stats;
		m_current_stats->m_status = old_status;

		SetStatusOn(WS_NEWCONTEXT); // we need to clear history when finished
	}

	return OpStatus::OK;
}

// Returns the task corresponding to the event event.
// ----------------------------------------------------------------------
// IN       event   - the event you want the handler of
// RETURN   the task associated with the event, NULL if none
WMLNewTaskElm*
WML_Context::GetEventHandler(WML_EventType event)
{
    if (m_current_stats->m_event_handlers)
    {
        switch (event)
        {
        case WEVT_ONTIMER:
            return m_current_stats->m_event_handlers[WH_TIMER];

        case WEVT_ONENTERFORWARD:
            return m_current_stats->m_event_handlers[WH_FORWARD];

        case WEVT_ONENTERBACKWARD:
            return m_current_stats->m_event_handlers[WH_BACKWARD];

		default:
			break;
		}
    }

    return NULL;
}

// sets the valid event handler for the event event.
// deletes the current one if it exists
// ----------------------------------------------------------------------
// IN       event   - the type of event
//          handler - the task to perform for that event
OP_STATUS
WML_Context::SetEventHandler(WML_EventType event, WMLNewTaskElm *handler)
{
    int handler_idx = 0;

    switch (event)
    {
    case WEVT_ONTIMER:
        handler_idx = WH_TIMER;
        break;

    case WEVT_ONENTERFORWARD:
        handler_idx = WH_FORWARD;
        break;

    case WEVT_ONENTERBACKWARD:
        handler_idx = WH_BACKWARD;
        break;

	default:
		break;
	}

    if (!m_current_stats->m_event_handlers)
    {
        m_current_stats->m_event_handlers = OP_NEWA(WMLNewTaskElm *, 3);
        if (!m_current_stats->m_event_handlers)
            return OpStatus::ERR_NO_MEMORY;

        m_current_stats->m_event_handlers[WH_TIMER] = NULL;
        m_current_stats->m_event_handlers[WH_FORWARD] = NULL;
        m_current_stats->m_event_handlers[WH_BACKWARD] = NULL;
    }
    else if (m_current_stats->m_event_handlers[handler_idx]) // we don't need it anymore
    {
		DeleteTask(m_current_stats->m_event_handlers[handler_idx]);
    }

    m_current_stats->m_event_handlers[handler_idx] = handler;

    return OpStatus::OK;
}

// handles callbacks (for now only timer)
// ******************************************************
// IN   msg     - the message MSG_WML_TIMER
//      par1    - this
//      par2    - unused
void
WML_Context::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    if (msg != MSG_WML_TIMER || par1 != (MH_PARAM_1)this || ! IsSet(WS_TIMING) || GetDocManager()->WMLGetContext() != this)
        return;

    if (m_current_stats->m_event_handlers && m_current_stats->m_event_handlers[WH_TIMER])
	{
		BOOL noop = TRUE;
		OP_STATUS status = PerformTask(m_current_stats->m_event_handlers[WH_TIMER], noop, FALSE, WEVT_ONTIMER);
		if (OpStatus::IsMemoryError(status))
			g_memory_manager->RaiseCondition(status);
	}
    else
	{
        m_current_stats->m_timer_val->SetVal(UNI_L("0"), 1);
		m_current_stats->m_variables_changed = TRUE;
	}
}

// sets the timer
// ******************************************************
// IN   name    - name of the timer variable
//      time    - the time until the event happens in
//              1/10 of a second
OP_STATUS
WML_Context::SetTimer(const uni_char *name, const uni_char *timestr)
{
	if (!timestr)
		return OpStatus::ERR;

    if (name && *name)
    {
        const uni_char *tmp_time;

        if (!(tmp_time = GetVariable(name)))
            tmp_time = timestr;

		if (!uni_isdigit(*tmp_time))
			return OpStatus::ERR;

        if (m_current_stats->m_timer_val == NULL)
        {
            m_current_stats->m_timer_val = SetVariable(name, tmp_time);
            if (! m_current_stats->m_timer_val)
                return OpStatus::ERR_NO_MEMORY;
        }
        else
        {
            if (m_current_stats->m_timer_val->IsNamed(name, uni_strlen(name)))
			{
                RETURN_IF_MEMORY_ERROR(m_current_stats->m_timer_val->SetVal(tmp_time, uni_strlen(tmp_time)));
				m_current_stats->m_variables_changed = TRUE;
			}
            else
            {
                m_current_stats->m_timer_val->Out(); // remove the non-valid timer
                m_current_stats->m_timer_val = SetVariable(name, tmp_time);

                if (! m_current_stats->m_timer_val)
                    return OpStatus::ERR_NO_MEMORY;
            }
        }
    }
    else // no named variable
    {
        if (m_current_stats->m_timer_val != NULL)
            m_current_stats->m_timer_val = NULL;

		if (!uni_isdigit(*timestr))
			return OpStatus::ERR;

        m_current_stats->m_timer_val = OP_NEW(WMLVariableElm, ());
        if (m_current_stats->m_timer_val)
        {
            RETURN_IF_MEMORY_ERROR(m_current_stats->m_timer_val->SetName(UNI_L(""), 0)); // no name, do not put in m_var_list

            RETURN_IF_MEMORY_ERROR(m_current_stats->m_timer_val->SetVal(timestr, uni_strlen(timestr)));
			m_current_stats->m_variables_changed = TRUE;
        }
        else
            return OpStatus::ERR_NO_MEMORY;
    }

    if (uni_str_eq(timestr, "0")) // stop and remove the timer
        RemoveTimer(TRUE);

    return OpStatus::OK;
}

void
WML_Context::DeleteTask(WMLNewTaskElm *task)
{
	if(!task)
		return;

	task->Out();

	if (m_current_stats)
		m_current_stats->RemoveReferencesToTask(task, this);

	if (m_old_stats)
		m_old_stats->RemoveReferencesToTask(task, NULL);

	if (m_tmp_stats)
		m_tmp_stats->RemoveReferencesToTask(task, NULL);

	OP_DELETE(task);
}

void
WML_Context::RemoveTimer(BOOL delete_task)
{
	SetStatusOff(WS_TIMING);

    MessageHandler *mh = m_doc_manager->GetWindow()->GetMessageHandler();
	if (mh)
		mh->RemoveDelayedMessage(MSG_WML_TIMER, (MH_PARAM_1)this , 0);

    if (m_current_stats->m_event_handlers
		&& m_current_stats->m_event_handlers[WH_TIMER])
    {
		if (delete_task)
			DeleteTask(m_current_stats->m_event_handlers[WH_TIMER]);

        if (m_current_stats->m_timer_val)
        {
			m_current_stats->m_timer_val->Out();
			OP_DELETE(m_current_stats->m_timer_val);
            m_current_stats->m_timer_val = NULL;
        }
    }
}

// starts the timer
// ----------------------------------------------------------------------
// IN   start   - TRUE for start, FALSE for stop
void
WML_Context::StartTimer(BOOL start)
{
    MessageHandler *mh = m_doc_manager->GetWindow()->GetMessageHandler();
    if (!mh)
        return;

    if (start && m_current_stats->m_timer_val)
    {
        int time_val = uni_atoi(m_current_stats->m_timer_val->GetValue());
        // TODO: send the start time as one of the parameters so that we can calculate
        //       how long the timer has been ticking when we turn it off again. This
        //       value should be put in the corresponding variable when navigating to
        //       another card/page. (stighal 2001-10-19)
        RAISE_IF_ERROR(mh->SetCallBack(this, MSG_WML_TIMER, (MH_PARAM_1)this));//FIXME:OOM - Can't return OOM error.
        mh->PostDelayedMessage(MSG_WML_TIMER, (MH_PARAM_1)this, 0, time_val * 100);
        SetStatusOn(WS_TIMING);
    }
    else
		RemoveTimer(TRUE);
}

// performs a WML task, carrying out the navigation
// ----------------------------------------------------------------------
// IN   task    - the task element to perform
//      event   - the type of event that triggered this task
OP_STATUS
WML_Context::PerformTask(WMLNewTaskElm *task, BOOL& noop, BOOL is_user_requested, WML_EventType event)
{
	noop = TRUE;
    if (!task)
        return OpStatus::OK;

	UINT32 action = 0;
    URL url = GetWmlUrl(task->GetHElm(), &action, event);
	SetStatusOn(action);

	if (IsSet(WS_NOOP))
		SetStatusOff(WS_NOOP);
	else
	{
		// this have to be done after the substitution to
		// avoid the temporary variables to be used
		OP_STATUS oom_stat = SetActiveTask(task);

		// stopping the timer has to be done before we return but after all use of task since it will be deleted
		if (IsSet(WS_TIMING))
			StartTimer(FALSE);

		if (OpStatus::IsMemoryError(oom_stat))
			return oom_stat;

		if (IsSet(WS_REFRESH))
		{
			m_doc_manager->GetMessageHandler()->PostMessage(MSG_WML_REFRESH, url.Id(TRUE), is_user_requested);
		}
		else if (IsSet(WS_ENTERBACK))
		{
			if (m_doc_manager->GetWindow()->HasHistoryPrev())
				m_doc_manager->GetWindow()->GetMessageHandler()
					->PostMessage(MSG_HISTORY_BACK, url.Id(TRUE), 0);
			else
			{
				noop = TRUE;
				return OpStatus::OK;
			}
		}
		else
		{
			// the actual loading of the document
			m_doc_manager->SetUrlLoadOnCommand(url,
				DocumentReferrer(m_doc_manager->GetCurrentURL()), FALSE,
				event == WEVT_ONCLICK || event == WEVT_ONPICK);
			m_doc_manager->GetMessageHandler()->PostMessage(MSG_URL_LOAD_NOW,
				url.Id(TRUE), event == WEVT_ONCLICK || event == WEVT_ONPICK);
		}

		noop = FALSE;
	}

    return OpStatus::OK;
}

// Sets the value of the WML variable called name
// ----------------------------------------------------------------------
// IN       name    - name of variable
//          value   - value of variable
// RETURN   A pointer to the new variable element
WMLVariableElm*
WML_Context::SetVariable(const uni_char *name, const uni_char *value)
{
	if (!name)
		return NULL;

    if (!m_current_stats->m_var_list)
    {
        m_current_stats->m_var_list = OP_NEW(AutoDeleteList<WMLVariableElm>, ());

        if (!m_current_stats->m_var_list)
            return NULL;
    }

	int n_len = uni_strlen(name);
    WMLVariableElm *tmp_elm = m_current_stats->m_var_list->First();

    while (tmp_elm && !tmp_elm->IsNamed(name, n_len))
        tmp_elm = tmp_elm->Suc();

    if (!tmp_elm) // no variable with name exists
    {
        tmp_elm = OP_NEW(WMLVariableElm, ());

		if (tmp_elm)
		{
			if (OpStatus::IsError(tmp_elm->SetName(name, n_len)))
			{
				OP_DELETE(tmp_elm);
				return NULL;
			}
			if (OpStatus::IsError(tmp_elm->SetVal(value, uni_strlen(value))))
			{
				OP_DELETE(tmp_elm);
				return NULL;
			}
			tmp_elm->Into(m_current_stats->m_var_list);
			m_current_stats->m_variables_changed = TRUE;
		}
    }
    else // exists, sets new value
    {
        if (OpStatus::IsMemoryError(tmp_elm->SetVal(value, uni_strlen(value))))
            return NULL;
		m_current_stats->m_variables_changed = TRUE;
    }

    return tmp_elm;
}

// Gets the value of the WML variable called name
// ----------------------------------------------------------------------
// IN       name    - name of variable
//          n_len   - length of name, if -1 uni_strlen is used
// RETURN   a pointer to the a buffer of the value, null-terminated
const uni_char*
WML_Context::GetVariable(const uni_char *name, int n_len)
{
    if (n_len == -1)
        n_len = uni_strlen(name);

    if (m_current_stats->m_active_vars)
    {
        WMLVariableElm *tmp_elm = m_current_stats->m_active_vars->First();

        while (tmp_elm && ! tmp_elm->IsNamed(name, n_len))
            tmp_elm = tmp_elm->Suc();

        if (tmp_elm)
            return tmp_elm->GetValue();
    }

    if (m_current_stats->m_var_list)
    {
        WMLVariableElm *tmp_elm = m_current_stats->m_var_list->First();

        while (tmp_elm && ! tmp_elm->IsNamed(name, n_len))
            tmp_elm = tmp_elm->Suc();

        if (tmp_elm)
            return tmp_elm->GetValue();
    }

    return NULL;
}

OP_STATUS
WML_Context::DenyAccess()
{
	OpString title, content;
	TRAP_AND_RETURN(oom_status, g_languageManager->GetStringL(Str::S_WML_ACCESS_DENIED, title));
	TRAP_AND_RETURN(oom_status, g_languageManager->GetStringL(Str::S_WML_INCORRECT_ORDER, content));

	OpString msg;
	RETURN_IF_MEMORY_ERROR(msg.AppendFormat(UNI_L("%s:\n\n%s"), title.CStr(), content.CStr()));

	m_doc_manager->GetWindow()->GetWindowCommander()->GetDocumentListener()
		->OnWmlDenyAccess(m_doc_manager->GetWindow()->GetWindowCommander());

	if (m_old_stats)
	{
		OP_DELETE(m_current_stats);
		m_current_stats = m_old_stats;
		m_old_stats = NULL;
	}
	else
	{
		WMLStats *new_stats = OP_NEW(WMLStats, (-1));
		if (!new_stats)
			return OpStatus::ERR_NO_MEMORY;

		OP_DELETE(m_current_stats);

		m_current_stats = new_stats;
	}

	m_preparse_called = FALSE; // reset this since we will show another card

	return OpStatus::OK;
}

// Must be called before a WML document is parsed
OP_STATUS
WML_Context::PreParse()
{
	if (!m_preparse_called)
	{
		m_preparse_called = TRUE;
		m_postparse_called = FALSE;

		// remove any old timers running
		if (m_current_stats)
			RemoveTimer(TRUE);

		WMLStats *new_stats = OP_NEW(WMLStats, (-1));
		if (!new_stats)
			return OpStatus::ERR_NO_MEMORY;

		m_old_stats = m_current_stats;
		m_current_stats = new_stats;

		if (m_old_stats)
			RETURN_IF_MEMORY_ERROR(m_current_stats->Copy(m_old_stats, TRUE, FALSE));

	    SetStatus((m_current_stats->m_status & (WS_ENTERBACK | WS_CLEANHISTORY | WS_REFRESH | WS_GO)) | WS_FIRSTCARD);

		SetPendingCurrentCard(NULL);
	}

	return OpStatus::OK;
}

// Must be called after a WML document has been parsed
OP_STATUS
WML_Context::PostParse()
{
	if (m_postparse_called)
		return OpStatus::OK;

	m_postparse_called = TRUE;
	m_preparse_called = FALSE;

	if (m_substitute_buffer || m_substitute_buffer_len)
	{
		OP_DELETEA(m_substitute_buffer);
		m_substitute_buffer_len = 0;
		m_substitute_buffer = NULL;
	}

    if (IsSet(WS_NOACCESS))
		return DenyAccess();

    // from here on out consider navigation successfull!
    OP_STATUS oom_stat = OpStatus::OK;

	if (IsSet(WS_NEWCONTEXT))
		m_doc_manager->WMLDeWmlifyHistory(TRUE);

	// delete the old stats from previous card
	OP_DELETE(m_old_stats);
	m_old_stats = NULL;
	OP_DELETE(m_tmp_stats);
	m_tmp_stats = NULL;

    RETURN_IF_MEMORY_ERROR(SetActiveTask(NULL));

    // perform entry events
    if (m_current_stats->m_event_handlers)
    {
        if (m_current_stats->m_event_handlers[WH_BACKWARD] && IsSet(WS_ENTERBACK))
        {
			BOOL noop;
			SetStatus(m_current_stats->m_status & ~(WS_ENTERBACK | WS_CLEANHISTORY));
            RETURN_IF_ERROR(PerformTask(m_current_stats->m_event_handlers[WH_BACKWARD], noop, FALSE, WEVT_ONENTERBACKWARD));

			if (!noop)
				return OpStatus::OK;
        }
        else if (m_current_stats->m_event_handlers[WH_FORWARD] && ! IsSet(WS_ENTERBACK) && ! IsSet(WS_REFRESH))
        {
			BOOL noop;
			SetStatus(m_current_stats->m_status & ~(WS_ENTERBACK | WS_CLEANHISTORY));
            RETURN_IF_ERROR(PerformTask(m_current_stats->m_event_handlers[WH_FORWARD], noop, FALSE, WEVT_ONENTERFORWARD));

			if (!noop)
				return OpStatus::OK;
        }

        if (m_current_stats->m_event_handlers[WH_TIMER])
            StartTimer(TRUE); // only start timer if not redirecting
    }

	SetCurrentCard();

	SetStatus(m_current_stats->m_status & ~(WS_NEWCONTEXT | WS_ENTERBACK | WS_CLEANHISTORY | WS_REFRESH | WS_GO));

	oom_stat = m_doc_manager->UpdateWindowHistoryAndTitle();

    return oom_stat;
}

// We have found out that the current card is not the target for this
// navigation and we have to clear the tasks in the old, wrong card
void
WML_Context::ScrapTmpCurrentCard()
{
    RemoveTimer(TRUE);
	SetStatusOff(WS_NEWCONTEXT); // if set, it was for the wrong card so we turn it off

	if (m_tmp_stats)
	{
		OP_DELETE(m_current_stats);
		m_current_stats = m_tmp_stats;
		m_tmp_stats = NULL;
	}
}

OP_STATUS
WML_Context::PushTmpCurrentCard()
{
	OP_DELETE(m_tmp_stats);

	m_tmp_stats = OP_NEW(WMLStats, (-1));
	if (!m_tmp_stats)
		return OpStatus::ERR_NO_MEMORY;

	return m_tmp_stats->Copy(m_current_stats, FALSE, TRUE);
}

/*static*/ BOOL
WML_Context::NeedSubstitution(const uni_char *s, size_t s_len)
{
	size_t idx = 0;
	while (idx < s_len)
	{
		if (s[idx] == '$')
			return TRUE;
		idx++;
	}

	return FALSE;
}

OP_STATUS
WmlConvertVariable(OpString8 &str, const uni_char *val, OutputConverter *converter, DocumentManager *doc_man)
{
	OP_STATUS enc_err;
	if (!converter)
	{
		const char *encoding = doc_man->GetCurrentDoc()
			->GetHLDocProfile()->GetCharacterSet();
		TRAP(enc_err, SetToEncodingL(&str, encoding, val));
	}
	else
	{
		TRAP(enc_err, SetToEncodingL(&str, converter, val));
	}

	return enc_err;
}

OP_STATUS
WML_Context::RestartParsing()
{
	if (m_old_stats)
	{
		OP_DELETE(m_current_stats);
		m_current_stats = m_old_stats;
		m_old_stats = NULL;
	}
	else
	{
		WMLStats *new_stats = OP_NEW(WMLStats, (-1));
		if (!new_stats)
			return OpStatus::ERR_NO_MEMORY;

		OP_DELETE(m_current_stats);

		m_current_stats = new_stats;
	}

	m_preparse_called = FALSE;

	return PreParse();
}


// Substitutes variables in a block of WML text
// -----------------------------------------------------------
// IN:      s		- the original text
//          s_len	- the length of the original text
//			len_tb	- length of tmp_buf
//          converter- the character converter used when substituting with URL escaping
//          esc_by_default  - used to indicate that escaping is default
// OUT:		tmp_buf	- a buffer to hold the substituted text (use the TmpBufs).
//			can return another buffer than the one sent in if result is too large
// RETURN:	the length of the substituted string
int
WML_Context::SubstituteVars(const uni_char *s, int s_len,
							uni_char *&tmp_buf, int len_tb,
							BOOL esc_by_default/*=FALSE*/,
							OutputConverter *converter/*=NULL*/)
{
	OP_STATUS status = OpStatus::OK;

	uni_char* dest_buf = tmp_buf;
	int dest_buf_len = len_tb;

    int conversion = esc_by_default ? WML_CONV_ESC : WML_CONV_NONE;
    int idx_s = 0; // index into s
    int idx_tb = 0; // index into TempBuf
    int var_start = -1; // index where variable starts, initialized to -1 to handle following a single $ block
    BOOL invar = IsSet(WS_INVAR);

    // TODO: fix the "in variable"-problem
    if (invar) // variable started in a previous call
        var_start = 0; // to make

    do
    {
        while (idx_s < s_len && !invar && s[idx_s] != '$') // scan until var is found
		{
			if ((status = GrowSubstituteBufferIfNeeded(dest_buf, dest_buf_len, idx_tb)) != OpStatus::OK)
				goto error_while_substituting;
            dest_buf[idx_tb++] = s[idx_s++];
		}

        if ((idx_s < s_len) && (s[idx_s] == '$' || invar)) // variable found (or double dollar)
        {
            if (invar)
            {
                if ((s[idx_s] == '$') && (idx_s == var_start)) // double dollar
                {
					if ((status = GrowSubstituteBufferIfNeeded(dest_buf, dest_buf_len, idx_tb)) != OpStatus::OK)
						goto error_while_substituting;
					dest_buf[idx_tb++] = s[idx_s++];
                    invar = FALSE;
                    continue;
                }

                var_start = 0;
            }
            else
            {
                var_start = ++idx_s;
                invar = TRUE;
            }

            BOOL in_par = FALSE;

            // handle variables enclosed in parentheses
            if (s[idx_s] == '(')
            {
                idx_s++;
                var_start++;
                in_par = TRUE;
            }

            int var_stop = var_start;

            if (in_par) // parentheses
            {
				if (uni_isalpha(s[idx_s]) || s[idx_s] == '_')
					while( (idx_s < s_len) && (uni_isalnum(s[idx_s]) || s[idx_s] == '_') )
						idx_s++;

                var_stop = idx_s;

				if (s[idx_s] == ':' && var_start < var_stop) // conversion
				{
					idx_s++;
					int conv_start = idx_s;

					while( idx_s < s_len && uni_isalpha(s[idx_s]) ) // conversion
						idx_s++;

					if (uni_strni_eq(&s[conv_start], "ESCAPE", idx_s - conv_start))
						conversion = WML_CONV_ESC;
					else if (uni_strni_eq(&s[conv_start], "UNESC", idx_s - conv_start))
						conversion = WML_CONV_UNESC;
					else
						conversion = WML_CONV_NONE;
				}

                if (s[idx_s] == ')')
                    idx_s++;

                in_par = FALSE;
                invar = FALSE;
            }
            else
            {
				if (uni_isalpha(s[idx_s]) || s[idx_s] == '_')
					while( (idx_s < s_len) && (op_isalnum(s[idx_s]) || s[idx_s] == '_') )
						idx_s++;

                var_stop = idx_s;
            }


            if (var_stop > var_start) // there is a variable name
            {
                const uni_char *tmp_str = GetVariable(&s[var_start], var_stop - var_start);

                if (tmp_str) // the variable is set
                {
					int val_len = uni_strlen(tmp_str);

					switch (conversion)
					{
					case WML_CONV_UNESC:
						if ((status = GrowSubstituteBufferIfNeeded(dest_buf, dest_buf_len, idx_tb + val_len - 1)) != OpStatus::OK)
							goto error_while_substituting;
						idx_tb += UriUnescape::Unescape(dest_buf+idx_tb, tmp_str, UriUnescape::All);
						break;

					case WML_CONV_ESC:
						{
							// we cannot use the global tempbuffer anymore
							// since SetToEncoding() also uses it
							if (dest_buf != m_substitute_buffer)
							{
								int use_len = idx_tb;
								if ((status = GrowSubstituteBufferIfNeeded(dest_buf, use_len, use_len + (s_len - idx_s))) != OpStatus::OK)
									goto error_while_substituting;
								dest_buf_len = use_len;
							}

							OpString8 encoded_val;
							status = WmlConvertVariable(encoded_val, tmp_str, converter, m_doc_manager);

							if (OpStatus::IsMemoryError(status))
								goto error_while_substituting;

							int encoded_val_len = encoded_val.Length();
							char *encoded_val_buf = encoded_val.CStr();
							int escaped_len = UriEscape::GetEscapedLength(encoded_val_buf, encoded_val_len, UriEscape::WMLUnsafe | UriEscape::UsePlusForSpace_DEF);

							if ((status = GrowSubstituteBufferIfNeeded(dest_buf, dest_buf_len, idx_tb + escaped_len - 1)) != OpStatus::OK)
								goto error_while_substituting;
							idx_tb += UriEscape::Escape(dest_buf+idx_tb, encoded_val_buf, encoded_val_len, UriEscape::WMLUnsafe | UriEscape::UsePlusForSpace_DEF);
						}
                        break;

					default:
						if ((status = GrowSubstituteBufferIfNeeded(dest_buf, dest_buf_len, idx_tb + val_len + 1)) != OpStatus::OK)
							goto error_while_substituting;

                        uni_strcpy(&dest_buf[idx_tb], tmp_str);
                        idx_tb += val_len;
						break;
					}
                }

                invar = FALSE;
            }
			else if (invar && idx_s != s_len && s[idx_s] != '$')
				invar = FALSE;
        }
    }
	while (idx_s < s_len);

	goto done_substituting;

error_while_substituting:
	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);

done_substituting:
	if (OpStatus::IsMemoryError(GrowSubstituteBufferIfNeeded(dest_buf, dest_buf_len, idx_tb)))
		g_memory_manager->RaiseCondition(status);
    dest_buf[idx_tb] = '\0'; // terminate the string

	if (invar)
		SetStatusOn(WS_INVAR);
	else
		SetStatusOff(WS_INVAR);

	tmp_buf = dest_buf; //In case buffer has grown, and moved to another address
    return idx_tb;
}

OP_STATUS
WML_Context::GrowSubstituteBufferIfNeeded(uni_char*& buffer, int& length, int wanted_index)
{
	if (wanted_index < length) //Buffer is large enough. Do nothing
		return OpStatus::OK;

	// Try to reuse the current m_substitute_buffer if possible, otherwise allocate a new buffer
	if (buffer != m_substitute_buffer && m_substitute_buffer && m_substitute_buffer_len >= wanted_index)
	{
		if (buffer && length > 0) // Keep any previous content
			op_memcpy(m_substitute_buffer, buffer, length * sizeof(uni_char));
	}
	else
	{
		int new_length = static_cast<int>(wanted_index*1.2)+1; //Grow by 20% (and 1, just to make sure we grow even for small values..)
		uni_char* new_buffer = OP_NEWA(uni_char, new_length);
		if (!new_buffer)
			return OpStatus::ERR_NO_MEMORY;

		if (buffer && length > 0) // Keep any previous content
			op_memcpy(new_buffer, buffer, length * sizeof(uni_char));

		OP_DELETEA(m_substitute_buffer);
		m_substitute_buffer = new_buffer;
		m_substitute_buffer_len = new_length;
	}


	buffer = m_substitute_buffer;
	length = m_substitute_buffer_len;

	return OpStatus::OK;
}

// Gets the task mapped to he
WMLNewTaskElm*
WML_Context::GetTaskByElement(HTML_Element *he)
{
    if (!he || !m_current_stats->m_task_map_list)
        return NULL;

    WMLTaskMapElm *tmp_map_elm = m_current_stats->m_task_map_list->First();

    while (tmp_map_elm && !tmp_map_elm->BelongsTo(he))
        tmp_map_elm = tmp_map_elm->Suc();

    if (tmp_map_elm)
        return tmp_map_elm->GetTask();

    return NULL;
}

// Maps the task new_task to he
OP_STATUS
WML_Context::SetTaskByElement(WMLNewTaskElm *new_task, HTML_Element *he)
{
    if (!m_current_stats->m_task_map_list)
    {
        m_current_stats->m_task_map_list = OP_NEW(AutoDeleteList<WMLTaskMapElm>, ());
        if (!m_current_stats->m_task_map_list)
            return OpStatus::ERR_NO_MEMORY;
    }

    WMLTaskMapElm *next_map_elm = m_current_stats->m_task_map_list->First();

    while (next_map_elm && ! next_map_elm->BelongsTo(he))
        next_map_elm = next_map_elm->Suc();

    if (next_map_elm)
        next_map_elm->SetTask(new_task);
    else
    {
        next_map_elm = OP_NEW(WMLTaskMapElm, (new_task, he));
        if (!next_map_elm)
            return OpStatus::ERR_NO_MEMORY;

        next_map_elm->Into(m_current_stats->m_task_map_list);
    }

	// hide elements bound to NOOP tasks
	if (he->IsMatchingType(WE_NOOP, NS_WML))
	{
		WMLTaskMapElm *existing_map_elm = m_current_stats->m_task_map_list->First();
		while (existing_map_elm)
		{
			HTML_Element* elm = existing_map_elm->GetHElm();

			if (existing_map_elm->GetTask() == new_task
				&& elm->IsMatchingType(WE_DO, NS_WML))
			{
				if (elm->SetSpecialBoolAttr(ATTR_WML_SHADOWING_TASK, TRUE, SpecialNs::NS_LOGDOC) == -1)
					return OpStatus::ERR_NO_MEMORY;

				elm->MarkExtraDirty(m_doc_manager->GetCurrentDoc());
				elm->MarkPropsDirty(m_doc_manager->GetCurrentDoc());
			}

			existing_map_elm = existing_map_elm->Suc();
		}

	}

    return OpStatus::OK;
}

OP_STATUS
WML_Context::SetActiveTask(WMLNewTaskElm *new_active_task)
{
    if (new_active_task)
    {
        if (m_current_stats->m_active_vars)
            m_current_stats->m_active_vars->Clear();
        else
        {
            m_current_stats->m_active_vars = OP_NEW(AutoDeleteList<WMLVariableElm>, ());
            if (!m_current_stats->m_active_vars)
                return OpStatus::ERR_NO_MEMORY;
        }

        HTML_Element *last_found = NULL;
        int name_buf_len = UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2Len());
        int value_buf_len = UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen());
        uni_char *name_buf = (uni_char *) g_memory_manager->GetTempBuf2();
        uni_char *value_buf = (uni_char *) g_memory_manager->GetTempBuf2k();
        const uni_char *name_str = NULL;
        const uni_char *value_str = NULL;

        new_active_task->GetFirstIntVar(&last_found, &name_str, &value_str);
        BOOL added = FALSE;
        while (last_found)
        {
			if (name_str)
			{
				SubstituteVars(name_str, uni_strlen(name_str), name_buf, name_buf_len, FALSE, NULL);
				if (value_str)
					SubstituteVars(value_str, uni_strlen(value_str), value_buf, value_buf_len, FALSE, NULL);
				else
					value_buf[0] = 0;

				WMLVariableElm *new_var = OP_NEW(WMLVariableElm, ());

				if (new_var)
				{
					OP_STATUS st = new_var->SetName(name_buf, uni_strlen(name_buf));
					if (OpStatus::IsError(st))
   					{
						OP_DELETE(new_var);
						return st;
					}
					st = new_var->SetVal(value_buf, uni_strlen(value_buf));
					if (OpStatus::IsError(st))
   					{
						OP_DELETE(new_var);
						return st;
					}
					new_var->Into(m_current_stats->m_active_vars);
					added = TRUE;
				}
				else
					return OpStatus::ERR_NO_MEMORY;
			}

            new_active_task->GetNextIntVar(&last_found, &name_str, &value_str);
        }

        if (added)
	        if (FramesDocument* frm_doc = m_doc_manager->GetCurrentDoc())
		        if (HLDocProfile *hld_profile = frm_doc->GetHLDocProfile())
			        if (HTML_Element* root = hld_profile->GetRoot())
				        root->ClearResolvedUrls();
    }
    else if (m_current_stats->m_active_vars)
    {
        OP_DELETE(m_current_stats->m_active_vars);
        m_current_stats->m_active_vars = NULL;
    }

    return OpStatus::OK;
}

URL
WML_Context::GetWmlUrl(HTML_Element* he, UINT32* action, WML_EventType event)
{
    URL ret_url;
	FramesDocument *current_doc = GetDocManager()->GetCurrentDoc();
    NS_Type ns_type = g_ns_manager->GetNsTypeAt(he->GetNsIdx());
    const uni_char *adr = NULL;

	BOOL check_loop = FALSE;
	BOOL is_implicit_go = FALSE;
	UINT32 url_action = 0;
	SetStatusOn(WS_NOSECWARN);

	if (ns_type == NS_HTML)
	{
		switch (he->Type())
		{
		case HE_A:
			adr = he->GetStringAttr(ATTR_HREF);
			break;

		case HE_BODY:
		case HE_HTML:
			{
				switch (event)
				{
				case WEVT_ONTIMER:
					adr = he->GetStringAttr(WA_ONTIMER, NS_IDX_WML);
					is_implicit_go = TRUE;
					break;
				case WEVT_ONENTERFORWARD:
					adr = he->GetStringAttr(WA_ONENTERFORWARD, NS_IDX_WML);
					is_implicit_go = TRUE;
					check_loop = TRUE;
					break;
				case WEVT_ONENTERBACKWARD:
					adr = he->GetStringAttr(WA_ONENTERBACKWARD, NS_IDX_WML);
					is_implicit_go = TRUE;
					break;
				}
			}
			break;

		case HE_OPTION:
			if (event == WEVT_ONPICK)
			{
				adr = he->GetStringAttr(WA_ONPICK, NS_IDX_WML);
				is_implicit_go = TRUE;
			}
			break;
		}
	}
	else if (ns_type == NS_WML)
	{
		BOOL stop = FALSE;
		while (he && !stop)
		{
			if (he->GetNsType() != NS_WML)
				he = he->SucActual();
			else
			{
				switch (he->Type())
				{
				case WE_GO:
					url_action |= WS_GO;
					stop = TRUE;
					break;

				case WE_DO:
				case WE_ANCHOR:
				case WE_ONEVENT:
					he = he->FirstChildActual();
					break;

				case WE_PREV:
					url_action |= WS_ENTERBACK;
					stop = TRUE;
					break;

				case WE_REFRESH:
					url_action |= WS_REFRESH;
					stop = TRUE;
					break;

				case WE_CARD:
				case WE_TEMPLATE:
					{
						switch (event)
						{
						case WEVT_ONTIMER:
							adr = he->GetStringAttr(WA_ONTIMER, NS_IDX_WML);
							is_implicit_go = TRUE;
							stop = TRUE;
							break;
						case WEVT_ONENTERFORWARD:
							adr = he->GetStringAttr(WA_ONENTERFORWARD, NS_IDX_WML);
							stop = TRUE;
							is_implicit_go = TRUE;
							check_loop = TRUE;
							break;
						case WEVT_ONENTERBACKWARD:
							adr = he->GetStringAttr(WA_ONENTERBACKWARD, NS_IDX_WML);
							is_implicit_go = TRUE;
							stop = TRUE;
							break;
						default:
							stop = TRUE;
							break;
						}
					}
					break;

				case WE_NOOP:
					stop = TRUE;
					url_action |= WS_NOOP;
					break;

				default:
					he = he->SucActual();
					break;
				}
			}
		}
	}

	if (url_action & WS_NOOP)
	{
		if (action)
			*action |= WS_NOOP;
	}
	else
	{
		URL doc_url = current_doc->GetURL();
#ifdef WEB_TURBO_MODE
		URL_CONTEXT_ID current_context = doc_url.GetContextId();
		URL_CONTEXT_ID turbo_context = g_windowManager->GetTurboModeContextId();
		URL_CONTEXT_ID use_context = GetDocManager()->GetWindow()->GetTurboMode() ? turbo_context :  0;

		if (use_context != current_context && (current_context == 0 || current_context == turbo_context))
		{
			const OpStringC8 base_str = doc_url.GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, URL::KNoRedirect);
			doc_url = g_url_api->GetURL(base_str.CStr(), use_context);
		}
#endif // WEB_TURBO_MODE

		if (url_action & WS_GO)
		{
			if (action)
				*action |= WS_GO;

			// use the GO task as a FORM
			Form form(doc_url, he, he, -1, -1);
			OP_STATUS oom_status = OpStatus::OK;
			ret_url = form.GetURL(current_doc, oom_status);

			if (oom_status == OpStatus::ERR_NO_MEMORY)
				return URL();

			if (action && he->GetBoolAttr(WA_SENDREFERER, NS_IDX_WML))
				*action |= WS_SENDREF;

			const uni_char *cache_str = he->GetStringAttr(WA_CACHE_CONTROL, NS_IDX_WML);
			if (cache_str)
			{
				char *tmp_buf = (char *) g_memory_manager->GetTempBuf2k();
				make_singlebyte_in_buffer(cache_str, uni_strlen(cache_str), tmp_buf, g_memory_manager->GetTempBuf2kLen());
				ret_url.SetAttribute(URL::KHTTPCacheControl, tmp_buf);
			}
			return ret_url;
		}

		if (url_action & WS_ENTERBACK)
		{
			if (action)
				*action |= WS_ENTERBACK;

			DocListElm *tmp_doc = m_doc_manager->CurrentDocListElm();
			tmp_doc = tmp_doc->Pred();

			if (tmp_doc)
				return tmp_doc->GetUrl();
		}

		if (url_action & WS_REFRESH)
		{
			if (action)
				*action |= WS_REFRESH;

			uni_char *tmp_url_str = (uni_char *) g_memory_manager->GetTempBuf2k();
			const uni_char *rel_name = m_doc_manager->GetCurrentURL().UniRelName();
			if (rel_name)
			{
				UINT32 tmp_len = uni_strlen(rel_name) + 1;

				if (tmp_len > g_memory_manager->GetTempBuf2kLen() - 1)
					tmp_len = g_memory_manager->GetTempBuf2kLen() - 1;

				tmp_url_str[0] = '#';
				uni_strncpy(tmp_url_str+1, rel_name, tmp_len-1);
				tmp_url_str[tmp_len] = 0;

				adr = tmp_url_str;
			}
			else
				adr = UNI_L("");
		}

		if (adr)
		{
			if (*adr)
			{
				uni_char *subst_adr = (uni_char *) g_memory_manager->GetTempBuf();
				SubstituteVars(adr, uni_strlen(adr),
					subst_adr, UNICODE_DOWNSIZE(g_memory_manager->GetTempBufLen()), TRUE, NULL);
				adr = subst_adr;
			}

			ret_url = g_url_api->GetURL(doc_url, adr);

			if (is_implicit_go && action)
				*action |= WS_GO;

			// If we try to go the same card as we are currently at
			// we should stop an infinite loop by doing a noop
			if (check_loop && ret_url == current_doc->GetURL())
			{
				const char *ret_rel = ret_url.RelName();
				const char *doc_rel = current_doc->GetURL().RelName();
				if (ret_rel && doc_rel
					&& op_strcmp(ret_rel, doc_rel) == 0)
				{
					ret_url = URL();
					if (action)
						*action |= WS_NOOP;
				}
			}
		}
	}

    return ret_url;
}

OP_STATUS
WML_Context::UpdateWmlInput(HTML_Element* he, FormObject* form_obj)
{
	if (he->Type() != HE_INPUT)
		return OpStatus::OK;

	InputType input_type = he->GetInputType();
	if (input_type != INPUT_TEXTAREA &&
		input_type != INPUT_PASSWORD &&
		input_type != INPUT_TEXT &&
		input_type != INPUT_URI &&
		input_type != INPUT_DATE && input_type != INPUT_WEEK &&
		input_type != INPUT_TIME && input_type != INPUT_EMAIL &&
		input_type != INPUT_NUMBER && input_type != INPUT_RANGE &&
		input_type != INPUT_MONTH &&
		input_type != INPUT_DATETIME && input_type != INPUT_DATETIME_LOCAL &&
		input_type != INPUT_COLOR && input_type != INPUT_TEL && input_type != INPUT_SEARCH)
		return OpStatus::OK;

	const uni_char* name_str = he->GetWmlName();
	if (name_str)
	{
		FormValue* form_value = he->GetFormValue();
		OpString value;

		RETURN_IF_MEMORY_ERROR(form_value->GetValueAsText(he, value));

		if (!value.IsEmpty())
		{
			const uni_char *val = value.CStr();
			size_t val_len = value.Length();

			// if value becomes substituted -- set new value in form_object
			if (WML_Context::NeedSubstitution(val, val_len))
			{
				uni_char* subst_val = (uni_char*)g_memory_manager->GetTempBuf2();
				int sub_len = SubstituteVars(val, val_len, subst_val,
								g_memory_manager->GetTempBuf2Len(), FALSE, NULL);

				if (sub_len > 0)
				{
					RETURN_IF_MEMORY_ERROR(form_value->SetValueFromText(he, subst_val));
					if (NULL == SetVariable(name_str, subst_val))
						return OpStatus::ERR_NO_MEMORY;
				}
			}
			else if (val)
			{
				if (NULL == SetVariable(name_str, val))
					return OpStatus::ERR_NO_MEMORY;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS
WML_Context::UpdateWmlSelection(HTML_Element* he, BOOL use_value)
{
	if (he->Type() != HE_SELECT)
		return OpStatus::OK;

	// get name/value and iname/ivalue
	const uni_char* name_str = he->GetWmlName();
	const uni_char* val_str = he->GetWmlValue();
	const uni_char* iname_str = he->GetAttrValue(WA_INAME, NS_IDX_WML);
	const uni_char* ival_str = he->GetAttrValue(WA_IVALUE, NS_IDX_WML);

	BOOL set_iname = FALSE;
	BOOL set_name = FALSE;

	if (use_value && iname_str && ival_str)
	{
		if (SetVariable(iname_str, ival_str) == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}
	else if (iname_str)
		set_iname = TRUE;

	if (use_value && name_str && val_str)
	{
		if (SetVariable(name_str, val_str) == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}
	else if (name_str)
		set_name = TRUE;

	if (set_iname || set_name)
	{
		FormValueList* list_value = FormValueList::GetAs(he->GetFormValue());

		uni_char* name_buf = (uni_char*)g_memory_manager->GetTempBuf();
		uni_char* iname_buf = (uni_char*)g_memory_manager->GetTempBuf2();
		*name_buf = '\0';
		*iname_buf = '\0';
		uni_char* name_tmp = name_buf;
		uni_char* iname_tmp = iname_buf;

		if (set_iname)
		{
			// Build a list of selected indexes as a text to be
			// compatible with the old form value code
			OpINT32Vector selected_indexes;
			RETURN_IF_ERROR(list_value->GetSelectedIndexes(he, selected_indexes));
			UINT32 sel_count = selected_indexes.GetCount();
			if (sel_count > 0)
			{
				for (unsigned int sel_i = 0; sel_i < sel_count; sel_i++)
				{
					if (iname_tmp != iname_buf)
					{
						*iname_tmp = ';';
						iname_tmp++;
					}
					uni_ltoa(selected_indexes.Get(sel_i) + 1, iname_tmp, 10);
					iname_tmp += uni_strlen(iname_tmp);
				}
			}
		}

		if (set_name)
		{
			OpAutoVector<OpString> selected_values;
			RETURN_IF_ERROR(list_value->GetSelectedValues(he, selected_values));
			UINT32 value_count = selected_values.GetCount();
			for (UINT32 i = 0; i < value_count; i++)
			{
				if (!selected_values.Get(i)->IsEmpty())
				{
					if (name_tmp != name_buf)
					{
						*name_tmp = ';';
						name_tmp++;
					}
					name_tmp = uni_strcpy(name_tmp, selected_values.Get(i)->CStr());
					name_tmp += uni_strlen(name_tmp);
				}
			}
		}

		if (set_iname)
		{
			if (SetVariable(iname_str, iname_buf) == NULL)
				return OpStatus::ERR_NO_MEMORY;
		}

		// The option value is allowed to contain variable references, we must substitute here.
		if (set_name)
		{
			uni_char* subst_name = (uni_char*)g_memory_manager->GetTempBuf2();
			SubstituteVars(name_buf, uni_strlen(name_buf), subst_name, g_memory_manager->GetTempBuf2Len(), FALSE, NULL);
			if (SetVariable(name_str, subst_name) == NULL)
				return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

static BOOL
SetSelectedByIndex(const uni_char *value, HTML_Element *select_elm, FormValueList *val_list)
{
	BOOL is_set = FALSE;
	const uni_char *next = value;
	unsigned int option_count = val_list->GetOptionCount(select_elm);

	while (next)
	{
		int candidate_idx = uni_atoi(next);
		if (candidate_idx > 0 && ((unsigned int)candidate_idx) <= option_count)
		{
			val_list->SelectValue(select_elm, candidate_idx - 1, TRUE);
			is_set = TRUE;
		}

		next = uni_strchr(next, ';');
		if (next)
			next++; // skip the ;
	}

	return is_set;
}

static BOOL
SetSelectedByValue(const uni_char *value, HTML_Element *select_elm, FormValueList* val_list)
{
	BOOL is_set = FALSE;
	OpAutoVector<OpString> val_vector;
	if (OpStatus::IsMemoryError(val_list->GetAllValues(select_elm, val_vector)))
		return FALSE;

	UINT vector_len = val_vector.GetCount();
	for (UINT idx = 0; idx < vector_len; idx++)
	{
		const uni_char *current_val = value;
		while (current_val)
		{
			int len;
			const uni_char *next = uni_strchr(current_val, ';');
			if (next)
			{
				len = next - current_val;
				next++;
			}
			else
				len = uni_strlen(current_val);

			OpString *val_string = val_vector.Get(idx);
			if (val_string->Length() == len && val_string->Compare(current_val, len) == 0)
			{
				val_list->SelectValue(select_elm, idx, TRUE);
				is_set = TRUE;
				break;
			}

			current_val = next;
		}
	}

	return is_set;
}

void
WML_Context::SetInitialSelectValue(HTML_Element *select_elm)
{
	FormValueList* val_list = FormValueList::GetAs(select_elm->GetFormValue());

	const uni_char *iname_str = select_elm->GetAttrValue(WA_INAME, NS_IDX_WML);
	if (iname_str)
	{
		const uni_char *var_value = GetVariable(iname_str);
		if (var_value)
		{
			if (SetSelectedByIndex(var_value, select_elm, val_list))
				return;
		}
	}

	const uni_char *ival_str = select_elm->GetAttrValue(WA_IVALUE, NS_IDX_WML);
	if (ival_str)
	{
		if (SetSelectedByIndex(ival_str, select_elm, val_list))
			return;
	}

	const uni_char *name_str = select_elm->GetWmlName();
	if (name_str)
	{
		const uni_char *var_value = GetVariable(name_str);
		if (var_value)
		{
			if (SetSelectedByValue(var_value, select_elm, val_list))
				return;
		}
	}

	const uni_char* val_str = select_elm->GetWmlValue();
	if (val_str)
	{
		if (SetSelectedByValue(val_str, select_elm, val_list))
			return;
	}
}

void
WML_Context::DecRef()
{
	if (m_refs > 0)
		m_refs--;
	else
	{
		OP_ASSERT(!"Someone has dereferenced this context once too many.");
	}

	if (!m_refs)
		OP_DELETE(this);
}

#endif // _WML_SUPPORT_
