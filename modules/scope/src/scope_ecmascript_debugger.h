/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jens Lindstrom / Lars T Hansen
**
*/

#ifndef SCOPE_ECMASCRIPT_DEBUGGER_H
#define SCOPE_ECMASCRIPT_DEBUGGER_H

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

#include "modules/ecmascript_utils/esdebugger.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/scope_jsconsole_listener.h"
#include "modules/scope/scope_readystate_listener.h"
#include "modules/scope/scope_element_inspector.h"
#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/scope_property_filter.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/style/css_collection.h"
#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esobjman.h"
#include "modules/dom/domutils.h"

#include "modules/scope/src/generated/g_scope_ecmascript_debugger_interface.h"

class HTML_Element;
class DOM_Environment;
class DOM_Object;
class DOM_EventListener;
class ES_Object;
class CSS_DOMRule;
class CSS_DOMStyleDeclaration;
class CSS_property_list;
class CSS_Rule;
class CSSCollection;
class CSS_decl;
class CSS;
class ES_ScopePropertyFilters;
class FramesDocument;
class OpScopeStyleListener;

class ES_DebugEventHandlerData
{
public:
	unsigned object_id;
	unsigned handler_id;
	const uni_char *namespace_uri;
	const uni_char *event_type;
	BOOL prevent_default;
	BOOL stop_propagation;
	ES_EventPhase phase;
};

