/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jens Lindstrom / Lars T Hansen / Johannes Hoff / Morten Rolland / Jan Borsodi
**
** Relevant documents:
**   Protocol: ../documentation/ecmascript-debugger-protocol.html
*/

#include "core/pch.h"

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

#include "modules/scope/src/scope_ecmascript_debugger.h"
#include "modules/xmlutils/xmlnames.h"

#include "modules/util/str.h"
#include "modules/util/opstring.h"
#include "modules/util/OpHashTable.h"
#include "modules/url/uamanager/ua.h"
#include "modules/url/url_enum.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/dom/domevents.h"

#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domobj.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/winman.h"

#include "modules/svg/SVGManager.h"

#include "modules/style/css.h"
#include "modules/style/css_decl.h"
#include "modules/style/css_property_list.h"
#include "modules/style/src/css_rule.h"
#include "modules/style/src/css_ruleset.h"
#include "modules/style/src/css_media_rule.h"
#include "modules/style/src/css_import_rule.h"
#include "modules/style/src/css_charset_rule.h"
#include "modules/style/src/css_fontface_rule.h"
#include "modules/style/src/css_selector.h"
#include "modules/style/src/css_aliases.h"

#include "modules/scope/src/scope_manager.h"
#include "modules/protobuf/src/json_tools.h"
#include "modules/scope/src/scope_window_manager.h"
#include "modules/scope/src/scope_utils.h"

#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esobjman.h"

#ifdef DISPLAY_SPOTLIGHT
#include "modules/display/vis_dev.h"
#endif

#include "modules/stdlib/include/double_format.h"

#if defined(DOC_SPOTLIGHT_API)
#define _USE_SPOTLIGHT
#endif

#define MAX_OBJECT_IDS  32

/* Enable __GET_STYLE_RULES to get css inspector, aka get-matching-css-rules.
   Note: You need to import API_STYLE_GETMATCHINGRULES as well. Remember to unimport it
         if you turn the define off again.
   Todo: Document the protocol
   Todo: Move it to a more logical place than esdebugger?
   Todo: Make it a feature?
   Todo: Test it properly
*/
#define __GET_STYLE_RULES

/* Enable __DOM_INSPECTOR to get a quicker DOM inspector than the javascript implementation.
   Relies on DOM's API_DOM_INSPECT_NODE.
*/
#ifdef DOM_INSPECT_NODE_SUPPORT
#define __DOM_INSPECTOR
#endif


class ES_DebugEventHandler
	: public DOM_EventsAPI::EventHandler,
	  public Link
{
	DOM_EventType event_type;
	uni_char *custom_event_name;       // != 0 iff event_type == DOM_EVENT_CUSTOM
	ES_ScopeDebugFrontend *debug_frontend;
	DOM_EventsAPI::EventTarget* event_target;
	BOOL prevent_default, stop_propagation;
	ES_EventPhase phase;
	unsigned id;
public:
	ES_DebugEventHandler()
		: event_type(DOM_EVENT_NONE)
		, custom_event_name(0)
		, event_target(0)
	{
	}

	~ES_DebugEventHandler()
	{
		if (event_target)
			event_target->RemoveEventHandler(this);
		Out();
		op_free(custom_event_name);
	}

	OP_STATUS Init(const ES_DebugEventHandlerData &data, DOM_EventsAPI::EventTarget* _event_target, ES_ScopeDebugFrontend *_debug_frontend)
	{
		id = data.handler_id;
		event_target = _event_target;
		debug_frontend = _debug_frontend;
		event_type = DOM_EventsAPI::GetEventType(data.event_type);
		if (event_type == DOM_EVENT_NONE)
		{
			event_type = DOM_EVENT_CUSTOM;
			custom_event_name = uni_strdup(data.event_type);
		}

		prevent_default = data.prevent_default;
		stop_propagation = data.stop_propagation;
		phase = data.phase;

		RETURN_IF_ERROR(event_target->AddEventHandler(this));
		return OpStatus::OK;
	}

	virtual BOOL HandlesEvent(DOM_EventType known_type, const uni_char *type, ES_EventPhase phase)
	{
		if (!(known_type == event_type && (phase == ES_PHASE_ANY || phase == ES_PHASE_AT_TARGET || phase == this->phase)))
			return false;

		if (known_type == DOM_EVENT_CUSTOM && uni_strcmp(type, custom_event_name) != 0)
			return false;

		return true;
	}

	virtual OP_STATUS HandleEvent(DOM_EventsAPI::Event *event)
	{
		const uni_char* event_name;
		OpString event_name16;
		if (event->GetType() == DOM_EVENT_CUSTOM)
			event_name = event->GetTypeString();
		else
		{
			const char* event_name8 = DOM_EventsAPI::GetEventTypeString(event->GetType());
			RETURN_IF_ERROR(event_name16.Set(event_name8));
			event_name = event_name16.CStr();
		}
		DOM_Object *dom_obj = event->GetTarget();
		ES_Runtime *runtime = DOM_Utils::GetES_Runtime(DOM_Utils::GetDOM_Runtime(dom_obj));
		RETURN_IF_ERROR(debug_frontend->HandleEvent(event_name, runtime, DOM_Utils::GetES_Object(dom_obj), id));

		if (prevent_default)
			event->PreventDefault();
		if (stop_propagation)
			event->StopImmediatePropagation();

		return OpStatus::OK;
	}

	virtual void RegisterHandlers(DOM_Environment *environment)
	{
		environment->AddEventHandler(event_type);
	}

	virtual void UnregisterHandlers(DOM_Environment *environment)
	{
		environment->RemoveEventHandler(event_type);
	}

	unsigned GetId() { return id; }
};

static ES_ScopeDebugFrontend::ConsoleLogType
ConvertConsoleLogType(OpScopeJSConsoleListener::LogType type)
{
	switch(type)
	{
	default:
		OP_ASSERT(!"Please add a new case!");
	case OpScopeJSConsoleListener::LOG_LOG:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_LOG;
	case OpScopeJSConsoleListener::LOG_DEBUG:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_DEBUG;
	case OpScopeJSConsoleListener::LOG_INFO:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_INFO;
	case OpScopeJSConsoleListener::LOG_WARN:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_WARN;
	case OpScopeJSConsoleListener::LOG_ERROR:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_ERROR;
	case OpScopeJSConsoleListener::LOG_ASSERT:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_ASSERT;
	case OpScopeJSConsoleListener::LOG_DIR:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_DIR;
	case OpScopeJSConsoleListener::LOG_DIRXML:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_DIRXML;
	case OpScopeJSConsoleListener::LOG_GROUP:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_GROUP;
	case OpScopeJSConsoleListener::LOG_GROUP_COLLAPSED:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_GROUP_COLLAPSED;
	case OpScopeJSConsoleListener::LOG_GROUP_END:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_GROUP_END;
	case OpScopeJSConsoleListener::LOG_COUNT:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_COUNT;
	case OpScopeJSConsoleListener::LOG_TABLE:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_TABLE;
	case OpScopeJSConsoleListener::LOG_CLEAR:
		return ES_ScopeDebugFrontend::CONSOLELOGTYPE_CLEAR;
	};
}

OP_STATUS
ES_ScopeDebugFrontend::AddEventHandler(const ES_DebugEventHandlerData &data)
{
	ES_Object *object;

	if (!(object = GetObjectById(data.object_id)))
		return OpStatus::ERR;

	DOM_Object *node = DOM_Utils::GetDOM_Object(object);

	if (!node)
		return OpStatus::ERR;

	DOM_EventsAPI::EventTarget* event_target;
	RETURN_IF_ERROR(DOM_EventsAPI::GetEventTarget(event_target, node));
	OpAutoPtr<ES_DebugEventHandler> handler(OP_NEW(ES_DebugEventHandler, ()));
	RETURN_OOM_IF_NULL(handler.get());
	RETURN_IF_ERROR(handler->Init(data, event_target, this));
	handler.release()->Into(&dbg_eventhandlers);

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::RemoveEventHandler(unsigned handler_id)
{
	for (ES_DebugEventHandler *dbg_eventhandler = (ES_DebugEventHandler *) dbg_eventhandlers.First();
	     dbg_eventhandler;
	     dbg_eventhandler = (ES_DebugEventHandler *) dbg_eventhandler->Suc())
	{
		if (dbg_eventhandler->GetId() == handler_id)
		{
			dbg_eventhandler->Out();
			OP_DELETE(dbg_eventhandler);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

#ifdef __GET_STYLE_RULES
#include "modules/style/css_dom.h"

static PseudoElementType
ConvertPseudoElement(ES_ScopeDebugFrontend::PseudoSelector sel)
{
	switch (sel)
	{
	case ES_ScopeDebugFrontend::PSEUDOSELECTOR_FIRST_LINE:
		return ELM_PSEUDO_FIRST_LINE;
	case ES_ScopeDebugFrontend::PSEUDOSELECTOR_FIRST_LETTER:
		return ELM_PSEUDO_FIRST_LETTER;
	case ES_ScopeDebugFrontend::PSEUDOSELECTOR_BEFORE:
		return ELM_PSEUDO_BEFORE;
	case ES_ScopeDebugFrontend::PSEUDOSELECTOR_AFTER:
		return ELM_PSEUDO_AFTER;
	case ES_ScopeDebugFrontend::PSEUDOSELECTOR_SELECTION:
		return ELM_PSEUDO_SELECTION;
	default:
		return ELM_NOT_PSEUDO;
	}
}

static ES_ScopeDebugFrontend::PseudoSelector
ConvertPseudoElement(PseudoElementType type)
{
	switch (type)
	{
	case ELM_PSEUDO_FIRST_LINE:
		return ES_ScopeDebugFrontend::PSEUDOSELECTOR_FIRST_LINE;
	case ELM_PSEUDO_FIRST_LETTER:
		return ES_ScopeDebugFrontend::PSEUDOSELECTOR_FIRST_LETTER;
	case ELM_PSEUDO_BEFORE:
		return ES_ScopeDebugFrontend::PSEUDOSELECTOR_BEFORE;
	case ELM_PSEUDO_AFTER:
		return ES_ScopeDebugFrontend::PSEUDOSELECTOR_AFTER;
	case ELM_PSEUDO_SELECTION:
		return ES_ScopeDebugFrontend::PSEUDOSELECTOR_SELECTION;
	default:
		return ES_ScopeDebugFrontend::PSEUDOSELECTOR_NONE;
	}
}

/* OpScopeNodeMatch::Property */

OpScopeNodeMatch::Property::Property(int index, BOOL important, CSS_decl *decl)
	: index(index)
	, important(important)
	, status(ES_ScopeDebugFrontend::DECLARATIONSTATUS_USED)
	, declaration(decl)
{
}

/*virtual*/
OpScopeNodeMatch::Property::~Property()
{
	Out();
}

OP_STATUS
OpScopeNodeMatch::Property::Construct(const OpString &val)
{
	return value.Set(val);
}

OP_STATUS
OpScopeNodeMatch::Property::Construct(const uni_char *val)
{
	return value.Set(val);
}

/* OpScopeNodeMatch::Rule */

OpScopeNodeMatch::Rule::Rule(ES_ScopeDebugFrontend::RuleOrigin origin, int flow_order)
	: origin(origin)
	, flow_order(flow_order)
	, stylesheet_id(0)
	, rule_id(0)
	, rule_type(0)
#ifdef SCOPE_CSS_RULE_ORIGIN
	, line_number(0)
#endif // SCOPE_CSS_RULE_ORIGIN
	, selector_specificity(0)
{
}

/*virtual*/
OpScopeNodeMatch::Rule::~Rule()
{
	Out();
	properties.Clear();
}

OP_STATUS
OpScopeNodeMatch::Rule::Process(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle::StyleDeclaration &decl) const
{
	decl.SetOrigin(Origin());

	if (Origin() == ES_ScopeDebugFrontend::RULEORIGIN_AUTHOR)
	{
		decl.SetStylesheetID(StylesheetID());
		decl.SetRuleID(RuleID());
		decl.SetRuleType(RuleType());
		decl.SetSpecificity(SelectorSpecificity());
		RETURN_IF_ERROR(decl.SetSelector(SelectorText()));
#ifdef SCOPE_CSS_RULE_ORIGIN
		if (line_number != 0)
			decl.SetLineNumber(line_number);
#endif // SCOPE_CSS_RULE_ORIGIN
	}
	else if (Origin() == ES_ScopeDebugFrontend::RULEORIGIN_LOCAL)
	{
		decl.SetSpecificity(SelectorSpecificity());
		RETURN_IF_ERROR(decl.SetSelector(SelectorText()));
	}
	/* Specificity is zero for the style 'generated' by SVG presentational
	   attributes, according to spec. */
	else if (Origin() == ES_ScopeDebugFrontend::RULEORIGIN_SVG)
		decl.SetSpecificity(0);

	return ProcessProperties(decl);
}

OP_STATUS
OpScopeNodeMatch::Rule::ProcessProperties(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle::StyleDeclaration &decl) const
{
	OpScopeNodeMatch::Property *prop_obj;

	for (prop_obj = FirstProperty();
		 prop_obj != NULL;
		 prop_obj = prop_obj->Suc())
	{
		RETURN_IF_ERROR(decl.GetIndexListRef().Add(prop_obj->Index()));
		OpAutoPtr<OpString> value_str(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(value_str.get());
		RETURN_IF_ERROR(value_str->Set(prop_obj->Value()));
		RETURN_IF_ERROR(decl.GetValueListRef().Add(value_str.release()));
		RETURN_IF_ERROR(decl.GetPriorityListRef().Add(prop_obj->IsImportant() ? 1 : 0));
		RETURN_IF_ERROR(decl.GetStatusListRef().Add(prop_obj->Status()));
#ifdef DOM_FULLSCREEN_MODE
		RETURN_IF_ERROR(decl.GetFullscreenListRef().Add(prop_obj->IsFullscreen() ? 1 : 0));
#endif // DOM_FULLSCREEN_MODE
	}

	return OpStatus::OK;
}

/*static*/
int
OpScopeNodeMatch::Rule::Compare(const Rule &a, const Rule &b)
{
	if (a.Origin() == b.Origin())
	{
		int cmp = b.SelectorSpecificity() - a.SelectorSpecificity(); // The higher the specificity the more important it is
		if (cmp == 0)
			return a.FlowOrder() - b.FlowOrder(); // The earlier it is in the flow (lower value) the higher priority it has

		return cmp;
	}
	else
		return a.FlowOrder() - b.FlowOrder(); // The earlier it is in the flow (lower value) the higher priority it has
}

/* OpScopeNodeMatch */
OpScopeNodeMatch::OpScopeNodeMatch(int object_id, const CSS_MatchElement &elm, const uni_char *el_name)
	: object_id(object_id)
	, element(elm)
	, element_name(el_name)
	, sorted_declarations(NULL)
	, needs_sorting(TRUE)
{
}

/*virtual*/
OpScopeNodeMatch::~OpScopeNodeMatch()
{
	declarations.Clear();
	OP_DELETEA(sorted_declarations);
}

OP_STATUS
OpScopeNodeMatch::Process(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle &style) const
{
	style.SetObjectID(ObjectID());
	RETURN_IF_ERROR(style.SetElementName(ElementName()));

	PseudoElementType pseudo = element.GetPseudoElementType();

	if (pseudo != ELM_NOT_PSEUDO)
		style.SetPseudoElement(ConvertPseudoElement(pseudo));

	RETURN_IF_ERROR(SortRules());
	int count;
	const OpScopeNodeMatch::Rule * const *rules = SortedRules(count);
	OP_ASSERT(rules != NULL);
	for (int i = 0;
		 i < count;
		 ++i)
	{
		OpAutoPtr<ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle::StyleDeclaration> decl(OP_NEW(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle::StyleDeclaration, ()));
		RETURN_OOM_IF_NULL(decl.get());
		RETURN_IF_ERROR(rules[i]->Process(*decl.get()));
		RETURN_IF_ERROR(style.GetStyleListRef().Add(decl.release()));
	}
	return OpStatus::OK;
}

const OpScopeNodeMatch::Rule * const *
OpScopeNodeMatch::SortedRules(int &count) const
{
	count = declarations.Cardinal();
	if (needs_sorting)
	{
		OP_STATUS status = SortRules();
		if (OpStatus::IsError(status))
			return NULL;
	}
	return sorted_declarations;
}

int OpScopeCompareRuleHelper(const void *a, const void *b)
{
	return OpScopeNodeMatch::Rule::Compare(**((const OpScopeNodeMatch::Rule **)a), **((const OpScopeNodeMatch::Rule **)b));
}

OP_STATUS
OpScopeNodeMatch::SortRules() const
{
	if (!needs_sorting)
		return OpStatus::OK;

	int count = declarations.Cardinal();
	OP_DELETEA(sorted_declarations);
	sorted_declarations = OP_NEWA(Rule *, count);
	RETURN_OOM_IF_NULL(sorted_declarations);
	Rule **it = sorted_declarations;
	for (Rule *rule = FirstRule(); rule != NULL; rule = rule->Suc())
	{
		*it++ = rule;
	}
	op_qsort(sorted_declarations, count, sizeof(Rule *), OpScopeCompareRuleHelper);
	needs_sorting = FALSE;
	return OpStatus::OK;
}

/* StyleListener */

ES_Object*
ES_ScopeGetStylesheetESObject(DOM_Object* dom_obj, DOM_Object* import_rule)
{
	DOM_Object *stylesheet = NULL;
	RETURN_VALUE_IF_ERROR(DOM_Utils::GetDOM_CSSStyleSheet(dom_obj, import_rule, stylesheet), NULL);
	return DOM_Utils::GetES_Object(stylesheet);
}

class ScopeStyleListener : public OpScopeStyleListener
{
public:
	ScopeStyleListener(const CSS_MatchElement& elm, DOM_Environment *env, int pseudo_selectors, ES_DebugFrontend *frontend);

	virtual int GetObjectID(HTML_Element* elm);
	virtual int GetObjectID(DOM_Object* dom_obj);
	virtual unsigned int GetStylesheetID(DOM_Object* dom_obj);

private:
	ES_DebugFrontend* frontend;
};

ScopeStyleListener::ScopeStyleListener(const CSS_MatchElement& elm, DOM_Environment *env, int pseudo_selectors, ES_DebugFrontend *frontend)
	: OpScopeStyleListener(elm, env, pseudo_selectors)
	, frontend(frontend)
{
	OP_ASSERT(frontend != NULL);
}

/*virtual*/
int
ScopeStyleListener::GetObjectID(HTML_Element* elm)
{
	DOM_Object *dom_obj;
	if (OpStatus::IsSuccess(Environment()->ConstructNode(dom_obj, elm)))
	{
		return GetObjectID(dom_obj);
	}
	return 0;
}

/*virtual*/
int
ScopeStyleListener::GetObjectID(DOM_Object* dom_obj)
{
	return frontend->GetObjectId(DOM_Utils::GetES_Runtime(DOM_Utils::GetDOM_Runtime(dom_obj)), DOM_Utils::GetES_Object(dom_obj));
}

/*virtual*/
unsigned int
ScopeStyleListener::GetStylesheetID(DOM_Object* dom_obj)
{
	ES_Object *css_es = ES_ScopeGetStylesheetESObject(dom_obj, NULL);
	if (!css_es)
		return 0;
	return frontend->GetObjectId(DOM_Utils::GetES_Runtime(DOM_Utils::GetDOM_Runtime(dom_obj)), css_es);
}

/* OpScopeStyleListener */

OpScopeStyleListener::OpScopeStyleListener(const CSS_MatchElement& elm, DOM_Environment* env, int pseudo_selectors)
	: inspect_elm(elm)
	, flow_order(0)
	, env(env)
	, pseudo_selectors(pseudo_selectors)
	, current_node(NULL)
{
}

OpScopeStyleListener::~OpScopeStyleListener()
{
	node_matches.Clear();
}

int OpScopeStyleListener::NextFlowOrder()
{
	return flow_order++;
}

OP_STATUS OpScopeStyleListener::NewRule(OpScopeNodeMatch::Rule*& match_rule, const CSS_MatchElement& elm, CSS_decl* _css_decl, ES_ScopeDebugFrontend::RuleOrigin origin)
{
	if (!elm.IsValid())
	{
		OP_ASSERT(!"Style module reported an invalid element.");
		return OpStatus::OK;
	}

	OpAutoPtr<OpScopeNodeMatch::Rule> mrule(OP_NEW(OpScopeNodeMatch::Rule, (origin, NextFlowOrder())));
	RETURN_OOM_IF_NULL(mrule.get());

	BOOL is_inspected_element = (inspect_elm.IsEqual(elm));
	TempBuffer value;
	for (CSS_decl* OP_MEMORY_VAR css_decl = _css_decl; css_decl != NULL; css_decl = css_decl->Suc())
	{
		css_decl = CSS_MatchRuleListener::ResolveDeclaration(css_decl);

		OP_MEMORY_VAR int prop = css_decl->GetProperty();

		// If the current element is not the inspected one we should only include inherited properties
		if (!is_inspected_element && !PropertyIsInherited(prop))
			continue;

		if (prop == CSS_PROPERTY_font_color)
			prop = CSS_PROPERTY_color;
		value.Clear();
		TRAP_AND_RETURN(ret, CSS::FormatDeclarationL(&value, css_decl, FALSE, CSS_FORMAT_CSSOM_STYLE));
		// FIXME: The code above belongs to style module, e.g. CSS_decl::GetPropertyAndValue

		OpAutoPtr<OpScopeNodeMatch::Property> prop_obj(OP_NEW(OpScopeNodeMatch::Property, (prop, css_decl->GetImportant(), css_decl))); // The correct PropertyStatus value is calculated after listener is finished
		RETURN_OOM_IF_NULL(prop_obj.get());
		RETURN_IF_ERROR(prop_obj->SetValue(value.GetStorage()));
		mrule->AddProperty(prop_obj.release());
	}

	if (!mrule->HasProperties() && origin == ES_ScopeDebugFrontend::RULEORIGIN_USER_AGENT)
	{
		match_rule = NULL;
		return OpStatus::OK;
	}

	if (current_node == NULL || !current_node->GetElement().IsEqual(elm))
	{
		HTML_Element *html_element = elm.GetHtmlElement();

		OpScopeNodeMatch *node = OP_NEW(OpScopeNodeMatch, (GetObjectID(html_element), elm, html_element->GetTagName()));
		RETURN_OOM_IF_NULL(node);
		current_node = node;
		current_node->Into(&node_matches);
	}

	match_rule = mrule.release();
	current_node->AddRule(match_rule);

	return OpStatus::OK;
}

OP_STATUS OpScopeStyleListener::LocalRuleMatched(const CSS_MatchElement& elm, CSS_DOMRule* rule, unsigned short specificity)
{
	OpScopeNodeMatch::Rule *match_rule = NULL;
	RETURN_IF_ERROR(NewRule(match_rule, elm, rule->GetPropertyList()->GetFirstDecl(), ES_ScopeDebugFrontend::RULEORIGIN_LOCAL));
	if (match_rule == NULL)
		return OpStatus::OK;
	TempBuffer selector_text;
	RETURN_IF_ERROR(rule->GetSelectorText(&selector_text));
	match_rule->SetSelector(specificity, selector_text.GetStorage());
	return OpStatus::OK;
}

OP_STATUS OpScopeStyleListener::RuleMatched(const CSS_MatchElement& elm, CSS_DOMRule* rule, unsigned short specificity)
{
	OP_ASSERT(env != NULL);
	OP_ASSERT(elm.IsValid());
	OpScopeNodeMatch::Rule *match_rule = NULL;
	RETURN_IF_ERROR(NewRule(match_rule, elm, rule->GetPropertyList()->GetFirstDecl(), ES_ScopeDebugFrontend::RULEORIGIN_AUTHOR));
	if (match_rule == NULL)
		return OpStatus::OK;
	DOM_Object *dom_obj;
	RETURN_IF_ERROR(env->ConstructCSSRule(dom_obj, rule));
	unsigned rule_id = GetObjectID(dom_obj);
	RETURN_IF_ERROR(env->ConstructNode(dom_obj, rule->GetStyleSheetElm()));
	unsigned int stylesheet_id = GetStylesheetID(dom_obj);
	TempBuffer selector_text;
	RETURN_IF_ERROR(rule->GetSelectorText(&selector_text));

	match_rule->SetStylesheetID(stylesheet_id);
	match_rule->SetRuleID(rule_id);
	match_rule->SetRuleType(rule->GetType());
	match_rule->SetSelector(specificity, selector_text.GetStorage());
#ifdef SCOPE_CSS_RULE_ORIGIN
	match_rule->SetLineNumber(rule->GetLineNumber());
#endif // SCOPE_CSS_RULE_ORIGIN

	return OpStatus::OK;
}

OP_STATUS OpScopeStyleListener::InlineStyle(const CSS_MatchElement& elm, CSS_property_list* style)
{
	OpScopeNodeMatch::Rule *match_rule = NULL;
	RETURN_IF_ERROR(NewRule(match_rule, elm, style->GetFirstDecl(), ES_ScopeDebugFrontend::RULEORIGIN_ELEMENT));
	if (match_rule == NULL)
		return OpStatus::OK;
	return OpStatus::OK;
}

OP_STATUS OpScopeStyleListener::DefaultStyle(const CSS_MatchElement& elm, CSS_property_list* style)
{
	OpScopeNodeMatch::Rule *match_rule = NULL;
	RETURN_IF_ERROR(NewRule(match_rule, elm, style->GetFirstDecl(), ES_ScopeDebugFrontend::RULEORIGIN_USER_AGENT));
	if (match_rule == NULL)
		return OpStatus::OK;
	return OpStatus::OK;
}

BOOL OpScopeStyleListener::MatchPseudo(int pseudo)
{
	return (pseudo & pseudo_selectors) != 0;
}

#ifdef SVG_SUPPORT
OP_STATUS OpScopeStyleListener::SVGRule(HTML_Element* element, CSS_property_list* style)
{
	OpScopeNodeMatch::Rule *match_rule = NULL;
	CSS_MatchElement elm(element);
	return NewRule(match_rule, elm, style->GetFirstDecl(), ES_ScopeDebugFrontend::RULEORIGIN_SVG);
}
#endif // SVG_SUPPORT

/**
 * Lookup table for HTML_Element objects based on the property index.
 * This is used to determine if shared properties (ie. same pointer value)
 * are actually in use or not.
 */
class OpScopeElementProperties
{
private:
	CSS_MatchElement elements[CSS_PROPERTIES_LENGTH];

public:
	OpScopeElementProperties() { Reset(); }
	void Reset() { for (int i = 0; i < CSS_PROPERTIES_LENGTH; i++) { elements[i] = CSS_MatchElement(); } }
	BOOL IsEmpty(int property) const { return elements[property].GetHtmlElement() == NULL; }
	const CSS_MatchElement &GetElement(int property) const { return elements[property]; }
	void SetElement(int property, const CSS_MatchElement &el) { elements[property] = el; }
};

OP_STATUS OpScopeStyleListener::AdjustDeclarations(CSS_Properties &props)
{
	OpScopeElementProperties el_props;
	for (OpScopeNodeMatch *match = FirstMatch();
		 match != NULL;
		 match = match->Suc())
	{
		for (OpScopeNodeMatch::Rule *rule = match->FirstRule();
			 rule != NULL;
			 rule = rule->Suc())
		{
			for (OpScopeNodeMatch::Property *prop_obj = rule->FirstProperty();
				 prop_obj != NULL;
				 prop_obj = prop_obj->Suc())
			{
				int prop = prop_obj->Index();
				CSS_decl *decl = props.GetDecl(prop);
				OP_ASSERT(decl != NULL);

				// If a shared property is the same as the one found in the CSS_Properties
				// object then it will check the HTML_Element in the lookup table,
				// if that differs it means it is not in use.
				ES_ScopeDebugFrontend::DeclarationStatus status = ES_ScopeDebugFrontend::DECLARATIONSTATUS_OVERWRITTEN;
				if (decl == prop_obj->CSSDeclaration())
					if (el_props.IsEmpty(decl->GetProperty()) || el_props.GetElement(decl->GetProperty()).IsEqual(match->GetElement()))
						status = ES_ScopeDebugFrontend::DECLARATIONSTATUS_USED;

				if (decl == prop_obj->CSSDeclaration() && el_props.IsEmpty(decl->GetProperty()) && status == ES_ScopeDebugFrontend::DECLARATIONSTATUS_USED)
					el_props.SetElement(decl->GetProperty(), match->GetElement());
				prop_obj->SetDeclarationStatus(status);
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpScopeStyleListener::ProcessNodes(OpProtobufMessageVector<ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle> &styles) const
{
	for (OpScopeNodeMatch *match = FirstMatch();
		 match != NULL;
		 match = match->Suc())
	{
		OpAutoPtr<ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle> style(OP_NEW(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle, ()));
		RETURN_OOM_IF_NULL(style.get());
		RETURN_IF_ERROR(match->Process(*style.get()));
		RETURN_IF_ERROR(styles.Add(style.release()));
	}
	return OpStatus::OK;
}

#endif // __GET_STYLE_RULES

#ifdef __DOM_INSPECTOR
#include "modules/scope/src/scope_dom_inspector.h"
#endif // __DOM_INSPECTOR

OP_STATUS
GetFramesDocument(FramesDocument **frm_doc, ES_Object* es_object)
{
	EcmaScript_Object *host_object = ES_Runtime::GetHostObject(es_object);

	if (host_object == 0)
		return OpStatus::ERR;

	ES_Runtime* runtime = host_object->GetRuntime();

	if (runtime == 0)
		return OpStatus::ERR;

	*frm_doc = runtime->GetFramesDocument();

	if (*frm_doc == 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS
GetVisualDevice(VisualDevice **vis_dev, ES_Object* es_object)
{
	FramesDocument* frm_doc;
	RETURN_IF_ERROR(GetFramesDocument(&frm_doc, es_object));

	DocumentManager* doc_man = frm_doc->GetDocManager();
	if (doc_man == NULL)
		return OpStatus::ERR;

	*vis_dev = doc_man->GetVisualDevice();
	if (*vis_dev == NULL)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS
GetHTMLDocument(HTML_Document **html_doc, ES_Object* es_object)
{
	FramesDocument* frm_doc;
	RETURN_IF_ERROR(GetFramesDocument(&frm_doc, es_object));

	*html_doc = frm_doc->GetHtmlDocument();

	if (*html_doc == NULL)
		return OpStatus::ERR;

	return OpStatus::OK;
}

COLORREF
DecodeRGBA(unsigned color)
{
	return OP_RGBA((color >> 24) & 0xff, (color >> 16) & 0xff, (color >> 8) & 0xff, color & 0xff);
}

OP_STATUS
SetObjectValue(ES_ScopeDebugFrontend::ObjectValue &out, const ES_DebugObject &object)
{
	out.SetObjectID(object.id);

	ES_DebugObjectInfo* objinfo = object.info;
	if ( objinfo != 0 )
	{
		out.SetIsCallable(objinfo->flags.packed.is_callable);
		out.SetPrototypeID(objinfo->prototype.id);

		if (objinfo->flags.packed.is_function)
		{
			if ( objinfo->function_name != 0 && uni_strlen(objinfo->function_name) > 0 )
				RETURN_IF_ERROR(out.SetFunctionName(objinfo->function_name));
			RETURN_IF_ERROR(out.SetType(UNI_L("function")));
		}
		else
		{
			RETURN_IF_ERROR(out.SetType(UNI_L("object")));
		}

		uni_char* tmp = uni_up_strdup(objinfo->class_name);
		if ( tmp != 0 )
			RETURN_IF_ERROR(out.SetClassName(tmp));
		op_free(tmp);

	}
	return OpStatus::OK;
}

OP_STATUS ES_ScopeDebugFrontend::GetESObjectValue(OpString &type, OpString &value, OpAutoPtr<ObjectValue> &object_value, const ES_DebugValue &dvalue)
{
	OpString buffer;
	ANCHOR(OpString, buffer);

	switch ( dvalue.type )
	{
	case VALUE_UNDEFINED:
		return type.Set(UNI_L("undefined"));

	case VALUE_NULL:
		return type.Set(UNI_L("null"));

	case VALUE_BOOLEAN:
		RETURN_IF_ERROR(type.Set(UNI_L("boolean")));
		return value.Set(dvalue.value.boolean ? UNI_L("true") : UNI_L("false"));

	case VALUE_NUMBER:
		// Logic stolen from es_util.cpp : JString* tostring(double val)
		// Should be shared.
		RETURN_IF_ERROR(type.Set(UNI_L("number")));
		if (op_isnan(dvalue.value.number))
			return value.Set(UNI_L("NaN"));
		else if (dvalue.value.number == 0.0)	// Handle -0 case
			return value.Set(UNI_L("0"));
		else {
			char numberbuffer[32]; // ARRAY OK 2009-05-05 jhoff
			OpDoubleFormat::ToString(numberbuffer, dvalue.value.number);
			return value.Set(numberbuffer);
		}

	case VALUE_STRING:
		RETURN_IF_ERROR(type.Set(UNI_L("string")));

		if ( dvalue.is_8bit )
			return value.Set(dvalue.value.string8.value, dvalue.value.string8.length);
		else
			return value.Set(dvalue.value.string16.value, dvalue.value.string16.length);

	case VALUE_OBJECT:
		RETURN_IF_ERROR(type.Set(UNI_L("object")));
		object_value.reset(OP_NEW(ObjectValue, ()));
		RETURN_OOM_IF_NULL(object_value.get());
		RETURN_IF_ERROR(SetObjectValue(*object_value.get(), dvalue.value.object));
		return OpStatus::OK;

	default:
		OP_ASSERT(!"Unknown type for value");
		RETURN_IF_ERROR(type.Set(UNI_L("unknown")));
		return value.Set(UNI_L("internal error"));
	}
}

ES_ScopeDebugFrontend::ES_ScopeDebugFrontend()
	: ES_ScopeDebugFrontend_SI()
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	, window_manager(NULL)
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
	, current_spotligheted_doc(NULL)
	, inspect_object(NULL)
	, inspect_runtime(NULL)
	, inspect_object_id(0)
	, inspect_window_id(0)
	, property_filters(NULL)
	, runtime_list(NULL)
{
}

/* virtual */
ES_ScopeDebugFrontend::~ES_ScopeDebugFrontend()
{
	Detach();
}

OP_STATUS
ES_ScopeDebugFrontend::Construct()
{
	//return ES_DebugFrontend::ConstructEngineBackend();
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
ES_ScopeDebugFrontend::OnPostInitialization()
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (OpScopeServiceManager *manager = GetManager())
		window_manager = OpScopeFindService<OpScopeWindowManager>(manager, "window-manager");
	if (!window_manager)
		return OpStatus::ERR_NULL_POINTER;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::Detach()
{
	dbg_eventhandlers.Clear();
	UnreferenceInspectObject();

	OP_STATUS status = ES_DebugFrontend::Detach();

	OP_DELETE(property_filters);
	property_filters = NULL;

	console_timers.DeleteAll();

	return status;
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::OnServiceEnabled()
{
	OP_STATUS status = ES_DebugFrontend::ConstructEngineBackend(
#if defined(SCOPE_WINDOW_MANAGER_SUPPORT)
		window_manager
#endif
	);
	if (OpStatus::IsError(status))
		return status;
	RETURN_IF_ERROR(Connected());
	FramesDocument::SetAlwaysCreateDOMEnvironment(TRUE);

	return OpStatus::OK;
}

/* virtual */ void
ES_ScopeDebugFrontend::FilterChanged()
{
	ES_DebugFrontend::Prune();
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::OnServiceDisabled()
{
	FramesDocument::SetAlwaysCreateDOMEnvironment(FALSE);
	OP_STATUS status = OpStatus::OK, tmp = OpStatus::OK;
	tmp = Detach();
	if (OpStatus::IsError(tmp))
		status = tmp;
	tmp = ClearCurrentSpotlight();
	if (OpStatus::IsError(tmp))
		status = tmp;
	inspect_object_id = 0;

	return status;
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::RuntimeStarted(unsigned dbg_runtime_id, const ES_DebugRuntimeInformation* runtimeinfo)
{
	if (!IsEnabled())
		return OpStatus::OK;

	RuntimeInfo info;
	RETURN_IF_ERROR(SetRuntimeInformation(info, runtimeinfo));
	return SendOnRuntimeStarted(info);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::RuntimeStopped(unsigned dbg_runtime_id, const ES_DebugRuntimeInformation* runtimeinfo)
{
	if (!IsEnabled())
		return OpStatus::OK;

	RuntimeID id;
	id.SetRuntimeID(dbg_runtime_id);
	return SendOnRuntimeStopped(id);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::RuntimesReply(unsigned tag, OpVector<ES_DebugRuntimeInformation>& runtimes)
{
	OP_ASSERT(runtime_list != NULL);
	if (runtime_list == NULL)
		return OpStatus::ERR;

	for ( unsigned int i=0 ; i < runtimes.GetCount() ; i++ )
	{
		OpAutoPtr<RuntimeInfo> info(OP_NEW(RuntimeInfo, ()));
		RETURN_OOM_IF_NULL(info.get());
		RETURN_IF_ERROR(SetRuntimeInformation(*info.get(), runtimes.Get(i)));
		RETURN_IF_ERROR(runtime_list->GetRuntimeListRef().Add(info.release()));
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::NewScript(unsigned dbg_runtime_id, ES_DebugScriptData *data, const ES_DebugRuntimeInformation *runtimeinfo)
{
	if (!IsEnabled())
		return OpStatus::OK;

	ScriptInfo info;
	info.SetRuntimeID(dbg_runtime_id);
	info.SetScriptID(data->script_guid);

	RETURN_IF_ERROR(info.SetScriptType(GetScriptTypeName(data->type)));

	if (data->source)
		RETURN_IF_ERROR(info.SetScriptData(data->source));

	switch (data->type)
	{
	case SCRIPT_TYPE_LINKED:
	case SCRIPT_TYPE_USER_JAVASCRIPT:
	case SCRIPT_TYPE_USER_JAVASCRIPT_GREASEMONKEY:
	case SCRIPT_TYPE_BROWSER_JAVASCRIPT:
	case SCRIPT_TYPE_EXTENSION_JAVASCRIPT:
		RETURN_IF_ERROR(info.SetUri(data->url));
	}

	return SendOnNewScript(info);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::ParseError(unsigned dbg_runtime_id, ES_ErrorInfo *err)
{
	if (!IsEnabled())
		return OpStatus::OK;
	DomParseError error;
	RETURN_IF_ERROR(SetParseError(error, dbg_runtime_id, err));
	return SendOnParseError(error);
}

void
ES_ScopeDebugFrontend::ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state)
{
	if(!IsEnabled())
		return;

	unsigned state_out = 0;

	switch(state)
	{
	case OpScopeReadyStateListener::READY_STATE_BEFORE_DOM_CONTENT_LOADED:
		state_out = 1;
		break;
	case OpScopeReadyStateListener::READY_STATE_AFTER_ONLOAD:
		state_out = 2;
		break;
	default:
		return; // Ignore silently.
	}

	unsigned dbg_runtime_id = GetRuntimeId(doc->GetESRuntime());

	// This either means that the window is filtered, or OOM.
	if(dbg_runtime_id == 0)
		return;

	ReadyStateInfo s;
	s.SetRuntimeID(dbg_runtime_id);
	s.SetState(state_out);
	OpStatus::Ignore(SendOnReadyStateChanged(s));
}

void
ES_ScopeDebugFrontend::ConsoleLog(ES_Runtime *runtime, OpScopeJSConsoleListener::LogType type, ES_Value* argv, int argc)
{
	if(!IsEnabled())
		return;

	unsigned dbg_runtime_id = GetRuntimeId(runtime);

	if(dbg_runtime_id == 0)
	{
		SilentError(OpStatus::ERR);
		return;
	}

	// Get current ES_Context.
	ES_ThreadScheduler *scheduler = runtime->GetESScheduler();
	ES_Context *context = scheduler->GetCurrentThread()->GetContext();

	ES_DebugValue *dbg_values = NULL;

	if(argc > 0)
	{
		dbg_values = OP_NEWA(ES_DebugValue, argc);

		if(!dbg_values)
		{
			SilentError(OpStatus::ERR_NO_MEMORY);
			return;
		}

		for(unsigned i = 0; i<(unsigned)argc; ++i)
			ExportValue(runtime, argv[i], dbg_values[i], FALSE);
	}

	ES_Runtime::CallerInformation call_info;
	if (OpStatus::IsMemoryError(runtime->GetCallerInformation(context, call_info)))
	{
		SilentError(OpStatus::ERR_NO_MEMORY);
		return;
	}

	ES_DebugPosition position(call_info.script_guid, call_info.line_no);

	ConsoleLogInfo msg;

	msg.SetRuntimeID(dbg_runtime_id);
	msg.SetType(ConvertConsoleLogType(type));

	OP_STATUS status = SetConsoleLogInfo(msg, argc, dbg_values);

	if(OpStatus::IsError(status))
	{
		OP_DELETEA(dbg_values);
		SilentError(status);
		return;
	}

	if(position.Valid())
	{
		OpAutoPtr<Position> pos(OP_NEW(Position, ()));
		if (!pos.get())
		{
			SilentError(OpStatus::ERR_NO_MEMORY);
			return;
		}
		status = SetPosition(*pos.get(), position);

		if(OpStatus::IsError(status))
		{
			SilentError(status);
			return;
		}

		msg.SetPosition(pos.release());
	}

	status = SendOnConsoleLog(msg);

	if(OpStatus::IsError(status))
	{
		SilentError(status);
		return;
	}

	OP_DELETEA(dbg_values);
}

void
ES_ScopeDebugFrontend::ConsoleTrace(ES_Runtime *runtime)
{
	if (!IsEnabled())
		return;

	OP_ASSERT(runtime);

	ES_DebugStackFrame *frames = NULL;
	unsigned length;

	ES_Thread *current_thread = runtime->GetESScheduler() ? runtime->GetESScheduler()->GetCurrentThread() : NULL;
	ES_Context *context = current_thread ? current_thread->GetContext() : NULL;

	if (!current_thread && !context)
	{
		SilentError(OpStatus::ERR);
		return;
	}

	ConsoleTraceInfo msg;

	OP_STATUS status = Backtrace(runtime, context, ES_DebugControl::ALL_FRAMES, length, frames);

	if(OpStatus::IsSuccess(status))
		status = SetConsoleTraceInfo(msg, runtime, length, frames);

	OP_DELETEA(frames);

	if(OpStatus::IsSuccess(status))
		status = SendOnConsoleTrace(msg);

	if(OpStatus::IsError(status))
		SilentError(status);
}

void
ES_ScopeDebugFrontend::ConsoleTime(ES_Runtime *runtime, const uni_char *title)
{
	if(!IsEnabled())
		return;

	unsigned dbg_runtime_id = GetRuntimeId(runtime);

	if(dbg_runtime_id == 0)
	{
		SilentError(OpStatus::ERR);
		return;
	}

	double * d = OP_NEW(double, ());

	if(!d)
	{
		SilentError(OpStatus::ERR_NO_MEMORY);
		return;
	}

	*d = g_op_time_info->GetTimeUTC();

	OP_STATUS status = console_timers.Add(title, d);

	if(OpStatus::IsError(status))
	{
		OP_DELETE(d);
		SilentError(status);
		return;
	}

	ConsoleTimeInfo msg;

	if(OpStatus::IsSuccess(status))
		status = SetConsoleTimeInfo(msg, dbg_runtime_id, title);

	if(OpStatus::IsSuccess(status))
		status = SendOnConsoleTime(msg);

	if(OpStatus::IsError(status))
		SilentError(status);
}

void
ES_ScopeDebugFrontend::ConsoleTimeEnd(ES_Runtime *runtime, const uni_char *title)
{
	if(!IsEnabled())
		return;

	unsigned dbg_runtime_id = GetRuntimeId(runtime);

	if(dbg_runtime_id == 0)
	{
		SilentError(OpStatus::ERR);
		return;
	}

	double * d = 0;

	if(OpStatus::IsError(console_timers.Remove(title, &d)))
		return; // No such element.

	double now = g_op_time_info->GetTimeUTC();

	ConsoleTimeEndInfo msg;

	OP_STATUS status = SetConsoleTimeEndInfo(msg, dbg_runtime_id, title, (unsigned)((now - *d)*1000.0));

	OP_DELETE(d);

	if(OpStatus::IsSuccess(status))
		status = SendOnConsoleTimeEnd(msg);

	if(OpStatus::IsError(status))
		SilentError(status);
}

void
ES_ScopeDebugFrontend::ConsoleProfile(ES_Runtime *runtime, const uni_char *title)
{
	if(!IsEnabled())
		return;

	unsigned dbg_runtime_id = GetRuntimeId(runtime);

	if(dbg_runtime_id == 0)
	{
		SilentError(OpStatus::ERR);
		return;
	}

	ConsoleProfileInfo msg;
	msg.SetRuntimeID(dbg_runtime_id);

	OP_STATUS status = SendOnConsoleProfile(msg);

	if(OpStatus::IsError(status))
		SilentError(status);
}

void
ES_ScopeDebugFrontend::ConsoleProfileEnd(ES_Runtime *runtime)
{
	if(!IsEnabled())
		return;

	unsigned dbg_runtime_id = GetRuntimeId(runtime);

	if(dbg_runtime_id == 0)
		return;

	ConsoleProfileEndInfo msg;

	OP_STATUS status = SetConsoleProfileEndInfo(msg, dbg_runtime_id);

	if(OpStatus::IsSuccess(status))
		status = SendOnConsoleProfileEnd(msg);

	if(OpStatus::IsError(status))
		SilentError(status);
}

void
ES_ScopeDebugFrontend::ConsoleException(ES_Runtime *runtime, ES_Value* argv, int argc)
{
	// To support this silly feature in some basic way, we map 'console.exception'
	// calls to two events: OnConsoleLog(type=error) + OnConsoleTrace. Although
	// it's not possible to understand it from the function name, this is
	// actually what 'console.exception' does (in Firebug).
	//
	// Also note that 'console.exception' does not throw an exception.
	ConsoleLog(runtime, OpScopeJSConsoleListener::LOG_ERROR, argv, argc);
	ConsoleTrace(runtime);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::ThreadStarted(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo)
{
	if (!IsEnabled())
		return OpStatus::OK;

	ThreadInfo info;
	RETURN_IF_ERROR(SetThreadStartedInfo(info, dbg_runtime_id, dbg_thread_id, dbg_parent_thread_id, type, namespace_uri, event_type, runtimeinfo));
	return SendOnThreadStarted(info);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::ThreadFinished(unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus thread_status, ES_DebugReturnValue *dbg_return_value)
{
	if (!IsEnabled())
		return OpStatus::OK;

	ThreadResult result;
	RETURN_IF_ERROR(SetThreadFinishedResult(result, dbg_runtime_id, dbg_thread_id, thread_status, dbg_return_value));
	return SendOnThreadFinished(result);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::ThreadMigrated(unsigned dbg_thread_id, unsigned dbg_from_runtime_id, unsigned dbg_to_runtime_id)
{
	if (!IsEnabled())
		return OpStatus::OK;

	ThreadMigration out;
	out.SetThreadID(dbg_thread_id);
	out.SetFromRuntimeID(dbg_from_runtime_id);
	out.SetToRuntimeID(dbg_to_runtime_id);

	return SendOnThreadMigrated(out);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::StoppedAt(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data)
{
	if (!IsEnabled())
		return Continue(dbg_runtime_id, RUN);

	ThreadStopInfo info;
	RETURN_IF_ERROR(SetThreadStoppedAtInfo(info, dbg_runtime_id, dbg_thread_id, data));
	return SendOnThreadStoppedAt(info);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::EvalReply(unsigned tag, EvalStatus evalstatus, const ES_DebugValue &result)
{
	EvalResult out;
	RETURN_IF_ERROR(SetEvalResult(out, tag, evalstatus, result));
	return SendEval(out, tag);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::ExamineObjectsReply(unsigned tag, unsigned object_count, ES_DebugObjectChain *result)
{
	ObjectChainList list;
	RETURN_IF_ERROR(SetObjectChainList(list, tag, object_count, result));
	return SendExamineObjects(list, tag);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::FunctionCallStarting(unsigned dbg_runtime_id, unsigned threadid, ES_Value* arguments, int argc, ES_Object* thisval, ES_Object* function, const ES_DebugPosition& position)
{
	if (!IsEnabled())
		return OpStatus::OK;

	ES_Runtime *runtime = GetRuntimeById(dbg_runtime_id);

	FunctionCallStartingInfo msg;

	msg.SetTime(g_op_time_info->GetTimeUTC());

	msg.SetRuntimeID(dbg_runtime_id);

	msg.SetThreadID(threadid);

	RETURN_IF_ERROR(ExportObjectValue(runtime, msg.GetThisObjectRef(), thisval));
	RETURN_IF_ERROR(ExportObjectValue(runtime, msg.GetFunctionObjectRef(), function));

	RETURN_IF_ERROR(SetFunctionCallArguments(runtime, msg, argc, arguments));

	if (position.Valid())
	{
		OpAutoPtr<Position> pos(OP_NEW(Position, ()));
		RETURN_OOM_IF_NULL(pos.get());
		RETURN_IF_ERROR(SetPosition(*pos.get(), position));
		msg.SetPosition(pos.release());
	}

	return SendOnFunctionCallStarting(msg);
}

/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::FunctionCallCompleted(unsigned dbg_runtime_id, unsigned threadid, const ES_Value &returnvalue, BOOL exception_thrown)
{
	FunctionCallCompletedInfo msg;

	msg.SetTime(g_op_time_info->GetTimeUTC());
	msg.SetRuntimeID(dbg_runtime_id);
	msg.SetThreadID(threadid);
	msg.SetResult(exception_thrown ? FunctionCallCompletedInfo::RESULT_EXCEPTION : FunctionCallCompletedInfo::RESULT_SUCCESS);

	OpAutoPtr<Value> rval(OP_NEW(Value, ()));
	RETURN_OOM_IF_NULL(rval.get());

	RETURN_IF_ERROR(ExportToValue(GetRuntimeById(dbg_runtime_id), returnvalue, *rval.get()));

	msg.SetReturnValue(rval.release());
	return SendOnFunctionCallCompleted(msg);
}

OP_STATUS
ES_ScopeDebugFrontend::ExportToValue(ES_Runtime* runtime, const ES_Value &value, ES_ScopeDebugFrontend_SI::Value &out)
{
	ES_DebugValue dbg_value;
	ExportValue(runtime, value, dbg_value);
	return SetValue(out, dbg_value);
}

const uni_char *
ES_ScopeDebugFrontend::GetScriptTypeName(ES_ScriptType type) const
{
	switch (type)
	{
	case SCRIPT_TYPE_LINKED:
		return UNI_L("linked");
	case SCRIPT_TYPE_INLINE:
		return UNI_L("inline");
	case SCRIPT_TYPE_GENERATED:
		return UNI_L("generated");
	case SCRIPT_TYPE_EVENT_HANDLER:
		return UNI_L("event handler");
	case SCRIPT_TYPE_EVAL:
		return UNI_L("eval");
	case SCRIPT_TYPE_TIMEOUT:
		return UNI_L("timeout");
	case SCRIPT_TYPE_DEBUGGER:
		return UNI_L("debugger-eval");
	case SCRIPT_TYPE_JAVASCRIPT_URL:
		return UNI_L("uri");
	case SCRIPT_TYPE_USER_JAVASCRIPT:
		return UNI_L("User JS");
	case SCRIPT_TYPE_USER_JAVASCRIPT_GREASEMONKEY:
		return UNI_L("Greasemonkey JS");
	case SCRIPT_TYPE_BROWSER_JAVASCRIPT:
		return UNI_L("Browser JS");
	case SCRIPT_TYPE_EXTENSION_JAVASCRIPT:
		return UNI_L("Extension JS");
	default:
		OP_ASSERT(!"unhandled script type!");
	case SCRIPT_TYPE_UNKNOWN:
		return UNI_L("unknown");
	}
}

/* virtual */
OP_STATUS
ES_ScopeDebugFrontend::EvalError(unsigned tag, const uni_char *message, unsigned line, unsigned offset)
{
	OpScopeTPError tp_err(OpScopeTPHeader::BadRequest, NULL, line, -1, offset);
	RETURN_IF_ERROR(tp_err.SetDescription(message));
	RETURN_IF_MEMORY_ERROR(SetCommandError(tp_err));
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::HandleEvent(const uni_char* event_name, unsigned target_id, unsigned handler_id)
{
	DomEvent evt;
	RETURN_IF_ERROR(SetDomEventInfo(evt, event_name, target_id, handler_id));
	return SendOnHandleEvent(evt); // TODO: Should the name of the event be OnDomEvent?
}

OP_STATUS
ES_ScopeDebugFrontend::HandleEvent(const uni_char* event_name, ES_Runtime *runtime, ES_Object* target, unsigned handler_id)
{
	DomEvent evt;
	RETURN_IF_ERROR(SetDomEventInfo(evt, event_name, GetObjectId(runtime, target), handler_id));
	return SendOnHandleEvent(evt); // TODO: Should the name of the event be OnDomEvent?
}


/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::Connected()
{
	return OpStatus::OK;
}

unsigned
ES_ScopeDebugFrontend::GetHTMLObjectID(DOM_Environment* environment, HTML_Element* elm)
{
	DOM_Object *dom_obj;
	if (OpStatus::IsSuccess(environment->ConstructNode(dom_obj, elm)))
	{
		return GetObjectId(GetESRuntime(dom_obj), DOM_Utils::GetES_Object(dom_obj));
	}
	return 0;
}



/* virtual */ OP_STATUS
ES_ScopeDebugFrontend::Closed()
{
	return Detach();
}

OP_STATUS
ES_ScopeDebugFrontend::SetRuntimeInformation(RuntimeInfo &info, const ES_DebugRuntimeInformation *runtimeinfo)
{
	if (!runtimeinfo)
		return OpStatus::ERR;

	info.SetRuntimeID(runtimeinfo->dbg_runtime_id);
	RETURN_IF_ERROR(info.SetHtmlFramePath(runtimeinfo->framepath));

	unsigned count = runtimeinfo->windows.GetCount();

	// If there is a one-to-one relationship between a runtime and window, set the field.
	info.SetWindowID(count == 1 ? runtimeinfo->windows.Get(0)->Id() : 0);

	// Add all associated windows.
	for (unsigned i = 0; i < count; ++i)
		RETURN_IF_ERROR(info.GetWindowIDListRef().Add(runtimeinfo->windows.Get(i)->Id()));

	info.SetObjectID(runtimeinfo->dbg_globalobject_id);
	if (runtimeinfo->description)
		RETURN_IF_ERROR(info.GetDescriptionRef().Set(runtimeinfo->description));

	RETURN_IF_ERROR(info.SetUri(runtimeinfo->documenturi));

#ifdef EXTENSION_SUPPORT
	if (runtimeinfo->extension_name)
		RETURN_IF_ERROR(info.SetExtensionName(runtimeinfo->extension_name));
#endif // EXTENSION_SUPPORT

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetParseError(DomParseError &error, unsigned dbg_runtime_id, ES_ErrorInfo *err)
{
	error.SetRuntimeID(dbg_runtime_id);
	error.SetScriptID(err->script_guid);
	error.SetLineNumber(err->error_position.GetRelativeLine());
	error.SetOffset(err->error_position.GetRelativeIndex());
	error.SetContext(err->error_context);
	return error.SetDescription(err->error_text);
}

OP_STATUS
ES_ScopeDebugFrontend::SetPosition(Position &msg, const ES_DebugPosition &position)
{
	msg.SetScriptID(position.scriptid);
	msg.SetLineNumber(position.linenr);
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetConsoleLogInfo(ConsoleLogInfo &msg, unsigned values_count, ES_DebugValue *values)
{
	for (unsigned i = 0; i < values_count; ++i)
	{
		ConsoleLogInfo::Value *console_log_value = msg.AppendNewValueList();
		RETURN_OOM_IF_NULL(console_log_value);

		OpString type, value;
		OpAutoPtr<ObjectValue> object_value(OP_NEW(ObjectValue, ()));
		RETURN_IF_ERROR(GetESObjectValue(type, value, object_value, values[i]));

		switch (values[i].type)
		{
		case VALUE_OBJECT:
			console_log_value->SetObjectValue(object_value.release());
			break;
		case VALUE_UNDEFINED:
		case VALUE_NULL:
			// The variable 'value' is empty in this case. Let's use 'type'.
			RETURN_IF_ERROR(console_log_value->SetValue(type));
			break;
		default:
			RETURN_IF_ERROR(console_log_value->SetValue(value));
			break;
		}
	}
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetFunctionCallArguments(ES_Runtime* runtime, FunctionCallStartingInfo &msg, unsigned values_count, ES_Value* ext_values)
{
	for(unsigned i = 0; i < values_count; ++i)
	{
		Value *functioncalled_value = msg.AppendNewArgumentList();
		RETURN_OOM_IF_NULL(functioncalled_value);
		RETURN_IF_ERROR(ExportToValue(runtime, ext_values[i], *functioncalled_value));
	}
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetConsoleTimeInfo(ConsoleTimeInfo &msg, unsigned dbg_runtime_id, const uni_char *title)
{
	if(title == NULL)
		return OpStatus::ERR_NULL_POINTER;

	msg.SetRuntimeID(dbg_runtime_id);
	msg.SetTitle(title);
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetConsoleTimeEndInfo(ConsoleTimeEndInfo &msg, unsigned dbg_runtime_id, const uni_char *title, unsigned elapsed)
{
	if(title == NULL)
		return OpStatus::ERR_NULL_POINTER;

	msg.SetRuntimeID(dbg_runtime_id);
	msg.SetTitle(title);
	msg.SetElapsed(elapsed);
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetConsoleTraceInfo(ConsoleTraceInfo &msg, ES_Runtime *runtime, unsigned length, ES_DebugStackFrame *stack)
{
	unsigned dbg_runtime_id = GetRuntimeId(runtime);
	msg.SetRuntimeID(dbg_runtime_id);
	OpProtobufMessageVector<BacktraceFrame> &frames = msg.GetFrameListRef();
	for (unsigned index = 0; index < length; ++index)
	{
		OpAutoPtr<BacktraceFrame> frame(OP_NEW(BacktraceFrame, ()));
		RETURN_OOM_IF_NULL(frame.get());

		RETURN_IF_ERROR(SetBacktraceFrame(*frame.get(), stack[index]));
		RETURN_IF_ERROR(frames.Add(frame.release()));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetConsoleProfileEndInfo(ConsoleProfileEndInfo &msg, unsigned dbg_runtime_id)
{
	msg.SetRuntimeID(dbg_runtime_id);
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetThreadStartedInfo(ThreadInfo &info, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo)
{
	info.SetRuntimeID(dbg_runtime_id);
	info.SetThreadID(dbg_thread_id);
	info.SetParentThreadID(dbg_parent_thread_id);

	const uni_char* t = NULL;
	switch (type)
	{
	case THREAD_TYPE_INLINE: t = UNI_L("inline"); break;
	case THREAD_TYPE_EVENT:  t = UNI_L("event"); break;
	case THREAD_TYPE_URL:    t = UNI_L("linked"); break;
	case THREAD_TYPE_TIMEOUT:t = UNI_L("timeout"); break;
	case THREAD_TYPE_OTHER:  t = UNI_L("unknown"); break;
	case THREAD_TYPE_DEBUGGER_EVAL:   t = UNI_L("debugger-eval"); break;
	default:                 t = UNI_L("unknown"); OP_ASSERT(!"Add case for new thread type");
	}
	RETURN_IF_ERROR(info.SetThreadType(t));

	if (type == THREAD_TYPE_EVENT)
	{
		RETURN_IF_ERROR(info.SetEventNamespace(namespace_uri));
		RETURN_IF_ERROR(info.SetEventType(event_type));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetThreadFinishedResult(ThreadResult &result, unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus status, ES_DebugReturnValue *dbg_return_value)
{
	result.SetRuntimeID(dbg_runtime_id);
	result.SetThreadID(dbg_thread_id);

	const uni_char *t = NULL;
	switch (status)
	{
	case THREAD_STATUS_FINISHED:  t = UNI_L("completed"); break;
	case THREAD_STATUS_EXCEPTION: t = UNI_L("unhandled-exception"); break;
	case THREAD_STATUS_ABORTED:   t = UNI_L("aborted"); break;
	case THREAD_STATUS_CANCELLED: t = UNI_L("cancelled-by-scheduler"); break;
	default:                      OP_ASSERT(!"Add a case for new thread status");
	}

	RETURN_IF_ERROR(result.SetStatus(t));

	OP_DELETE(dbg_return_value);

	return OpStatus::OK;
}

static const uni_char* GetReasonName(ES_DebugStopReason stopped_reason)
{
	switch (stopped_reason) {
		default:
			OP_ASSERT(!"Stopped reason out of range");
			/* fall through */
		case STOP_REASON_UNKNOWN:
			return UNI_L("unknown");
		case STOP_REASON_NEW_SCRIPT:
			return UNI_L("new script");
		case STOP_REASON_EXCEPTION:
			return UNI_L("exception");
		case STOP_REASON_ERROR:
			return UNI_L("error");
		case STOP_REASON_HANDLED_ERROR:
			return UNI_L("handled error");
		case STOP_REASON_ABORT:
			return UNI_L("abort");
		case STOP_REASON_BREAK:
			return UNI_L("broken");
		case STOP_REASON_DEBUGGER_STATEMENT:
			return UNI_L("debugger statement");
		case STOP_REASON_BREAKPOINT:
			return UNI_L("breakpoint");
		case STOP_REASON_STEP:
			return UNI_L("step");
	}
}

OP_STATUS
ES_ScopeDebugFrontend::SetThreadStoppedAtInfo(ThreadStopInfo &info, unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data)
{
	info.SetRuntimeID(dbg_runtime_id);
	info.SetThreadID(dbg_thread_id);
	info.SetScriptID(data.position.scriptid);
	info.SetLineNumber(data.position.linenr);

	RETURN_IF_ERROR(info.SetStoppedReason(GetReasonName(data.reason)));

	switch (data.reason)
	{
	case STOP_REASON_BREAKPOINT:
		info.SetBreakpointID(data.u.breakpoint_id);
		break;
	case STOP_REASON_EXCEPTION:
	case STOP_REASON_ERROR:
		{
			OpAutoPtr<Value> value(OP_NEW(Value, ()));
			RETURN_OOM_IF_NULL(value.get());
			RETURN_IF_ERROR(SetValue(*value.get(), *data.u.exception));
			info.SetExceptionValue(value.release());
		}
		break;
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetBacktraceFrame(BacktraceFrame &frame, ES_DebugStackFrame &stack)
{
	frame.SetFunctionID(stack.function.id);
	frame.SetArgumentObject(stack.arguments.id);
	frame.SetVariableObject(stack.variables);
	frame.SetThisObject(stack.this_obj.id);

	unsigned script_guid = stack.script_guid;
	if (script_guid)
	{
		frame.SetScriptID(script_guid);
		frame.SetLineNumber(stack.line_no);
	}

	if (stack.arguments.id)
	{
		OpAutoPtr<ObjectValue> argument_value(OP_NEW(ObjectValue, ()));
		RETURN_OOM_IF_NULL(argument_value.get());
		RETURN_IF_ERROR(SetObjectValue(*argument_value.get(), stack.arguments));
		frame.SetArgumentValue(argument_value.release());
	}

	if (stack.this_obj.id)
	{
		OpAutoPtr<ObjectValue> this_value(OP_NEW(ObjectValue, ()));
		RETURN_OOM_IF_NULL(this_value.get());
		RETURN_IF_ERROR(SetObjectValue(*this_value.get(), stack.this_obj));
		frame.SetThisValue(this_value.release());
	}

	if (stack.function.id)
	{
		OpAutoPtr<ObjectValue> object_value(OP_NEW(ObjectValue, ()));
		RETURN_OOM_IF_NULL(object_value.get());
		RETURN_IF_ERROR(SetObjectValue(*object_value.get(), stack.function));
		frame.SetObjectValue(object_value.release());
	}

	// Add scope chain information.
	for (unsigned i = 0; i < stack.scope_chain_length; ++i)
		RETURN_IF_ERROR(frame.GetScopeListRef().Add(stack.scope_chain[i]));

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetBacktraceList(BacktraceFrameList &list, unsigned length, ES_DebugStackFrame *stack)
{
	OpProtobufMessageVector<BacktraceFrame> &frames = list.GetFrameListRef();
	for (unsigned index = 0; index < length; ++index)
	{
		OpAutoPtr<BacktraceFrame> frame(OP_NEW(BacktraceFrame, ()));
		RETURN_OOM_IF_NULL(frame.get());
		RETURN_IF_ERROR(SetBacktraceFrame(*frame.get(), stack[index]));
		RETURN_IF_ERROR(frames.Add(frame.get()));
		frame.release();
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetBacktraceReturnValues(BacktraceFrameList &list, const ES_DebugReturnValue *value)
{
	OpProtobufMessageVector<ReturnValue> &return_values = list.GetReturnValueListRef();

	while (value)
	{
		OpAutoPtr<ReturnValue> retval(OP_NEW(ReturnValue, ()));
		RETURN_OOM_IF_NULL(retval.get());
		RETURN_IF_ERROR(SetReturnValue(*retval, *value));
		RETURN_IF_ERROR(return_values.Add(retval.get()));
		retval.release();
		value = value->next;
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetReturnValue(ReturnValue &out, const ES_DebugReturnValue &in)
{
	// Set the return value.
	RETURN_IF_ERROR(SetValue(out.GetValueRef(), in.value));

	// Set function object.
	RETURN_IF_ERROR(SetObjectValue(out.GetFunctionFromRef(), in.function));

	// Set return value's context information (script id, line number).
	Position &position_from = out.GetPositionFromRef();
	position_from.SetScriptID(in.from.scriptid);
	position_from.SetLineNumber(in.from.linenr);

	Position &position_to = out.GetPositionToRef();
	position_to.SetScriptID(in.to.scriptid);
	position_to.SetLineNumber(in.to.linenr);

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetEvalResult(EvalResult &eval, unsigned tag, EvalStatus status, const ES_DebugValue &result)
{
	const uni_char *t = NULL;
	switch (status)
	{
	case EVAL_STATUS_FINISHED:  t = UNI_L("completed"); break;
	case EVAL_STATUS_EXCEPTION: t = UNI_L("unhandled-exception"); break;
	case EVAL_STATUS_ABORTED:   t = UNI_L("aborted"); break;
	case EVAL_STATUS_CANCELLED: t = UNI_L("cancelled-by-scheduler"); break;
	default:                    OP_ASSERT(!"Add a case for new eval status");
	}
	RETURN_IF_ERROR(eval.SetStatus(t));

	if (status == EVAL_STATUS_FINISHED || status == EVAL_STATUS_EXCEPTION)
	{
		OpString value;
		OpAutoPtr<ObjectValue> object_value;
		RETURN_IF_ERROR(GetESObjectValue(eval.GetTypeRef(), value, object_value, result));
		if (result.type == VALUE_OBJECT)
		{
			OP_ASSERT(object_value.get() != NULL);
			eval.SetObjectValue(object_value.release());
		}
		else
			RETURN_IF_ERROR(eval.SetValue(value));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetObjectInfo(ObjectInfo &object, const ES_DebugObjectProperties &prop)
{
	RETURN_IF_ERROR(SetObjectValue(object.GetValueRef(), prop.object));

	for ( unsigned int p = 0; p < prop.properties_count; p++ )
	{
		OpAutoPtr<ObjectInfo::Property> prop_info(OP_NEW(ObjectInfo::Property, ()));
		RETURN_OOM_IF_NULL(prop_info.get());
		RETURN_IF_ERROR(SetPropertyInfo(*prop_info.get(), prop.properties[p]));
		RETURN_IF_ERROR(object.GetPropertyListRef().Add(prop_info.release()));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetObjectList(ObjectList &list, ES_DebugObjectChain &chain)
{
	for(ES_DebugObjectChain * focal = &chain; focal; focal = focal->prototype)
	{
		OpAutoPtr<ObjectInfo> info(OP_NEW(ObjectInfo, ()));
		RETURN_OOM_IF_NULL(info.get());
		RETURN_IF_ERROR(SetObjectInfo(*info.get(), focal->prop));
		RETURN_IF_ERROR(list.GetObjectListRef().Add(info.release()));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetObjectChainList(ObjectChainList &list, unsigned tag, unsigned chains_count, ES_DebugObjectChain *chains)
{
	for(unsigned i = 0; i < chains_count; ++i)
	{
		OpAutoPtr<ObjectList> object_list(OP_NEW(ObjectList, ()));
		RETURN_OOM_IF_NULL(object_list.get());
		RETURN_IF_ERROR(SetObjectList(*object_list.get(), chains[i]));
		RETURN_IF_ERROR(list.GetObjectChainListRef().Add(object_list.release()));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetPropertyInfo(ObjectInfo::Property &info, ES_DebugObjectProperties::Property &p)
{
	OpString value;
	OpAutoPtr<ObjectValue> object_value;
	RETURN_IF_ERROR(GetESObjectValue(info.GetTypeRef(), value, object_value, p.value));
	RETURN_IF_ERROR(info.SetName(p.name));

	if (p.value_status == GETNATIVE_SCRIPT_GETTER)
		RETURN_IF_ERROR(info.SetType(UNI_L("script_getter")));
	else if (p.value_status == GETNATIVE_NEEDS_CALLSTACK)
		RETURN_IF_ERROR(info.SetType(UNI_L("function_caller_or_argument")));

	if (p.value.type == VALUE_OBJECT)
	{
		OP_ASSERT(object_value.get() != NULL);
		info.SetObjectValue(object_value.release());
	}
	else
		RETURN_IF_ERROR(info.SetValue(value));
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetDomEventInfo(DomEvent &evt, const uni_char* event_name, unsigned target_id, unsigned handler_id)
{
	evt.SetObjectID(target_id);
	evt.SetHandlerID(handler_id);
	return evt.SetEventType(event_name);
}

OP_STATUS
ES_ScopeDebugFrontend::ClearCurrentSpotlight()
{
#ifdef DISPLAY_SPOTLIGHT
	for ( Window* w = g_windowManager->FirstWindow() ; w != NULL ; w = w->Suc() )
	{
		DocumentTreeIterator iter(w);
		iter.SetIncludeThis();
		while (iter.Next())
		{
			VisualDevice* vis_dev = iter.GetFramesDocument()->GetDocManager()->GetVisualDevice();
			if (vis_dev)
				vis_dev->RemoveAllSpotlights();
		}
	}
#else
	if (current_spotligheted_doc)
	{
		for ( Window* w = g_windowManager->FirstWindow() ; w != NULL ; w = w->Suc() )
		{
			DocumentManager *docman = w->DocManager();
			if (docman)
			{
				FramesDocument *frm_doc = docman->GetCurrentDoc();
				if (frm_doc == current_spotligheted_doc)
				{
					current_spotligheted_doc->SetSpotlightedElm(0);
					continue;
				}
			}
		}
	}
	current_spotligheted_doc = 0;
#endif
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetSpotlight(unsigned object_id, BOOL scroll_into_view)
{
	FramesDocument* frm_doc = 0;
	HTML_Element *html_elm = 0;

	if (object_id != 0)
	{
		ES_Object *es_object = GetObjectById(object_id);
		if (es_object == 0)
			return OpStatus::ERR;

		RETURN_IF_ERROR(GetHTMLElement   (&html_elm,    es_object));
		RETURN_IF_ERROR(GetFramesDocument(&frm_doc,     es_object));
	}

	if (frm_doc != current_spotligheted_doc)
		ClearCurrentSpotlight();

	if (frm_doc)
	{
		frm_doc->SetSpotlightedElm(html_elm);
		HTML_Document* doc = frm_doc->GetHtmlDocument();
		if (scroll_into_view && doc)
			doc->ScrollToElement(html_elm, SCROLL_ALIGN_SPOTLIGHT, FALSE, VIEWPORT_CHANGE_REASON_SCOPE);
	}

	current_spotligheted_doc = frm_doc;

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::GetCSSIndexMap(CssIndexMap &map)
{
	OpString style_name;
	if (style_name.Reserve(CSS_PROPERTY_NAME_MAX_SIZE) == 0)
		return OpStatus::ERR;
	OpVector<OpString> &names = map.GetNameListRef();
	for (unsigned int property = 0;
		 property < CSS_PROPERTY_NAME_SIZE;
		 ++property)
	{
		OpAutoPtr<OpString> style_name(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(style_name.get());
		RETURN_IF_ERROR(style_name->Set(GetCSS_PropertyName(property)));
		style_name->MakeLower();
		RETURN_IF_ERROR(names.Add(style_name.release()));
	}
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::GetComputedStyle(OpVector<OpString> &list, CSS_DOMStyleDeclaration* style)
{
	OP_ASSERT(style != NULL);
	TempBuffer property_value;
	for (unsigned int property = 0;
		 property < CSS_PROPERTY_NAME_SIZE;
		 ++property)
	{
		RETURN_IF_ERROR(style->GetPropertyValue(&property_value, GetAliasedProperty(property)));
		OpAutoPtr<OpString> value_str(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(value_str.get());
		RETURN_IF_ERROR(value_str->Set(property_value.GetStorage()));
		RETURN_IF_ERROR(list.Add(value_str.release()));
		property_value.Clear();
	}
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::GetPropertyList(CssStylesheetRules::StylesheetRule &rule, DOM_Environment *environment, HTML_Element* elm, CSS_property_list *style)
{
	TempBuffer property_name, property_value;
	BOOL important;

	RETURN_IF_ERROR(property_name.Expand(CSS_PROPERTY_NAME_MAX_SIZE + 1));


	unsigned int inline_length = style->GetLength();
	for (unsigned int inline_idx = 0;
		inline_idx < inline_length;
		++inline_idx)
	{
		RETURN_IF_ERROR(style->GetPropertyNameAndValue(inline_idx, &property_name, &property_value, important));

		RETURN_IF_ERROR(rule.GetIndexListRef().Add(GetCSS_Property(property_name.GetStorage(), property_name.Length())));
		OpAutoPtr<OpString> value_str(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(value_str.get());
		RETURN_IF_ERROR(value_str->Set(property_value.GetStorage()));
		RETURN_IF_ERROR(rule.GetValueListRef().Add(value_str.release()));
		RETURN_IF_ERROR(rule.GetPriorityListRef().Add(important ? 1 : 0));

		property_name.Clear();
		property_value.Clear();
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::GetStylesheetList(CssStylesheetList &list, FramesDocument *doc, DOM_Environment *environment, CSSCollection* collection)
{
	OpProtobufMessageVector<CssStylesheetList::Stylesheet> &stylesheets = list.GetStylesheetListRef();

	CSSCollection::Iterator iter(collection, CSSCollection::Iterator::STYLESHEETS);

	CSS *css;
	while ((css = static_cast<CSS *>(iter.Next())))
	{
		OpAutoPtr<CssStylesheetList::Stylesheet> stylesheet(OP_NEW(CssStylesheetList::Stylesheet, ()));
		RETURN_OOM_IF_NULL(stylesheet.get());
		RETURN_IF_ERROR(GetStylesheet(*stylesheet.get(), doc, environment, css));
		RETURN_IF_ERROR(stylesheets.Add(stylesheet.release()));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::GetStylesheet(CssStylesheetList::Stylesheet &stylesheet, FramesDocument *doc, DOM_Environment *environment, CSS *css)
{
	HLDocProfile *hld_profile = doc->GetHLDocProfile();
	if (!hld_profile)
		return OpStatus::ERR;

	LogicalDocument *logdoc = hld_profile->GetLogicalDocument();
	OP_ASSERT(logdoc);
	OpString url_str;
	if (const URL *url = css->GetHRef(logdoc))
		RETURN_IF_ERROR(url->GetAttribute(URL::KUniName_Username_Escaped, 0, url_str));

	HTML_Element *html_el = css->GetHtmlElement();
	OP_ASSERT(html_el);
	OpStackAutoPtr<CSS_DOMMediaList> media_list(OP_NEW_L(CSS_DOMMediaList, (environment, html_el)));
	unsigned int media_count = media_list->GetMediaCount();

	DOM_Object *dom_obj;
	RETURN_IF_ERROR(environment->ConstructNode(dom_obj, html_el));
	ES_Object* owner_node_obj = DOM_Utils::GetES_Object(html_el->GetESElement());

	DOM_Object *dom_import_rule = NULL;
	CSS_DOMRule *import_rule = NULL;
	unsigned int import_owner_rule_id = 0;
	unsigned int import_owner_stylesheet_id = 0;

	if (html_el->IsCssImport()) // OWNER-RULE
	{
		RETURN_IF_ERROR(CSS_DOMStyleSheet::GetImportRule(import_rule, html_el, environment));

		/* Retrieve parentStylesheetID for the current imported stylesheet. */
		if (HTML_Element *elm = import_rule->GetStyleSheetElm())
		{
			DOM_Object *dom_parent_elm;
			RETURN_IF_ERROR(environment->ConstructNode(dom_parent_elm, elm));
			import_owner_stylesheet_id = GetStylesheetID(dom_parent_elm, NULL);
		}

		/* Retrieve ownerRuleID for the current imported stylesheet. */
		RETURN_IF_ERROR(environment->ConstructCSSRule(dom_import_rule, import_rule));
		import_owner_rule_id = GetObjectId(GetESRuntime(dom_import_rule), DOM_Utils::GetES_Object(dom_import_rule));
	}

	/* Retrieve objectID of the current stylesheet. */
	unsigned int stylesheet_id = GetStylesheetID(dom_obj, dom_import_rule);
	if (stylesheet_id == 0)
		return OpStatus::ERR;

	const uni_char *title = css->GetTitle();
	if (!title)
		title = UNI_L("");

	const uni_char* content_type = html_el->DOMGetAttribute(environment, ATTR_XML, UNI_L("type"), 0, TRUE, FALSE);
	if (!content_type)
		content_type = UNI_L("");

	stylesheet.SetObjectID(stylesheet_id);
	stylesheet.SetIsDisabled(!css->IsEnabled());
	RETURN_IF_ERROR(stylesheet.SetHref(url_str));
	RETURN_IF_ERROR(stylesheet.SetTitle(title));
	RETURN_IF_ERROR(stylesheet.SetType(content_type));

	TempBuffer media_name_tmp;
	for (unsigned int media_idx = 0; media_idx < media_count; ++media_idx)
	{
		CSS_DOMException dom_e;
		RETURN_IF_ERROR(media_list->GetMedium(&media_name_tmp, media_idx, dom_e));
		OP_ASSERT(media_name_tmp.GetStorage());
		OpAutoPtr<OpString> media_name(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(media_name.get());
		RETURN_IF_ERROR(media_name->Set(media_name_tmp.GetStorage()));
		RETURN_IF_ERROR(stylesheet.GetMediaListRef().Add(media_name.release()));
		media_name_tmp.Clear();
	}

	if (owner_node_obj && !import_rule)
		stylesheet.SetOwnerNodeID(GetObjectId(GetESRuntime(dom_obj), owner_node_obj));

	if (import_rule)
	{
		stylesheet.SetOwnerRuleID(import_owner_rule_id);
		stylesheet.SetParentStylesheetID(import_owner_stylesheet_id);
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::GetStylesheetRules(CssStylesheetRules &rules, DOM_Environment *environment, CSS* css, unsigned int object_id)
{
	unsigned int rule_count = css->GetRuleCount();

	for (unsigned int rule_idx = 0;
		 rule_idx < rule_count;
		 ++rule_idx)
	{
		CSS_Rule *rule = css->GetRule(rule_idx);
		OP_ASSERT(rule != NULL);
		OpAutoPtr<CssStylesheetRules::StylesheetRule> stylesheet_rule(OP_NEW(CssStylesheetRules::StylesheetRule, ()));
		RETURN_OOM_IF_NULL(stylesheet_rule.get());
		RETURN_IF_ERROR(GetCSSRule(*stylesheet_rule.get(), environment, css, object_id, rule));
		RETURN_IF_ERROR(rules.GetRuleListRef().Add(stylesheet_rule.release()));
	}
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::GetCSSRule(CssStylesheetRules::StylesheetRule &out, DOM_Environment *environment, CSS* css, unsigned int object_id, CSS_Rule* rule)
{
	CSS_Rule::Type rule_type = rule->GetType();
	CSS_DOMRule *dom_rule;
	RETURN_IF_ERROR(GetDOMRule(dom_rule, rule, css->GetHtmlElement(), environment));
	DOM_Object* dom_obj;
	RETURN_IF_ERROR(environment->ConstructCSSRule(dom_obj, dom_rule));
	unsigned rule_id = GetObjectId(GetESRuntime(dom_obj), DOM_Utils::GetES_Object(dom_obj));

	out.SetType(rule_type);
	out.SetStylesheetID(object_id);
	out.SetRuleID(rule_id);

#ifdef SCOPE_CSS_RULE_ORIGIN
	unsigned line_number = dom_rule->GetLineNumber();

	if (line_number)
		out.SetLineNumber(line_number);
#endif // SCOPE_CSS_RULE_ORIGIN

	if (rule_type == CSS_Rule::STYLE || rule_type == CSS_Rule::PAGE) // STYLE-RULE | PAGE-RULE
	{
		CSS_StyleRule *style_rule = static_cast<CSS_StyleRule *>(rule);
		CSS_property_list *prop_list = style_rule->GetPropertyList();

		if (rule_type == CSS_Rule::STYLE) // SELECTOR-LIST , SPECIFICITY-LIST
		{

			for (CSS_Selector *selector = style_rule->FirstSelector();
				 selector;
				 selector = static_cast<CSS_Selector*>(selector->Suc()))
			{
				selector->CalculateSpecificity();
				unsigned short specificity = selector->GetSpecificity();
				TempBuffer selector_text; // SELECTOR-TEXT
				RETURN_IF_ERROR(selector->GetSelectorText(css, &selector_text));

				RETURN_IF_ERROR(out.GetSpecificityListRef().Add(specificity));
				OpAutoPtr<OpString> selector_str(OP_NEW(OpString, ()));
				RETURN_OOM_IF_NULL(selector_str.get());
				RETURN_IF_ERROR(selector_str->Set(selector_text.GetStorage()));
				RETURN_IF_ERROR(out.GetSelectorListRef().Add(selector_str.release()));
			}
		}
		else if (rule_type == CSS_Rule::PAGE)
		{
			CSS_PageRule* page_rule = static_cast<CSS_PageRule*>(rule);

			unsigned short specificity = page_rule->GetSpecificity();
			const uni_char *page_name = page_rule->GetName();

			RETURN_IF_ERROR(out.GetSpecificityListRef().Add(specificity));
			OpAutoPtr<OpString> selector_str(OP_NEW(OpString, ()));
			RETURN_OOM_IF_NULL(selector_str.get());
			RETURN_IF_ERROR(selector_str->Set(page_name));
			RETURN_IF_ERROR(out.GetSelectorListRef().Add(selector_str.release()));
			out.SetPseudoClass(page_rule->GetPagePseudo());
		}
 		else
		{
			return OpStatus::ERR;
		}

		RETURN_IF_ERROR(GetPropertyList(out, environment, css->GetHtmlElement(), prop_list));
	}
	else if (rule_type == CSS_Rule::IMPORT) // IMPORT-RULE
	{
		CSS_ImportRule *import_rule = static_cast<CSS_ImportRule *>(rule);

		HTML_Element* elm = import_rule->GetElement();
		if (!elm)
			return OpStatus::ERR;

		RETURN_IF_ERROR(out.SetHref(elm->GetStringAttr(ATTR_HREF)));

		CSS_MediaObject *media = import_rule->GetMediaObject(FALSE); // MEDIA-LIST
		if (media)
		{
			TempBuffer media_name;
			unsigned int media_len = media->GetLength();
			for (unsigned int media_idx = 0;
				 media_idx < media_len;
				 ++media_idx)
			{
				RETURN_IF_ERROR(media->GetItemString(&media_name, media_idx));
				OpAutoPtr<OpString> media_str(OP_NEW(OpString, ()));
				RETURN_OOM_IF_NULL(media_str.get());
				RETURN_IF_ERROR(media_str->Set(media_name.GetStorage()));
				RETURN_IF_ERROR(out.GetMediaListRef().Add(media_str.release()));
				media_name.Clear();
			}
		}

		if (elm) // STYLESHEET-ID of the stylesheet associated with that import rule
		{
			DOM_Object *dom_import_obj;
			RETURN_IF_ERROR(environment->ConstructCSSRule(dom_import_obj, dom_rule));
			RETURN_IF_ERROR(environment->ConstructNode(dom_obj, elm));
			unsigned int imported_stylesheet_id = GetStylesheetID(dom_obj, dom_import_obj);
			out.SetImportStylesheetID(imported_stylesheet_id);
		}
	}
	else if ( rule_type == CSS_Rule::MEDIA) // MEDIA-RULE
	{
		CSS_MediaRule *media_rule = static_cast<CSS_MediaRule*>(rule);

		CSS_MediaObject *media = media_rule->GetMediaObject(); // MEDIA-LIST
		if (media)
		{
			TempBuffer media_name;
			unsigned int media_len = media->GetLength();
			for (unsigned int media_idx = 0;
				 media_idx < media_len;
				 ++media_idx)
			{
				RETURN_IF_ERROR(media->GetItemString(&media_name, media_idx));
				OpAutoPtr<OpString> media_str(OP_NEW(OpString, ()));
				RETURN_OOM_IF_NULL(media_str.get());
				RETURN_IF_ERROR(media_str->Set(media_name.GetStorage()));
				RETURN_IF_ERROR(out.GetMediaListRef().Add(media_str.release()));
				media_name.Clear();
			}
		}

		for (CSS_Rule *sub_rule = media_rule->FirstRule();
			 sub_rule;
			 sub_rule = static_cast<CSS_Rule*>(sub_rule->Suc()))
		{
			OpAutoPtr<CssStylesheetRules::StylesheetRule> stylesheet_rule(OP_NEW(CssStylesheetRules::StylesheetRule, ()));
			RETURN_OOM_IF_NULL(stylesheet_rule.get());
			RETURN_IF_ERROR(GetCSSRule(*stylesheet_rule.get(), environment, css, object_id, sub_rule));
			RETURN_IF_ERROR(out.GetRuleListRef().Add(stylesheet_rule.release()));
		}
	}
	else if ( rule_type == CSS_Rule::CHARSET) // CHARSET-RULE
	{
		CSS_CharsetRule *charset_rule = static_cast<CSS_CharsetRule*>(rule);

		RETURN_IF_ERROR(out.SetCharset(charset_rule->GetCharset()));
	}
	else if ( rule_type == CSS_Rule::FONT_FACE) // FONT_FACE-RULE
	{

		CSS_FontfaceRule *fontface_rule = static_cast<CSS_FontfaceRule*>(rule);
		CSS_property_list *prop_list = fontface_rule->GetPropertyList();
		if (prop_list == 0)
			return OpStatus::ERR;

		RETURN_IF_ERROR(GetPropertyList(out, environment, css->GetHtmlElement(), prop_list));
	}
	else // Handles also UNKNOWN rules
	{
	}

	return OpStatus::OK;
}

/** @return 0 or the Window who has frm_doc as its current FramesDocument */
static unsigned
GetWindowId(FramesDocument* frm_doc)
{
	for ( Window* w = g_windowManager->FirstWindow() ; w != NULL ; w = w->Suc() )
	{
		DocumentManager *docman = w->DocManager();
		if (docman)
		{
			FramesDocument *current_frm_doc = docman->GetCurrentDoc();
			if (current_frm_doc && current_frm_doc->HasSubDoc(frm_doc))
			{
				return w->Id();
			}
		}
	}
	return 0;
}

/** Find the Window with the given ID, or NULL if no such exists
    @param window_id The window id to check for
	@return A pointer to a Window where Window->Id() == window_id, or NULL if no such exists
 */
static Window*
GetWindowById(unsigned window_id)
{
	for ( Window* w = g_windowManager->FirstWindow() ; w != NULL ; w = w->Suc() )
		if (w->Id() == window_id)
			return w;
	return NULL;
}

void
ES_ScopeDebugFrontend::UnreferenceInspectObject()
{
	if (inspect_object)
	{
		inspect_runtime->Unprotect(inspect_object);
		inspect_object = NULL;
		inspect_runtime = NULL;
	}
}

OP_STATUS
ES_ScopeDebugFrontend::InspectElement(FramesDocument* frm_doc, HTML_Element* html_elm)
{
	UnreferenceInspectObject();

	if (!frm_doc || !html_elm)
		return OpStatus::ERR;

#ifdef _DEBUG
	// Allow core devs to debug opera: pages and other internal pages
	// which have scripting disabled.
	if (frm_doc->GetURL().GetAttribute(URL::KIsGeneratedByOpera))
		frm_doc->GetURL().SetAttribute(URL::KForceAllowScripts, TRUE);
#endif

	RETURN_IF_ERROR(frm_doc->ConstructDOMEnvironment());
	DOM_Environment* env = frm_doc->GetDOMEnvironment();

	inspect_window_id = GetWindowId(frm_doc);
	inspect_object_id = 0;

	DOM_Object *dom_obj;
	ES_Object *es_obj = NULL;
	if (OpStatus::IsSuccess(env->ConstructNode(dom_obj, html_elm)))
		es_obj = DOM_Utils::GetES_Object(dom_obj);

	if (IsEnabled())
	{
		inspect_object_id = GetObjectId(GetESRuntime(dom_obj), es_obj);
		ObjectSelection selection;
		BOOL has_selection;
		RETURN_IF_ERROR(SetSelectedObjectInfo(selection, has_selection));
		if (has_selection)
			RETURN_IF_ERROR(SendOnObjectSelected(selection));
	}
	else
	{
		if (env->GetRuntime()->Protect(es_obj))
		{
			inspect_object = es_obj;
			inspect_runtime = env->GetRuntime();
		}
	}
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetSelectedObjectInfo(ObjectSelection &selection, BOOL &has_selection)
{
	has_selection = FALSE;
	if (!IsEnabled())
		return OpStatus::OK;
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (!window_manager)
		return OpStatus::OK;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

	if (inspect_object)
	{
		if (GetWindowById(inspect_window_id)) // No need sending this information and keeping a reference to the object if the window has been deleted
			inspect_object_id = GetObjectId(inspect_runtime, inspect_object);

		UnreferenceInspectObject();
	}

	OP_MEMORY_VAR unsigned runtime_id = 0;
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (window_manager->AcceptWindow(inspect_window_id))
#endif
	{
		ES_Object *object = GetObjectById(inspect_object_id);
		EcmaScript_Object *host_object = object ? ES_Runtime::GetHostObject(object) : NULL;

		if (host_object == NULL)
		{
			inspect_object_id = 0;
				return OpStatus::OK;
		}
		else
			runtime_id = GetRuntimeId(host_object->GetRuntime());
	}

	selection.SetObjectID(inspect_object_id);
	selection.SetWindowID(inspect_window_id);
	if (runtime_id)
		selection.SetRuntimeID(runtime_id);
	has_selection = TRUE;

	return OpStatus::OK;
}

/*static*/
OP_STATUS
ES_ScopeDebugFrontend::GetDOMRule(CSS_DOMRule*& dom_rule, CSS_Rule* rule, HTML_Element* sheet_elm, DOM_Environment* dom_env)
{
	CSS_DOMRule* new_rule = rule->GetDOMRule();
	if (!new_rule)
	{
		new_rule = OP_NEW(CSS_DOMRule, (dom_env, rule, sheet_elm));
		RETURN_OOM_IF_NULL(new_rule);
		rule->SetDOMRule(new_rule);
	}

	dom_rule = new_rule;
	return OpStatus::OK;
}

ES_Runtime *
ES_ScopeDebugFrontend::GetESRuntime(DOM_Object *dom_obj)
{
	DOM_Runtime *dom_rt = DOM_Utils::GetDOM_Runtime(dom_obj);
	ES_Runtime *es_rt = dom_rt ? DOM_Utils::GetES_Runtime(dom_rt) : NULL;

	return es_rt;
}

CSSPseudoClassType
ES_ScopeDebugFrontend::Convert(PseudoSelector in)
{
	switch (in)
	{
	default:
		OP_ASSERT(!"Please add a new case");
	case PSEUDOSELECTOR_NONE: return CSS_PSEUDO_CLASS_UNKNOWN;
	case PSEUDOSELECTOR_HOVER: return CSS_PSEUDO_CLASS_HOVER;
	case PSEUDOSELECTOR_ACTIVE: return CSS_PSEUDO_CLASS_ACTIVE;
	case PSEUDOSELECTOR_FOCUS: return CSS_PSEUDO_CLASS_FOCUS;
	case PSEUDOSELECTOR_LINK: return CSS_PSEUDO_CLASS_LINK;
	case PSEUDOSELECTOR_VISITED: return CSS_PSEUDO_CLASS_VISITED;
	case PSEUDOSELECTOR_FIRST_LINE: return CSS_PSEUDO_CLASS_FIRST_LINE;
	case PSEUDOSELECTOR_FIRST_LETTER: return CSS_PSEUDO_CLASS_FIRST_LETTER;
	case PSEUDOSELECTOR_BEFORE: return CSS_PSEUDO_CLASS_BEFORE;
	case PSEUDOSELECTOR_AFTER: return CSS_PSEUDO_CLASS_AFTER;
	case PSEUDOSELECTOR_SELECTION: return CSS_PSEUDO_CLASS_SELECTION;
	}
}

#ifdef SVG_SUPPORT
OP_STATUS
ES_ScopeDebugFrontend::GetSVGRules(HTML_Element *elm, HLDocProfile *hld_profile, CSS_Properties &props, OpAutoVector<CSS_property_list> &rules, OpScopeStyleListener *listener)
{
	while (elm && elm->GetNsType() == NS_SVG)
	{
		OpAutoPtr<CSS_property_list> list(OP_NEW(CSS_property_list, ()));
		RETURN_OOM_IF_NULL(list.get());

		RETURN_IF_ERROR(g_svg_manager->GetPresentationalAttributes(elm, hld_profile, list.get()));

		if (list->GetLength() > 0)
		{
			// Set the properties in CSS props. This ensures that declaration status
			// is set correctly.
			for (CSS_decl *d = list->GetFirstDecl(); d != NULL; d = d->Suc())
				if (props.GetDecl(d->GetProperty()) == NULL)
					props.SetDecl(d->GetProperty(), d);

			RETURN_IF_ERROR(listener->SVGRule(elm, list.get()));
		}

		RETURN_IF_ERROR(rules.Add(list.get()));
		list.release();

		elm = elm->ParentActual();
	}

	return OpStatus::OK;
}
#endif // SVG_SUPPORT

unsigned int
ES_ScopeDebugFrontend::GetStylesheetID(DOM_Object* dom_obj, DOM_Object* import_rule)
{
	ES_Object *css_es = ES_ScopeGetStylesheetESObject(dom_obj, import_rule);
	if (!css_es)
		return 0;
	return GetObjectId(GetESRuntime(dom_obj), css_es);
}

OP_STATUS
ES_ScopeDebugFrontend::DoListRuntimes(const RuntimeSelection &in, RuntimeList &out)
{
	unsigned int tag = 0;
	// Note: The tag value should be ignored by the call to Runtimes so we set it to 0.
	//       Instead the current tag is handled by the code that calls this function.
	OpUINTPTRVector runtime_ids;
	BOOL force_create_all = in.GetAllRuntimes();
	const OpValueVector<UINT32> &list = in.GetRuntimeList();
	int count = list.GetCount();
	for (int i = 0; i < count; ++i)
		runtime_ids.Add((UINTPTR)list.Get(i));

	runtime_list = &out;
	OP_STATUS status = Runtimes(tag, runtime_ids, force_create_all);
	runtime_list = NULL;
	return status;
}

OP_STATUS
ES_ScopeDebugFrontend::DoContinueThread(const ThreadMode &in)
{
	unsigned int runtime_id = in.GetRuntimeID();
//	unsigned int thread_id = in.GetThreadID();  // FIXME: Thread ID is not used. Remove from spec?
	ES_DebugFrontend::Mode mode = RUN;
	const OpString &modeName = in.GetMode();
	if (modeName.Compare("run") == 0)
		mode = RUN;
	else if (modeName.Compare("step-into-call") == 0)
		mode = STEPINTO;
	else if (modeName.Compare("step-next-line") == 0)
		mode = STEPOVER;
	else if (modeName.Compare("step-out-of-call") == 0)
		mode = FINISH;
	else
		return OpStatus::ERR_PARSING_FAILED;

	return Continue(runtime_id, mode);
}


OP_STATUS
ES_ScopeDebugFrontend::DoSetPropertyFilter(const SetPropertyFilterArg &in)
{
	// Clear existing filter, if any.
	if (property_filters)
		property_filters->Clear();
	else
	{
		property_filters = OP_NEW(ES_ScopePropertyFilters, ());
		RETURN_OOM_IF_NULL(property_filters);
	}

	const PropertyFilter &filter = in.GetPropertyFilter();

	unsigned num_classes = filter.GetClassMaskList().GetCount();

	for (unsigned i = 0; i < num_classes; ++i)
	{
		PropertyFilter::ClassMask *mask = filter.GetClassMaskList().Get(i);
		RETURN_IF_ERROR(AddClassMask(*mask, *property_filters));
	}

	return OpStatus::OK;
}

OP_STATUS ES_ScopeDebugFrontend::DoSetFunctionFilter(const FunctionFilter &in)
{
	// set the functionfilter on the debugger, FunctionFilter contains a vector of opstrings with objectclasses

	const OpAutoVector<OpString>  &classList = in.GetClassList();
	unsigned num_classes = classList.GetCount();

	ResetFunctionFilter();

	for (unsigned i = 0; i<num_classes; ++i)
	{
		OpString8 classname8;
		RETURN_IF_ERROR(classname8.Set(classList.Get(i)->CStr()));
		RETURN_IF_ERROR(AddFunctionClassToFilter(classname8.CStr()));
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoSearchDom(const SearchDomArg &in, NodeList &out)
{
	ES_Runtime *runtime = GetRuntimeById(in.GetRuntimeID());

	if (!runtime)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No runtime with that ID"));

	DOM_Runtime *dom_runtime = DOM_Utils::GetDOM_Runtime(runtime);

	if (!dom_runtime)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Not a DOM runtime"));

	DOM_Utils::SearchOptions options(in.GetQuery().CStr(), dom_runtime);
	options.SetType(Convert(in.GetType()));
	options.SetIgnoreCase(in.GetIgnoreCase());

	DOM_Object *root = NULL;

	if (in.HasObjectID())
	{
		ES_Object *obj = GetObjectById(in.GetObjectID());

		if (!obj)
			return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("No object with that ID"));

		root = DOM_Utils::GetDOM_Object(obj);

		if (!root)
			return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Not a DOM object"));
	}

	options.SetRoot(root);

	return SearchDOMNodes(this, out.GetNodeListRef(), options);
}

OP_STATUS
ES_ScopeDebugFrontend::DoGetFunctionPositions(const GetFunctionPositionsArg &in, FunctionPositionList &out)
{
	unsigned runtime_id = in.GetRuntimeID();

	for (unsigned i = 0; i < in.GetObjectList().GetCount(); ++i)
	{
		unsigned object_id = in.GetObjectList().Get(i);

		FunctionPositionList::FunctionPosition *func_position = out.AppendNewFunctionPositionList();
		RETURN_OOM_IF_NULL(func_position);
		func_position->SetObjectID(object_id);

		unsigned script_guid, line_no;
		OP_STATUS status = GetFunctionPosition(runtime_id, object_id, script_guid, line_no);
		// Let scope handle other errors such as OOM
		if (OpStatus::IsError(status) && status != OpStatus::ERR && status != OpStatus::ERR_NO_SUCH_RESOURCE)
			return status;

		if (OpStatus::IsSuccess(status) && script_guid != ~0u && line_no != ~0u)
		{
			Position *position = OP_NEW(Position, (script_guid, line_no));
			RETURN_OOM_IF_NULL(position);
			func_position->SetPosition(position);
		}
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoGetEventNames(const GetEventNamesArg &in, EventNames &out)
{
	unsigned int runtime_id = in.GetRuntimeID();
	ES_Runtime *runtime = GetRuntimeById(in.GetRuntimeID());
	SCOPE_BADREQUEST_IF_NULL(runtime, this, UNI_L("No runtime with that ID"));

	DOM_Runtime *dom_runtime = DOM_Utils::GetDOM_Runtime(runtime);
	SCOPE_BADREQUEST_IF_NULL(dom_runtime, this, UNI_L("Not a DOM runtime"));

	DOM_Environment *environment = DOM_Utils::GetDOM_Environment(runtime);
	if (!environment)
		return OpStatus::ERR;

	DOM_Object *root = DOM_Utils::GetDocumentNode(dom_runtime);;
	SCOPE_BADREQUEST_IF_NULL(root, this, UNI_L("No document node in DOM runtime"));

	out.SetRuntimeID(runtime_id);
	out.SetObjectID(GetObjectId(runtime, DOM_Utils::GetES_Object(root)));

	RETURN_IF_ERROR(GetEventNames(out.GetEventNameListRef(), root));

	// Special case: Get event-names from the "window" object as well
	RETURN_IF_ERROR(GetEventNames(out.GetEventNameListRef(), environment->GetWindow(), FALSE));

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoGetEventListeners(const GetEventListenersArg &in, EventTargets &out)
{
	ES_Runtime *runtime = GetRuntimeById(in.GetRuntimeID());
	SCOPE_BADREQUEST_IF_NULL(runtime, this, UNI_L("No runtime with that ID"));
	out.SetRuntimeID(in.GetRuntimeID());

	for (unsigned i = 0; i < in.GetObjectList().GetCount(); ++i)
	{
		unsigned object_id = in.GetObjectList().Get(i);

		EventTarget *target_out = out.AppendNewTargetList();
		RETURN_OOM_IF_NULL(target_out);
		target_out->SetObjectID(object_id);

		ES_Object *es_obj = GetObjectById(runtime, object_id);
		if (!es_obj)
			continue;
		DOM_Object *dom_obj = DOM_Utils::GetDOM_Object(es_obj);
		if (!dom_obj)
			continue;
		DOM_EventTarget *event_target = dom_obj->GetEventTarget();
		if (!event_target)
			continue;

		DOM_EventTarget::NativeIterator it = event_target->NativeListeners();
		for (; !it.AtEnd(); it.Next())
		{
			const DOM_EventListener *listener = it.Current();
			EventListener *listener_out = target_out->AppendNewEventListenerList();
			RETURN_OOM_IF_NULL(listener_out);
			RETURN_IF_ERROR(SetEventListenerInfo(*listener_out, *listener, runtime));
		}
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoEval(const EvalData &in, unsigned tag)
{
	unsigned int runtime_id = in.GetRuntimeID();
	unsigned int thread_id = in.GetThreadID();
	unsigned int frame_index = in.GetFrameIndex();
	const OpString &script_data = in.GetScriptData();
	BOOL want_debugging = in.GetWantDebugging();

	const OpProtobufMessageVector<EvalData::Variable> &variable_list = in.GetVariableList();
	unsigned variables_count = variable_list.GetCount();

	ES_DebugVariable *variables = NULL;
	OpAutoArray<ES_DebugVariable> variables_anchor;

	if (variables_count > 0)
	{
		variables = OP_NEWA(ES_DebugVariable, variables_count);
		RETURN_OOM_IF_NULL(variables);
		variables_anchor.reset(variables);

		for (unsigned i = 0; i < variables_count; ++i)
		{
			const EvalData::Variable *prop = variable_list.Get(i);

			variables[i].name = prop->GetName().CStr();
			variables[i].value.value.object.id = prop->GetObjectID();
			variables[i].value.type = VALUE_OBJECT;
			variables[i].value.value.object.info = 0;
		}
	}

	OP_STATUS status = Eval(tag, runtime_id, thread_id, frame_index, script_data.CStr(), script_data.Length(), variables, variables_count, want_debugging);
	if (OpStatus::IsError(status))
	{
		if (GetCommandError().GetStatus() != OpScopeTPHeader::OK)
			return OpStatus::OK; // Error is already set so we just return normally
		RETURN_IF_ERROR(SetCommandError(OpScopeTPHeader::InternalError, UNI_L("Evaluation of script failed, reason unknown")));
		return status;
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoExamineObjects(const ExamineList &in, unsigned int async_tag)
{
	OP_STATUS status = OpStatus::OK;

	const OpValueVector<UINT32> &list = in.GetObjectList();
	unsigned objectids_count = list.GetCount();

	if(objectids_count > 0)
	{
		unsigned runtimeid = in.GetRuntimeID();
		BOOL examine_prototypes = in.GetExaminePrototypes();
		BOOL skip_non_enum = in.GetSkipNonenumerables();
		BOOL filterProperties = in.GetFilterProperties();

		unsigned *objectids = OP_NEWA(unsigned, objectids_count);
		RETURN_OOM_IF_NULL(objectids);

		for (unsigned i = 0; i < objectids_count; ++i)
			objectids[i] = list.Get(i);

		ES_DebugObjectChain *chains = NULL;

		status = Examine(runtimeid, objectids_count, objectids, examine_prototypes, skip_non_enum, filterProperties ? property_filters : NULL, chains, async_tag);

		OP_DELETEA(objectids);
	}
	else
		status = ExamineObjectsReply(async_tag, objectids_count, NULL);

 	return status;
}

OP_STATUS
ES_ScopeDebugFrontend::DoSpotlightObjects(const SpotlightSelection &in)
{
#ifdef DISPLAY_SPOTLIGHT
	ClearCurrentSpotlight();

	const OpProtobufMessageVector<SpotlightObject> &objects = in.GetSpotlightObjectList();
	for (unsigned int i = 0; i < objects.GetCount(); ++i)
	{
		SpotlightObject *object = objects.Get(i);
		if (object == NULL)
			return OpStatus::ERR_NULL_POINTER;

		unsigned object_id;
		unsigned box_type;
		BOOL scroll_into_view;
		VisualDevice* vis_dev;
		HTML_Element* html_elm;

		object_id = object->GetObjectID();
		scroll_into_view = object->GetScrollIntoView();

		OpAutoPtr<VDSpotlight> spotlight(OP_NEW(VDSpotlight, ()));
		RETURN_OOM_IF_NULL(spotlight.get());

		const OpProtobufMessageVector<SpotlightObject::SpotlightBox> &boxes = object->GetBoxList();
		for (unsigned int box_idx = 0; box_idx < boxes.GetCount(); ++box_idx)
		{
			SpotlightObject::SpotlightBox *box = boxes.Get(box_idx);
			if (box == NULL)
				return OpStatus::ERR_NULL_POINTER;

			VDSpotlightInfo* current_box;
			box_type = box->GetBoxType();
			if (box_type == 0)      current_box = &spotlight->content;
			else if (box_type == 1) current_box = &spotlight->padding;
			else if (box_type == 2) current_box = &spotlight->border;
			else if (box_type == 3) current_box = &spotlight->margin;
			else return OpStatus::ERR_PARSING_FAILED;

			if (box->HasFillColor())
				current_box->EnableFill(DecodeRGBA(box->GetFillColor()));
			if (box->HasFrameColor())
				current_box->EnableInnerFrame(DecodeRGBA(box->GetFrameColor()));
			if (box->HasGridColor())
				current_box->EnableGrid(DecodeRGBA(box->GetGridColor()));
		}

		ES_Object* es_obj = GetObjectById(object_id);

		if (es_obj == NULL)
			return OpStatus::ERR;

		RETURN_IF_ERROR(GetVisualDevice(&vis_dev, es_obj));
		RETURN_IF_ERROR(GetHTMLElement(&html_elm, es_obj));

		if (scroll_into_view)
		{
			HTML_Document* html_doc;
			RETURN_IF_ERROR(GetHTMLDocument(&html_doc, es_obj));
			html_doc->ScrollToElement(html_elm, SCROLL_ALIGN_SPOTLIGHT,
									  FALSE, VIEWPORT_CHANGE_REASON_SCOPE);
		}

		if (OpStatus::IsSuccess(vis_dev->AddSpotlight(spotlight.get(), html_elm)))
			spotlight.release();
	}
	return OpStatus::OK;
#else
	return OpStatus::ERR;
#endif
}

OP_STATUS
ES_ScopeDebugFrontend::DoReleaseObjects(const ReleaseObjectsArg &in)
{
	unsigned count = in.GetObjectList().GetCount();

	if(count == 0)
		FreeObjects(); // Free all
	else
		for (unsigned i = 0; i < count; ++i)
			FreeObject(in.GetObjectList().Get(i));

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoSpotlightObject(const SpotlightObjectSelection &in)
{
#if !defined(DISPLAY_SPOTLIGHT) && defined(_USE_SPOTLIGHT)
	unsigned object_id = in.GetObjectID();
	BOOL scroll_into_view = in.GetScrollIntoView();

	return SetSpotlight(object_id, scroll_into_view);
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif
}

OP_STATUS
ES_ScopeDebugFrontend::DoAddBreakpoint(const Breakpoint &in)
{
	unsigned int id = in.GetBreakpointID();
	ES_DebugBreakpointData breakpoint_data;

	if(id == 0)
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Breakpoint ID must be nonzero"));
		return OpStatus::ERR_PARSING_FAILED;
	}
	else if (HasBreakpoint(id))
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Breakpoint with same ID already exists"));
		return OpStatus::ERR_PARSING_FAILED;
	}

	breakpoint_data.type = breakpoint_data.TYPE_POSITION;
	breakpoint_data.data.position.scriptid = in.GetScriptID();
	breakpoint_data.data.position.linenr = in.GetLineNumber();

	return AddBreakpoint(id, breakpoint_data);
}

OP_STATUS
ES_ScopeDebugFrontend::DoAddEventBreakpoint(const EventBreakpoint &in)
{
	unsigned int id = in.GetBreakpointID();
	ES_DebugBreakpointData breakpoint_data;

	if(id == 0)
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Breakpoint ID must be nonzero"));
		return OpStatus::ERR_PARSING_FAILED;
	}
	else if (HasBreakpoint(id))
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Breakpoint with same ID already exists"));
		return OpStatus::ERR_PARSING_FAILED;
	}

	breakpoint_data.type = breakpoint_data.TYPE_EVENT;
	breakpoint_data.data.event.event_type = in.GetEventType().CStr();
	breakpoint_data.data.event.namespace_uri = NULL;
	breakpoint_data.window_id = in.GetWindowID();

	return AddBreakpoint(id, breakpoint_data);
}

OP_STATUS
ES_ScopeDebugFrontend::DoRemoveBreakpoint(const BreakpointID &in)
{
	unsigned int id = in.GetBreakpointID();

	if(id == 0)
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Breakpoint ID must be nonzero"));
		return OpStatus::ERR_PARSING_FAILED;
	}

	return RemoveBreakpoint(id);
}

OP_STATUS
ES_ScopeDebugFrontend::DoAddEventHandler(const EventHandler &in)
{
	if(in.GetHandlerID() == 0)
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Event handler ID must be nonzero"));
		return OpStatus::ERR_PARSING_FAILED;
	}

	ES_DebugEventHandlerData eventhandler_data;

	eventhandler_data.handler_id = in.GetHandlerID();
	eventhandler_data.object_id = in.GetObjectID();
	eventhandler_data.namespace_uri = in.GetNamespace().CStr();
	eventhandler_data.event_type = in.GetEventType().CStr();

	eventhandler_data.prevent_default = in.GetPreventDefaultHandler(); // Default is TRUE
	eventhandler_data.stop_propagation = in.GetStopPropagation(); // Default is TRUE
	eventhandler_data.phase = ES_PHASE_CAPTURING;

	return AddEventHandler(eventhandler_data);
}

OP_STATUS
ES_ScopeDebugFrontend::DoRemoveEventHandler(const EventHandlerID &in)
{
	unsigned handler_id = in.GetHandlerID();

	if(handler_id == 0)
	{
		SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Event handler ID must be nonzero"));
		return OpStatus::ERR_PARSING_FAILED;
	}

	return RemoveEventHandler(handler_id);
}

OP_STATUS
ES_ScopeDebugFrontend::DoSetConfiguration(const Configuration &in)
{
	if (in.HasStopAtScript())
		SetStopAt(STOP_TYPE_SCRIPT,    in.GetStopAtScript());
	if (in.HasStopAtException())
		SetStopAt(STOP_TYPE_EXCEPTION, in.GetStopAtException());
	if (in.HasStopAtError())
		SetStopAt(STOP_TYPE_ERROR,     in.GetStopAtError());
	if (in.HasStopAtAbort())
		SetStopAt(STOP_TYPE_ABORT,     in.GetStopAtAbort());
	if (in.HasStopAtDebuggerStatement())
		SetStopAt(STOP_TYPE_DEBUGGER_STATEMENT, in.GetStopAtDebuggerStatement());
	if (in.HasReformatScriptData())
		SetReformatFlag(in.GetReformatScriptData());
	if (in.HasReformatConditionFunction())
		SetReformatConditionFunction(in.GetReformatConditionFunction().CStr());
	if (in.HasStopAtCaughtError())
		SetStopAt(STOP_TYPE_HANDLED_ERROR, in.GetStopAtCaughtError());

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoGetBacktrace(const BacktraceSelection &in, BacktraceFrameList &out)
{
	unsigned int runtime_id = in.GetRuntimeID();
	unsigned int thread_id  = in.GetThreadID();
	unsigned int maxframes  = in.GetMaxFrames(); // Default is 0 which means infinite

	// Set backtrace.
	ES_DebugStackFrame *frames = NULL;
	unsigned length;

	OP_STATUS s = Backtrace(runtime_id, thread_id, maxframes, length, frames);

	if (OpStatus::IsSuccess(s))
		s = SetBacktraceList(out, length, frames);

	OP_DELETEA(frames);

	// Set return values.
	if (ES_DebugReturnValue *return_value = GetReturnValue(runtime_id, thread_id))
		s = SetBacktraceReturnValues(out, return_value);

	return s;
}

OP_STATUS
ES_ScopeDebugFrontend::DoBreak(const BreakSelection &in)
{
	unsigned int runtime_id = in.GetRuntimeID();
	unsigned int thread_id  = in.GetThreadID();

	return Break(runtime_id, thread_id);
}

OP_STATUS
ES_ScopeDebugFrontend::DoInspectDom(const DomTraversal &in, NodeList &out)
{
#ifdef __DOM_INSPECTOR
	unsigned int object_id = in.GetObjectID();
	const OpString &traversal = in.GetTraversal();

	ES_Object *es_object = GetObjectById(object_id);
	if (!es_object)
		return OpStatus::ERR;

	DOM_Object *node = DOM_Utils::GetDOM_Object(es_object);

	if (!node)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Object is not a DOM object"));

	node = DOM_Utils::GetProxiedObject(node);

	if (!node)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Proxied object could not be retrieved"));

	if (!DOM_Utils::IsA(node, DOM_TYPE_NODE))
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Object is not a DOM node"));

	TraversalType traversal_type;
	if (traversal.Compare(UNI_L("subtree")) == 0)                              traversal_type = TRAVERSAL_SUBTREE;
	else if (traversal.Compare(UNI_L("node")) == 0)                            traversal_type = TRAVERSAL_NODE;
	else if (traversal.Compare(UNI_L("children")) == 0)                        traversal_type = TRAVERSAL_CHILDREN;
	else if (traversal.Compare(UNI_L("parent-node-chain-with-children")) == 0) traversal_type = TRAVERSAL_PARENT_NODE_CHAIN_WITH_CHILDREN;
	else return OpStatus::ERR_PARSING_FAILED; // TODO: Need a way to set more detailed error messages/types

	OpProtobufMessageVector<NodeInfo> &nodes = out.GetNodeListRef();
	RETURN_IF_ERROR(GetDOMNodes(this, nodes, node, traversal_type));

#endif // __DOM_INSPECTOR
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoCssGetIndexMap(CssIndexMap &out)
{
#ifdef __GET_STYLE_RULES
	RETURN_IF_ERROR(GetCSSIndexMap(out));
#endif // __GET_STYLE_RULES
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoCssGetAllStylesheets(const RuntimeID &in, CssStylesheetList &out)
{
#ifdef __GET_STYLE_RULES
	unsigned int runtime_id = in.GetRuntimeID();
	ES_Runtime *runtime = GetRuntimeById(runtime_id);

	if (runtime == 0)
		return OpStatus::ERR;

	FramesDocument *current_doc = runtime->GetFramesDocument(); // REFACTOR: func getframesdoc(runtime_id)
	if (current_doc == 0)
		return OpStatus::ERR;

	DOM_Environment *environment = DOM_Utils::GetDOM_Environment(runtime);
	if (environment == 0)
		return OpStatus::ERR;

	HLDocProfile *hld_profile = current_doc->GetHLDocProfile();
	if (hld_profile == 0)
		return OpStatus::ERR;

	CSSCollection *collection = hld_profile->GetCSSCollection();
	if (collection == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(GetStylesheetList(out, current_doc, environment, collection));
#endif // __GET_STYLE_RULES
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoCssGetStylesheet(const CssStylesheetSelection &in, CssStylesheetRules &out)
{
#ifdef __GET_STYLE_RULES
	unsigned int runtime_id = in.GetRuntimeID();
	unsigned int object_id = in.GetStylesheetID();

	ES_Runtime *runtime   = GetRuntimeById(runtime_id);

	if (!runtime)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Runtime does not exist"));

	ES_Object  *es_object = GetObjectById(runtime, object_id);

	if (es_object == 0 || runtime == 0)
		return OpStatus::ERR;

	FramesDocument *current_doc = runtime->GetFramesDocument(); // REFACTOR: func getframesdoc(runtime_id)

	if (current_doc == 0)
		return OpStatus::ERR;

	DOM_Environment *environment = DOM_Utils::GetDOM_Environment(runtime);
	if (environment == 0)
		return OpStatus::ERR;

	DOM_Object *stylesheet = DOM_Utils::GetDOM_Object(es_object);

	if (!stylesheet)
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Object is not a DOM object"));

	DOM_Object *owner = NULL;

	OP_STATUS err = DOM_Utils::GetDOM_CSSStyleSheetOwner(stylesheet, owner);

	if (OpStatus::IsMemoryError(err))
		return err;
	else if (OpStatus::IsError(err))
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Object is not a stylesheet"));

	HTML_Element *html_elm = DOM_Utils::GetHTML_Element(owner);

	if (!html_elm)
		return OpStatus::ERR;

	CSS *css = html_elm->GetCSS();
	if (css == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(GetStylesheetRules(out, environment, css, object_id));
	return OpStatus::OK;
#else // __GET_STYLE_RULES
	return OpStatus::ERR_NOT_SUPPORTED;
#endif // __GET_STYLE_RULES
}

static BOOL
IsPseudoElementSupported(PseudoElementType type)
{
	switch (type)
	{
	case ELM_PSEUDO_FIRST_LINE:
	case ELM_PSEUDO_FIRST_LETTER:
	case ELM_PSEUDO_BEFORE:
	case ELM_PSEUDO_AFTER:
		return TRUE;
	default:
		return FALSE;
	}
}

OP_STATUS
ES_ScopeDebugFrontend::DoCssGetStyleDeclarations(const CssElementSelection &in, CssStyleDeclarations &out)
{
#ifdef __GET_STYLE_RULES
	int runtime_id = in.GetRuntimeID();
	int object_id = in.GetObjectID();

	ES_Runtime *runtime = GetRuntimeById(runtime_id);

	if (runtime == 0)
		return OpStatus::ERR;

	DOM_Environment *environment = DOM_Utils::GetDOM_Environment(runtime);

	if (environment == 0)
		return OpStatus::ERR;

	FramesDocument *current_doc = runtime->GetFramesDocument(); // REFACTOR: func getframesdoc(runtime_id)

	if (current_doc == 0)
		return OpStatus::ERR;

	ES_Object *es_object = GetObjectById(runtime, object_id);
	if (es_object == 0)
		return OpStatus::ERR;

	EcmaScript_Object *host_object = ES_Runtime::GetHostObject(es_object);

	if (!host_object || !host_object->IsA(ES_HOSTOBJECT_DOM))
		return OpStatus::ERR;

	HTML_Element *html_elm = DOM_Utils::GetHTML_Element((DOM_Object*)host_object); // REFACTOR: func gethtmlelm(object_id)

	if (!html_elm)
		return OpStatus::ERR;

	PseudoElementType pseudo = ELM_NOT_PSEUDO;

	if (in.HasPseudoElement())
	{
		pseudo = ConvertPseudoElement(in.GetPseudoElement());

		SCOPE_BADREQUEST_IF_FALSE(IsPseudoElementSupported(pseudo), this,
			UNI_L("Unsupported pseudo-selector"));
	}

	int pseudo_selectors = 0;

	for (unsigned i = 0; i < in.GetPseudoSelectorList().GetCount(); ++i)
	{
		PseudoSelector selector = static_cast<PseudoSelector>(in.GetPseudoSelectorList().Get(i));
		pseudo_selectors = (pseudo_selectors | Convert(selector));
	}

	CSS_MatchElement element(html_elm, pseudo);

	ScopeStyleListener listener(element, environment, pseudo_selectors, this);
	HLDocProfile *hld_profile = current_doc->GetHLDocProfile();
	if (hld_profile == 0)
		return OpStatus::ERR;

	CSSCollection *collection = hld_profile->GetCSSCollection();
	if (collection == 0)
		return OpStatus::ERR;

	CSS_Properties css_props;

#ifdef SVG_SUPPORT
	OpAutoVector<CSS_property_list> svg_rules;
	RETURN_IF_ERROR(GetSVGRules(html_elm, hld_profile, css_props, svg_rules, &listener));
#endif // SVG_SUPPORT

	OP_STATUS stat = collection->GetMatchingStyleRules(element,
													   &css_props,
													   current_doc->GetMediaType(),
													   TRUE,
													   &listener);

	SCOPE_BADREQUEST_IF_ERROR(stat, this, UNI_L("No such pseudo-element"));
	RETURN_IF_ERROR(listener.AdjustDeclarations(css_props));

	CSS_DOMStyleDeclaration* computed_style_decl_ptr;
	RETURN_IF_ERROR(html_elm->DOMGetComputedStyle(computed_style_decl_ptr, environment, NULL));
	OpAutoPtr<CSS_DOMStyleDeclaration> computed_style_decl(computed_style_decl_ptr);

	// Start COMPUTED-STYLE
	RETURN_IF_ERROR(GetComputedStyle(out.GetComputedStyleListRef(), computed_style_decl.get()));
	// End COMPUTED-STYLE

	listener.ProcessNodes(out.GetNodeStyleListRef());
#endif // __GET_STYLE_RULES
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::DoGetSelectedObject(ObjectSelection &out)
{
	BOOL has_selection;
	return SetSelectedObjectInfo(out, has_selection);
}

OP_STATUS
ES_ScopeDebugFrontend::ExportObjectValue(ES_Runtime* runtime, ObjectValue &out, ES_Object* in)
{
	ES_DebugObject dbg_obj;
	ExportObject(runtime, in, dbg_obj, FALSE);
	OP_STATUS status = SetObjectValue(out, dbg_obj);
	OP_DELETE(dbg_obj.info); // Does not happen automatically.
	return status;
}

OP_STATUS
ES_ScopeDebugFrontend::SetValue(Value& out, const ES_DebugValue& value)
{
	switch (value.type)
	{
	case VALUE_UNDEFINED:
		out.SetType(ES_ScopeDebugFrontend_SI::Value::TYPE_UNDEFINED);
		break;
	case VALUE_NULL:
		out.SetType(ES_ScopeDebugFrontend_SI::Value::TYPE_NULL);
		break;
	case VALUE_BOOLEAN:
		out.SetType(value.value.boolean ? ES_ScopeDebugFrontend_SI::Value::TYPE_TRUE : ES_ScopeDebugFrontend_SI::Value::TYPE_FALSE);
		break;
	case VALUE_NUMBER:
		// NaN and +-Infinite needs to be set using enums as JSON does not support them
		if (op_isnan(value.value.number))
			out.SetType(ES_ScopeDebugFrontend_SI::Value::TYPE_NAN);
		else if (op_isinf(value.value.number))
			out.SetType(op_signbit(value.value.number) ? ES_ScopeDebugFrontend_SI::Value::TYPE_MINUS_INFINITY : ES_ScopeDebugFrontend_SI::Value::TYPE_PLUS_INFINITY);
		else
		{
			out.SetType(ES_ScopeDebugFrontend_SI::Value::TYPE_NUMBER);
			out.SetNumber(value.value.number);
		}
		break;
	case VALUE_STRING_WITH_LENGTH:
	case VALUE_STRING:
		out.SetType(ES_ScopeDebugFrontend_SI::Value::TYPE_STRING);
		RETURN_IF_ERROR(value.GetString(out.GetStrRef()));
		break;
	case VALUE_OBJECT:
	{
		out.SetType(ES_ScopeDebugFrontend_SI::Value::TYPE_OBJECT);
		OpAutoPtr<ObjectValue> object_value(OP_NEW(ObjectValue, ()));
		RETURN_OOM_IF_NULL(object_value.get());
		RETURN_IF_ERROR(SetObjectValue(*object_value.get(), value.value.object));
		out.SetObject(object_value.release());
		return OpStatus::OK;
	}
	default:
		OP_ASSERT(!"Please add a case for the new type");
	}

	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::SetEventListenerInfo(EventListener &out, const DOM_EventListener &listener, ES_Runtime *runtime)
{
	ES_Object *native_listener = listener.GetNativeHandler();
	const uni_char *native_code = listener.GetNativeCode();
	OP_ASSERT(native_listener || native_code);

	if (listener.HasCallerInformation())
	{
		unsigned script_guid = listener.GetCallerScriptGuid();
		unsigned line_no = listener.GetCallerLineNo();
		if (script_guid  != ~0u && line_no != ~0u)
		{
			Position *position= OP_NEW(Position, (script_guid, line_no));
			RETURN_OOM_IF_NULL(position);
			out.SetPosition(position);
		}
	}

	DOM_EventType event_type = listener.GetKnownType();
	if (event_type == DOM_EVENT_CUSTOM)
		RETURN_IF_ERROR(out.SetEventType(listener.GetCustomEventType()));
	else
		RETURN_IF_ERROR(out.SetEventType(DOM_EventsAPI::GetEventTypeString(event_type)));

	out.SetUseCapture(listener.IsCapture());

	if (listener.IsFromAttribute())
		out.SetOrigin(EventListener::ORIGIN_ATTRIBUTE);
	else
		out.SetOrigin(EventListener::ORIGIN_EVENT_TARGET);

	if (native_listener)
		out.SetListenerObjectID(GetObjectId(runtime, native_listener));
	if (native_code)
		out.SetListenerScriptData(native_code);
	return OpStatus::OK;
}

OP_STATUS
ES_ScopeDebugFrontend::AddClassMask(const PropertyFilter::ClassMask &classmask, ES_ScopePropertyFilters &filters)
{
	// Convert to 8-bit.
	OpString8 classname;
	RETURN_IF_ERROR(classname.Set(classmask.GetClassName()));

	ES_ScopePropertyFilters::Filter *filter;

	OP_STATUS status = filters.AddPropertyFilter(classname.CStr(), filter);

	SCOPE_BADREQUEST_IF_ERROR1(status, this,
		UNI_L("Class mask '%s' already present"),
		classmask.GetClassName().CStr());

	unsigned count = classmask.GetPropertyMaskList().GetCount();

	for (unsigned i = 0; i < count; ++i)
	{
		PropertyFilter::ClassMask::PropertyMask *prop_mask = classmask.GetPropertyMaskList().Get(i);
		RETURN_IF_ERROR(AddPropertyMask(*prop_mask, *filter));
	}

	return OpStatus::OK;
}

static const uni_char *
ConvertPropertyTypeToString(unsigned type)
{
	switch (type)
	{
	case 3: // boolean
		return UNI_L("Boolean");
	case 4: // number
		return UNI_L("Number");
	case 5: // string
		return UNI_L("String");
	default:
		return UNI_L("Unknown");
	}
}

OP_STATUS
ES_ScopeDebugFrontend::AddPropertyMask(const PropertyFilter::ClassMask::PropertyMask &mask, ES_ScopePropertyFilters::Filter &filter)
{
	const uni_char *key = mask.GetName().CStr();

	// First, validate input.
	BOOL valid = TRUE;

	switch (mask.GetType())
	{
	case 3: // boolean
		valid = mask.HasBoolean();
		break;
	case 4: // number
		valid = mask.HasNumber();
		break;
	case 5: // string
		valid = mask.HasString();
		break;
	}

	if (!valid)
	{
		SCOPE_BADREQUEST_IF_ERROR2(OpStatus::ERR, this,
			UNI_L("%s mask '%s' has no value set"),
			ConvertPropertyTypeToString(mask.GetType()), key);
	}

	OP_STATUS status = OpStatus::OK;

	if (mask.HasType())
	{
		switch (mask.GetType())
		{
		case 1: // null
			status = filter.Add(key, VALUE_NULL);
			break;
		case 2: // undefined
			status = filter.Add(key, VALUE_UNDEFINED);
			break;
		case 3: // boolean
			status = filter.Add(key, mask.GetBoolean());
			break;
		case 4: // number
			status = filter.Add(key, static_cast<double>(mask.GetNumber()));
			break;
		case 5: // string
			status = filter.Add(key, mask.GetString().CStr());
			break;
		case 6: // object
			status = filter.Add(key, VALUE_OBJECT);
			break;
		default:
			SCOPE_BADREQUEST(this, UNI_L("Unknown type"));
		}
	}
	else
		status = filter.Add(key);

	SCOPE_BADREQUEST_IF_ERROR1(status, this,
		UNI_L("Property mask '%s' already present"), key);

	return OpStatus::OK;
}

DOM_Utils::SearchType
ES_ScopeDebugFrontend::Convert(SearchDomArg::Type type)
{
	switch (type)
	{
	default:
		OP_ASSERT(!"Please add a case for the new type");
	case SearchDomArg::TYPE_PLAIN_TEXT:
		return DOM_Utils::SEARCH_PLAINTEXT;
	case SearchDomArg::TYPE_REGEXP:
		return DOM_Utils::SEARCH_REGEXP;
	case SearchDomArg::TYPE_XPATH:
		return DOM_Utils::SEARCH_XPATH;
	case SearchDomArg::TYPE_SELECTOR:
		return DOM_Utils::SEARCH_SELECTOR;
	case SearchDomArg::TYPE_EVENT:
		return DOM_Utils::SEARCH_EVENT;
	}
}
#endif // SCOPE_ECMASCRIPT_DEBUGGER