class ES_ScopeDebugFrontend
	: public ES_DebugFrontend
	, public ES_ScopeDebugFrontend_SI
{
public:
	ES_ScopeDebugFrontend();
	virtual ~ES_ScopeDebugFrontend();

	OP_STATUS Construct();

	virtual OP_STATUS OnPostInitialization();

	virtual OP_STATUS RuntimeStarted(unsigned dbg_runtime_id, const ES_DebugRuntimeInformation* runtimeinfo);
	virtual OP_STATUS RuntimeStopped(unsigned dbg_runtime_id, const ES_DebugRuntimeInformation* runtimeinfo);
	virtual OP_STATUS RuntimesReply(unsigned tag, OpVector<ES_DebugRuntimeInformation>& runtimes);
	virtual OP_STATUS NewScript(unsigned dbg_runtime_id, ES_DebugScriptData *data, const ES_DebugRuntimeInformation *runtimeinfo);
	virtual OP_STATUS ThreadStarted(unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo);
	virtual OP_STATUS ThreadFinished(unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus status, ES_DebugReturnValue *dbg_return_value);
	virtual OP_STATUS ThreadMigrated(unsigned dbg_thread_id, unsigned dbg_from_runtime_id, unsigned dbg_to_runtime_id);
	virtual OP_STATUS StoppedAt(unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data);
	virtual OP_STATUS EvalReply(unsigned tag, EvalStatus status, const ES_DebugValue &result);
	virtual OP_STATUS ExamineObjectsReply(unsigned tag, unsigned object_count, ES_DebugObjectChain *result);
	virtual OP_STATUS FunctionCallStarting(unsigned dbg_runtime_id, unsigned threadid, ES_Value* arguments, int argc, ES_Object* thisval, ES_Object* function, const ES_DebugPosition& position);
	virtual OP_STATUS FunctionCallCompleted(unsigned dbg_runtime_id, unsigned threadid, const ES_Value &returnvalue, BOOL exception_thrown);

	virtual OP_STATUS EvalError(unsigned tag, const uni_char *message, unsigned line, unsigned offset);
	OP_STATUS HandleEvent(const uni_char* event_name, unsigned target_id, unsigned handler_id);
	OP_STATUS HandleEvent(const uni_char* event_name, ES_Runtime *runtime, ES_Object* target, unsigned handler_id);
	virtual OP_STATUS ParseError(unsigned dbg_runtime_id, ES_ErrorInfo *err);

	/* OpScopeReadyStateListener */
	void ReadyStateChanged(FramesDocument *doc, OpScopeReadyStateListener::ReadyState state);

	/* OpScopeJSConsoleListener */
	void ConsoleLog(ES_Runtime *runtime, OpScopeJSConsoleListener::LogType type, ES_Value* argv, int argc);
	void ConsoleTrace(ES_Runtime *runtime);
	void ConsoleTime(ES_Runtime *runtime, const uni_char *title);
	void ConsoleTimeEnd(ES_Runtime *runtime, const uni_char *title);
	void ConsoleProfile(ES_Runtime *runtime, const uni_char *title);
	void ConsoleProfileEnd(ES_Runtime *runtime);
	void ConsoleException(ES_Runtime *runtime, ES_Value* argv, int argc);


	/* OpScopeElementInspector */
	OP_STATUS InspectElement(FramesDocument* frm_doc, HTML_Element* html_elm);

	virtual OP_STATUS Connected();
	virtual OP_STATUS Closed();

	// OpScopeService
	virtual OP_STATUS OnServiceEnabled();
	virtual OP_STATUS OnServiceDisabled();
	virtual void FilterChanged();

	OP_STATUS Detach(); //< Detach ecmascript debugger

	unsigned int GetStylesheetID(DOM_Object* dom_obj, DOM_Object* import_rule);
	/**< Returns the id for the stylesheet which @a dom_obj refers to.
	     @a dom_obj must either be a LINK or STYLE element.

		 @param dom_obj The stylesheet object.
		 @param import_rule The import rule object if this is an @import'ed
		        stylesheet. NULL otherwise.

	     @return id of stylesheet or 0 on failure
	*/
	unsigned GetHTMLObjectID(DOM_Environment* env, HTML_Element* elm);

private:
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OpScopeWindowManager *window_manager;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT


	// FIXME Temporary functions for CSS inspector

/*	static int CompareRuleEntry(const void *a, const void *b); ///< Compares two RuleEntry objects according to specificity and flow order*/
	static OP_STATUS GetDOMRule(CSS_DOMRule*& dom_rule, CSS_Rule* rule, HTML_Element* sheet_elm, DOM_Environment* dom_env);
    /**<
	   Helper function for creating/fetching the CSS_DOMRule object for a given CSS_Rule object.

	   @todo This function most likely belongs in the style module and not in scope.
	*/

	/**
	 * Get the associated ES_Runtime for the specified DOM_Object.
	 *
	 * @param dom_obj The object to to get the ES_Runtime for.
	 * @return A runtime, or NULL if no runtime was associated.
	 */
	ES_Runtime *GetESRuntime(DOM_Object *dom_obj);

	CSSPseudoClassType Convert(PseudoSelector in);

#ifdef SVG_SUPPORT
	/**
	 * Get the equivalent CSS rules for the presentational attribute on a SVG
	 * element (and its parent SVG elements).
	 *
	 * @param [in] elm The SVG element to get styles for.
	 * @param [in] hld_profile The HLDocProfile for the document that contains the element.
	 * @param [out] props The list of used CSS properties (for override resolution).
	 * @param [out] rules Contains the list of rules. This must be allocated on the outside
	 *              to ensure the continued existence of the CSS_decl objects allocated by
	 *              this function.
	 * @param listener The listener to notify when a rule is encountered.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetSVGRules(HTML_Element *elm, HLDocProfile *hld_profile, CSS_Properties &props, OpAutoVector<CSS_property_list> &rules, OpScopeStyleListener *listener);
#endif // SVG_SUPPORT

	// Event handlers:
	OP_STATUS AddEventHandler(const ES_DebugEventHandlerData &data);
	OP_STATUS RemoveEventHandler(unsigned handler_id);
	Head dbg_eventhandlers;

	FramesDocument* current_spotligheted_doc;   //< The frames document that has the current global spotlight.
	OP_STATUS SetSpotlight(unsigned object_id, BOOL scroll_into_view); //< Spotlight the given object. Only one spotlight per session. Set object_id to 0 to remove the spotlight.
	OP_STATUS ClearCurrentSpotlight(); //< Clear the current spotlight

	/// Set inspect_object to NULL, and clean up references
	void UnreferenceInspectObject();

	ES_Object* inspect_object;  /// Pointer to the ES_Object that was selected. Protected from garbage collection. Never non-NULL if inspect_object_id is != 0
	ES_Runtime* inspect_runtime; /// Runtime for the @a inspect_object variable, only set if @c inspect_object is set.
	unsigned inspect_object_id; /// Object id of selected object. Never != 0 if inspect_object is non-NULL
	unsigned inspect_window_id;

	ES_ScopePropertyFilters *property_filters;
	OpStringHashTable<double> console_timers;

	OP_STATUS ExportToValue(ES_Runtime* runtime, const ES_Value &value, ES_ScopeDebugFrontend_SI::Value &out);

	OP_STATUS ExportObjectValue(ES_Runtime* runtime, ObjectValue &out, ES_Object* in);

	const uni_char *GetScriptTypeName(ES_ScriptType type) const; /// Convert script type enumerator into a string name for transport.

public:
	// Request/Response functions
	virtual OP_STATUS DoListRuntimes(const RuntimeSelection &in, RuntimeList &out);
	virtual OP_STATUS DoContinueThread(const ThreadMode &in);
	virtual OP_STATUS DoSpotlightObject(const SpotlightObjectSelection &in);
	virtual OP_STATUS DoAddBreakpoint(const Breakpoint &in);
	virtual OP_STATUS DoRemoveBreakpoint(const BreakpointID &in);
	virtual OP_STATUS DoAddEventHandler(const EventHandler &in);
	virtual OP_STATUS DoRemoveEventHandler(const EventHandlerID &in);
	virtual OP_STATUS DoSetConfiguration(const Configuration &in);
	virtual OP_STATUS DoGetBacktrace(const BacktraceSelection &in, BacktraceFrameList &out);
	virtual OP_STATUS DoBreak(const BreakSelection &in);
	virtual OP_STATUS DoInspectDom(const DomTraversal &in, NodeList &out);
	virtual OP_STATUS DoCssGetIndexMap(CssIndexMap &out);
	virtual OP_STATUS DoCssGetAllStylesheets(const RuntimeID &in, CssStylesheetList &out);
	virtual OP_STATUS DoCssGetStylesheet(const CssStylesheetSelection &in, CssStylesheetRules &out);
	virtual OP_STATUS DoCssGetStyleDeclarations(const CssElementSelection &in, CssStyleDeclarations &out);
	virtual OP_STATUS DoGetSelectedObject(ObjectSelection &out);
	virtual OP_STATUS DoSpotlightObjects(const SpotlightSelection &in);
	virtual OP_STATUS DoReleaseObjects(const ReleaseObjectsArg &in);
	virtual OP_STATUS DoSetPropertyFilter(const SetPropertyFilterArg &in);
	virtual OP_STATUS DoAddEventBreakpoint(const EventBreakpoint &in);
	virtual OP_STATUS DoSetFunctionFilter(const FunctionFilter &in);
	virtual OP_STATUS DoSearchDom(const SearchDomArg &in, NodeList &out);
	virtual OP_STATUS DoEval(const EvalData &in, unsigned int async_tag);
	virtual OP_STATUS DoExamineObjects(const ExamineList &in, unsigned int async_tag);
	virtual OP_STATUS DoGetFunctionPositions(const GetFunctionPositionsArg &in, FunctionPositionList &out);
	virtual OP_STATUS DoGetEventNames(const GetEventNamesArg &in, EventNames &out);
	virtual OP_STATUS DoGetEventListeners(const GetEventListenersArg &in, EventTargets &out);

	// Helper methods
	OP_STATUS GetCSSIndexMap(CssIndexMap &map); ///< Generates and index map of CSS styles
	OP_STATUS GetStylesheetList(CssStylesheetList &list, FramesDocument *doc, DOM_Environment *environment, CSSCollection* collection); ///< Generates a list of STYLESHEET entries
	OP_STATUS GetStylesheet(CssStylesheetList::Stylesheet &stylesheet, FramesDocument *doc, DOM_Environment *environment, CSS* css); ///< Generates a STYLESHEET entry
	OP_STATUS GetPropertyList(CssStylesheetRules::StylesheetRule &rule, DOM_Environment *environment, HTML_Element* elm, CSS_property_list* style); ///< Generates a CSS_property_list as a comma separated JSON list or XML structure
	OP_STATUS GetStylesheetRules(CssStylesheetRules &rules, DOM_Environment *environment, CSS* css, unsigned int object_id);
	OP_STATUS GetCSSRule(CssStylesheetRules::StylesheetRule &out, DOM_Environment *environment, CSS* css, unsigned int object_id, CSS_Rule* rule);
	OP_STATUS GetComputedStyle(OpVector<OpString> &list, CSS_DOMStyleDeclaration* style);

	OP_STATUS SetRuntimeInformation(RuntimeInfo &info, const ES_DebugRuntimeInformation *runtimeinfo);
	OP_STATUS SetThreadStartedInfo(ThreadInfo &info, unsigned dbg_runtime_id, unsigned dbg_thread_id, unsigned dbg_parent_thread_id, ThreadType type, const uni_char *namespace_uri, const uni_char *event_type, const ES_DebugRuntimeInformation *runtimeinfo);
	OP_STATUS SetThreadFinishedResult(ThreadResult &result, unsigned dbg_runtime_id, unsigned dbg_thread_id, ThreadStatus status, ES_DebugReturnValue *dbg_return_value);
	OP_STATUS SetThreadStoppedAtInfo(ThreadStopInfo &info, unsigned dbg_runtime_id, unsigned dbg_thread_id, const ES_DebugStoppedAtData &data);
	OP_STATUS SetBacktraceFrame(BacktraceFrame &frame, ES_DebugStackFrame &stack);
	OP_STATUS SetBacktraceList(BacktraceFrameList &list, unsigned length, ES_DebugStackFrame *stack);
	OP_STATUS SetBacktraceReturnValues(BacktraceFrameList &list, const ES_DebugReturnValue *value);
	OP_STATUS SetReturnValue(ReturnValue &out, const ES_DebugReturnValue &in);
	OP_STATUS SetDomEventInfo(DomEvent &info, const uni_char* event_name, unsigned target_id, unsigned handler_id);
	OP_STATUS SetEvalResult(EvalResult &eval, unsigned tag, EvalStatus status, const ES_DebugValue &result);
	OP_STATUS GetESObjectValue(OpString &type, OpString &value, OpAutoPtr<ObjectValue> &object_value, const ES_DebugValue &dvalue);
	OP_STATUS SetConsoleLogInfo(ConsoleLogInfo &msg, unsigned values_count, ES_DebugValue *values);
	OP_STATUS SetObjectList(ObjectList &list, ES_DebugObjectChain &chain);
	OP_STATUS SetObjectInfo(ObjectInfo &info, const ES_DebugObjectProperties &prop);
	OP_STATUS SetObjectChainList(ObjectChainList &list, unsigned tag, unsigned chains_count, ES_DebugObjectChain *chains);
	OP_STATUS SetPropertyInfo(ObjectInfo::Property &info, ES_DebugObjectProperties::Property &p);
	OP_STATUS SetSelectedObjectInfo(ObjectSelection &selection, BOOL &has_selection);
	OP_STATUS SetParseError(DomParseError &msg, unsigned dbg_runtime_id, ES_ErrorInfo *err);
	OP_STATUS SetPosition(Position &msg, const ES_DebugPosition &position);
	OP_STATUS SetConsoleTimeInfo(ConsoleTimeInfo &msg, unsigned dbg_runtime_id, const uni_char *title);
	OP_STATUS SetConsoleTimeEndInfo(ConsoleTimeEndInfo &msg, unsigned dbg_runtime_id, const uni_char *title, unsigned elapsed);
	OP_STATUS SetConsoleTraceInfo(ConsoleTraceInfo &msg, ES_Runtime *runtime, unsigned length, ES_DebugStackFrame *stack);
	OP_STATUS SetConsoleProfileEndInfo(ConsoleProfileEndInfo &msg, unsigned dbg_runtime_id);
	OP_STATUS SetFunctionCallArguments(ES_Runtime* runtime, FunctionCallStartingInfo &msg, unsigned values_count, ES_Value* ext_values);
	OP_STATUS SetValue(Value &out, const ES_DebugValue &value);
	/**
	 * Export information on the event listener to the protobuf EventListener message.
	 *
	 * @param out The protobuf message which receives the information.
	 * @param listener The listener which is exported.
	 * @param runtime The runtime which the listener belongs to, used to create object IDs.
	 * @return OpStatus::OK if the export succeeded or OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS SetEventListenerInfo(EventListener &out, const DOM_EventListener &listener, ES_Runtime *runtime);

	RuntimeList        *runtime_list; ///< Pointer to the current RuntimeList object, will only be set when DoListRuntimes() is active.
private:
	OP_STATUS AddClassMask(const PropertyFilter::ClassMask &classmask, ES_ScopePropertyFilters &filters);
	OP_STATUS AddPropertyMask(const PropertyFilter::ClassMask::PropertyMask &mask, ES_ScopePropertyFilters::Filter &filter);
	DOM_Utils::SearchType Convert(SearchDomArg::Type type);
};

// FIXME: Add API doc for OpScopeNodeMatch and subclasses
class OpScopeNodeMatch : public Link
{
public:
	class Property : public Link
	{
	public:
		Property(int index, BOOL important, CSS_decl *decl);
		virtual ~Property();
		OP_STATUS Construct(const OpString &val);
		OP_STATUS Construct(const uni_char *val);

		int Index() const { return index; }
		const OpString &Value() const { return value; }
		BOOL IsImportant() const { return important; }
		ES_ScopeDebugFrontend::DeclarationStatus Status() const { return status; }
		CSS_decl *CSSDeclaration() const { return declaration; }
		void SetIndex(int idx) { index = idx; }
		OP_STATUS SetValue(const OpString &v) { return value.Set(v); }
		OP_STATUS SetValue(const uni_char *v) { return value.Set(v); }
		void SetIsImportant(BOOL im) { important = im; }
		void SetDeclarationStatus(ES_ScopeDebugFrontend::DeclarationStatus s) { status = s; }
		void SetCSSDeclaration(CSS_decl *decl) { declaration = decl; }

#ifdef DOM_FULLSCREEN_MODE
		BOOL IsFullscreen() const { return declaration && declaration->GetOrigin() == CSS_decl::ORIGIN_USER_AGENT_FULLSCREEN; }
#endif // DOM_FULLSCREEN_MODE

		Property *Suc() const { return static_cast<Property *>(Link::Suc()); }
		Property *Pred() const { return static_cast<Property *>(Link::Pred()); }

	private:
		int            index;
		OpString       value;
		BOOL           important;
		ES_ScopeDebugFrontend::DeclarationStatus status;
		CSS_decl*      declaration;
	};

	class Rule : public Link
	{
	public:
		Rule(ES_ScopeDebugFrontend::RuleOrigin origin, int flow_order);
		virtual ~Rule();

		ES_ScopeDebugFrontend::RuleOrigin Origin() const { return origin; }
		Property *FirstProperty() const { return static_cast<Property *>(properties.First()); }
		void AddProperty(Property *prop) { prop->Into(&properties); }
		BOOL HasProperties() const { return properties.Cardinal() > 0; }
		INT  PropertyCount() const { return properties.Cardinal(); }
		int FlowOrder() const { return flow_order; }

		unsigned StylesheetID() const { return stylesheet_id; }
		unsigned RuleID() const { return rule_id; }
		int RuleType() const { return rule_type; }
		void SetStylesheetID(unsigned id) { stylesheet_id = id; }
		void SetRuleID(unsigned id) { rule_id = id; }
		void SetRuleType(int type) { rule_type = type; }
#ifdef SCOPE_CSS_RULE_ORIGIN
		void SetLineNumber(unsigned line_number) { this->line_number = line_number; }
#endif // SCOPE_CSS_RULE_ORIGIN

		unsigned short SelectorSpecificity() const { return selector_specificity; }
		const OpString &SelectorText() const { return selector_text; }
		OP_STATUS SetSelector(unsigned short specificity, const OpString &text)
		{
			selector_specificity = specificity;
			return selector_text.Set(text);
		}
		OP_STATUS SetSelector(unsigned short specificity, const uni_char *text)
		{
			selector_specificity = specificity;
			return selector_text.Set(text);
		}

		Rule *Suc() const { return static_cast<Rule *>(Link::Suc()); }
		Rule *Pred() const { return static_cast<Rule *>(Link::Pred()); }

		OP_STATUS Process(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle::StyleDeclaration &decl) const;
		OP_STATUS ProcessProperties(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle::StyleDeclaration &decl) const;

		static int Compare(const Rule &a, const Rule &b);

	private:
		ES_ScopeDebugFrontend::RuleOrigin origin;
		Head       properties; // List of Property objects
		int        flow_order;

		// For author origin
		unsigned stylesheet_id;
		unsigned rule_id;
		int rule_type;
#ifdef SCOPE_CSS_RULE_ORIGIN
		unsigned line_number;
#endif // SCOPE_CSS_RULE_ORIGIN

		// For author and local origin
		unsigned short selector_specificity;
		OpString selector_text;
	};

	OpScopeNodeMatch(int object_id, const CSS_MatchElement &elm, const uni_char *elm_name);
	virtual ~OpScopeNodeMatch();

	int ObjectID() const { return object_id; }
	const uni_char *ElementName() const { return element_name; }
	const CSS_MatchElement &GetElement() const { return element; }


	Rule *FirstRule() const { return static_cast<Rule *>(declarations.First()); }
	void AddRule(Rule *rule) { rule->Into(&declarations); needs_sorting = TRUE; }
	const Rule * const *SortedRules(int &count) const;

	OpScopeNodeMatch *Suc() const { return static_cast<OpScopeNodeMatch *>(Link::Suc()); }
	OpScopeNodeMatch *Pred() const { return static_cast<OpScopeNodeMatch *>(Link::Pred()); }

	OP_STATUS Process(ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle &style) const;

private:
	OP_STATUS SortRules() const; // Prepares a sorted version of the rules

	int             object_id;
	CSS_MatchElement element;
	const uni_char* element_name;
	Head            declarations; // Rule entries
	mutable Rule**  sorted_declarations; // Is NULL until they are sorted
	mutable BOOL    needs_sorting; // Is set to TRUE when new elements are added to list to enforce sorting.
};

class OpScopeStyleListener : public CSS_MatchRuleListener
{
public:
	OpScopeStyleListener(const CSS_MatchElement& elm, DOM_Environment*env, int pseudo_selectors);
	~OpScopeStyleListener();
	OP_STATUS LocalRuleMatched(const CSS_MatchElement& elm, CSS_DOMRule* rule, unsigned short specificity);
	OP_STATUS RuleMatched(const CSS_MatchElement& elm, CSS_DOMRule* rule, unsigned short specificity);
	OP_STATUS InlineStyle(const CSS_MatchElement& elm, CSS_property_list* style);
	OP_STATUS DefaultStyle(const CSS_MatchElement& elm, CSS_property_list* style);
	BOOL MatchPseudo(int pseudo);

#ifdef SVG_SUPPORT
	/**
	 * Call this when the CSS style equivalent of SVG presentational
	 * attributes is encountered.
	 *
	 * @param element The HTML element the style applies to.
	 * @param style The style for the element.
	 * @return OpStatus::OK, or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS SVGRule(HTML_Element* element, CSS_property_list* style);
#endif // SVG_SUPPORT

	OP_STATUS NewRule(OpScopeNodeMatch::Rule*& match_rule, const CSS_MatchElement& elm, CSS_decl* css_decl, ES_ScopeDebugFrontend::RuleOrigin origin);
	OpScopeNodeMatch *FirstMatch() const { return static_cast<OpScopeNodeMatch *>(node_matches.First()); }
	OP_STATUS AdjustDeclarations(CSS_Properties &props);

	int NextFlowOrder();
	virtual int GetObjectID(HTML_Element* elm) = 0; // convenience function, calls GetObjectID(DOM_Object*)
	virtual int GetObjectID(DOM_Object* dom_obj) = 0;
	virtual unsigned int GetStylesheetID(DOM_Object* dom_obj) = 0;

	OP_STATUS ProcessNodes(OpProtobufMessageVector<ES_ScopeDebugFrontend::CssStyleDeclarations::NodeStyle> &styles) const;
	// TODO: Put FormatNodes in ifdefs when selftest is updated with latest CVS

	DOM_Environment* Environment() const { return env; }
	void SetEnvironment(DOM_Environment *e) { env = e; }
	void SetInspectedElement(HTML_Element *element) { inspect_elm = CSS_MatchElement(element); }

private:
	CSS_MatchElement inspect_elm;
	int flow_order;

	Head              node_matches; // OpScopeNodeMatch objects
	DOM_Environment*  env;
	int pseudo_selectors;
	OpScopeNodeMatch* current_node;
};

#endif // SCOPE_ECMASCRIPT_DEBUGGER
#endif // SCOPE_ECMASCRIPT_DEBUGGER_H
