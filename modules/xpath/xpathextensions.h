/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/* Note: You can just include modules/xpath/xpath.h, it pulls in the entire
   XPath API. */

#ifdef XPATH_EXTENSION_SUPPORT
#include "modules/xpath/xpath.h"

class XPathFunction;
class XPathVariable;

#ifdef XPATH_INCLUDE_XPATHEXTENSIONS

class XPathExtensions
{
public:
	virtual OP_STATUS GetFunction(XPathFunction *&function, const XMLExpandedName &name) = 0;
	/**< Retrieve a function by name.  If no such function is known,
	     OpStatus::ERR should be returned.  This will cause a compilation
	     error.

	     @param function Should be set to an XPathFunction implementation on
	                     success.
	     @param name Function name.

	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS GetVariable(XPathVariable *&variable, const XMLExpandedName &name) = 0;
	/**< Retrieve a variable by name.  If no such variable is known,
	     OpStatus::ERR should be returned.  This will cause a compilation
	     error.

	     @param variable Should be set to an XPathVariable implementation on
	                     success.
	     @param name Variable name.

	     @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY. */

	/**< Context for extension functions and variables.  Optional and provided
	     by the calling code when evaluating an expression or matching a
	     pattern.  Will be passed to any called extension function or
	     referenced extension variable by the XPath engine. */
	class Context
	{
		/* Class intentionally left blank. */
	};

protected:
	virtual ~XPathExtensions() {}
	/**< Destructor.  Objects are owned by the implementor and never deleted
	     through this interface. */
};

#endif // XPATH_INCLUDE_XPATHEXTENSIONS


#ifdef XPATH_INCLUDE_XPATHVALUE

class XPathValue
{
public:
	virtual void SetNumber(double value) = 0;
	/**< Sets the type of this value to 'number' and sets its value to
	     'value'. */

	virtual void SetBoolean(BOOL value) = 0;
	/**< Sets the type of this value to 'boolean' and sets its value to
	     'value'. */

	virtual OP_STATUS SetString(const uni_char *value, unsigned value_length = ~0u) = 0;
	/**< Sets the type of this value to 'string' and sets its value to
	     'value'.  The string is copied.  If 'value_length' is not '~0u' the
	     string must be null-terminated, otherwise only the 'value_length'
	     first characters of 'value' is copied.  In the latter case, 'value'
	     must contain at least 'value_length' non-null characters.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	virtual OP_STATUS SetNodeSet(BOOL ordered, BOOL duplicates) = 0;
	/**< Sets the type of this value to 'node-set'.  Nodes can be added using
	     the AddNode() function.  This function should only be called once per
	     value; calling it again resets the value (starts another node-set.)

	     If 'ordered' is TRUE, nodes must be added in document order for
	     correct result (unless zero or one nodes will be added, in which case
	     the 'ordered' argument doesn't matter.)  If 'ordered' is FALSE, nodes
	     can be added in any order, and the node-set will be sorted if
	     required.  Note: sorting is expensive, so if possible, add nodes in
	     document order, and set 'ordered' to TRUE.

	     If 'duplicates' is TRUE, the same node can safely be added several
	     times in different calls to AddNode().  If 'duplicates' is FALSE,
	     each node can only be added once for correct results.  Storage and
	     additions are more efficient if 'duplicates' is FALSE, so if it is
	     known that no node will be added more than once, set 'duplicates' to
	     FALSE.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	enum AddNodeStatus
	{
		ADDNODE_CONTINUE,
		/**< More nodes should be added immediately if known. */

		ADDNODE_PAUSE,
		/**< It'd make sense to pause now and process the nodes that have been
		     added already.  This could be returned because the expression is
		     evaluated as a node-set iterator, or because all nodes aren't
		     necessarily required. */

		ADDNODE_STOP
		/**< No more nodes need to be added. */
	};

	virtual OP_STATUS AddNode(XPathNode *node, AddNodeStatus &status) = 0;
	/**< Adds 'node' to this value.  The value's type must be 'node-set'; that
	     is, the SetNodeSet() function must be called before any nodes are
	     added to it.

	     The XPathValue object assumes ownership of 'node' regardless of
	     whether the call succeeds or not.  If the call fails, 'node' is freed
	     immediately, otherwise 'node' is freed sometime later (or possibly
	     immediately.)

	     It is possible the actual nodes in a node-set aren't relevant, only
	     whether there are any at all, or how many they are.  This means that
	     the added node might not be stored at all, and instead be freed
	     immediately.

	     The 'status' argument is used to signal whether more should be added
	     now, later or not at all.  If each node is calculated, and 'status'
	     is set to ADDNODE_PAUSE, it'd make sense to pause and return, and
	     continue the calculations later.  If all nodes are already known,
	     they can still all be added immediately (though this of course
	     requires some more memory for storage.)  If 'status' is set to
	     ADDNODE_STOP, all future nodes will be discarded anyway, so the
	     calling code might as well stop right now.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

protected:
	virtual ~XPathValue() {}
	/**< Destructor.  Objects are owned by the implementor and never deleted
	     through this interface. */
};

#endif // XPATH_INCLUDE_XPATHVALUE

#ifdef XPATH_INCLUDE_XPATHFUNCTION

class XPathFunction
{
public:
	virtual ~XPathFunction() {}
	/**< Destructor.  A function is destroyed when the expression or pattern
	     that referenced it is destroyed. */

	class State
	{
	public:
		virtual ~State() {}
		/**< Destructor.  The state object is destroyed by the XPath module
		     only if evaluation is terminated early. */
	};

	enum ResultType
	{
		TYPE_INVALID = 0,

		TYPE_NUMBER = 0x01,
		TYPE_BOOLEAN = 0x02,
		TYPE_STRING = 0x04,
		TYPE_NODESET = 0x08,

		TYPE_ANY = 0x0f
	};

	virtual unsigned GetResultType(unsigned *arguments, unsigned arguments_count) = 0;
	/**< Called to check the function's possible result types given the types
	     of the arguments.  If the function can return values of different
	     types, the different type enumerators should be OR:ed together.  The
	     specified argument types can also be several type enumerators OR:ed
	     together, though they're usually only unknown if the arguments in
	     turn are calls to extension functions that claim to be returning
	     different types or if the arguments are extension variable
	     references.

	     If there are too few or too many arguments, or if the arguments are
	     fundamentally wrongly typed (primitive when a node-set is required,)
	     this function should return zero (TYPE_INVALID,) signalling that the
	     function call is invalid.  This will cause a compilation error.

	     @param arguments The argument types.
	     @param arguments_count Number of arguments.

	     @return Possible result types OR:ed together, or zero if the
	             arguments are invalid. */

	enum Flags
	{
		FLAG_NEEDS_CONTEXT_POSITION = 0x01,
		/**< If reported, the correct context position will always be
		     specified when the function is called.  If not reported, the
		     XPath module will, as an optimization, not bother, and the
		     'context_position' field in Context will be zero in every call.
		     A function shouldn't report this unless actually necessary. */

		FLAG_NEEDS_CONTEXT_SIZE = 0x02,
		/**< If reported, the correct context size will always be specified
		     when the function is called.  If not reported, the XPath module
		     will, as an optimization, not bother, and the 'context_size'
		     field in Context will be zero in every call.  A function
		     shouldn't report this unless actually necessary. */

		FLAG_BLOCKING = 0x04
		/**< Must be reported if the function can block execution, that is, if
		     its Call() implementation can return RESULT_BLOCK. */
	};

	virtual unsigned GetFlags();
	/**< Called to check the function's information requirements.  Return
	     value should be zero or more of the Flags enumerators OR:ed
	     together.  The default implementation returns zero. */

	class CallContext
	{
	public:
		XPathNode *context_node;
		/**< Context node.  The reference is owned by the XPath module, so the
		     function implementation needs to copy the node if it wants to use
		     it outside the call to Call(). */

		unsigned context_position;
		/**< Context position.  Will be zero if the function didn't report it
		     needs it (see Flags,) otherwise it will always be non-zero (the
		     first node has position one.) */

		unsigned context_size;
		/**< Context size.  Will be zero if the function didn't report it
		     needs it (see Flags,) otherwise it will always be non-zero and
		     never less than 'context_position'. */

		XPathExpression::Evaluate **arguments;
		/**< Arguments.  Prepared with the specified context node, position
		     and size, but the function implementation is allowed to evaluate
		     the arguments in a context of its choice by setting a different
		     context on either or all of the argument objects. */

		unsigned arguments_count;
		/**< Number of arguments. */
	};

	enum Result
	{
		RESULT_FINISHED,
		/**< The function call finished. */
		RESULT_PAUSED,
		/**< The function call paused.  This return value should only be used
		     if the function is returning a node-set and can produce more
		     nodes right away but chose not to. */
		RESULT_BLOCKED,
		/**< The function call blocked.  This will typically cause the whole
		     XPath expression evaluation to block.  If, however, the function
		     is returning a node-set, and at least one node has already been
		     returned, it is possible that no more nodes are needed, and that
		     the function call is cancelled (and its State being destroyed)
		     immediately. */
		RESULT_FAILED,
		/**< The function call failed.  An evaluation error will be signalled
		     from the XPath engine. */
		RESULT_OOM
		/**< The function call failed due to OOM. */
	};

	virtual Result Call(XPathValue &result, XPathExtensions::Context *extensions_context, CallContext *call_context, State *&state) = 0;
	/**< Called each time the function is called while evaluating an XPath
	     expression or pattern.  The function should evaluate its arguments
	     itself, as it would evaluate a regular expression.

	     If the function call is asynchronous (or if the evaluation of any of
	     the arguments is,) OpBoolean::IS_FALSE should be returned.  In that
	     case, 'state' can be set to a state object implemented by the
	     function's implementation.  The value set to 'state' will be provided
	     in the next call to Call(), and will be deleted by the XPath module
	     if the evaluation (or the function call) is terminated before Call()
	     is called again.  If the function's result is a node-set, nodes can
	     be added to 'value' incrementally by each call to Call().  The same
	     XPathValue will be used in all related calls to Call().  If
	     calculating each node is expensive, but the overhead of calling
	     Call() many times is slow (on the function implementation's side,)
	     each call to Call() can add only one (or a few) nodes and then
	     return, even if more nodes could be calculated immediately.  The
	     nodes might be processed incrementally, and it is possible that all
	     nodes in the function's result aren't really required, especially if
	     the nodes are produced in document order.

	     If the function call fails, OpStatus::ERR should be returned.  This
	     will cause the evaluation to fail.

	     @param result Value that should be set to the function's result.
	     @param arguments Objects that can be used to evaluate the function's
	                      arguments.
	     @param arguments_count Number of arguments.
	     @param state NULL on the first call.  Can be set to a state object by
	                  the function's implementation, if OpBoolean::IS_FALSE is
	                  returned.

	     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE, OpStatus::ERR or
	             OpStatus::ERR_NO_MEMORY. */
};

#endif // XPATH_INCLUDE_XPATHFUNCTION


#ifdef XPATH_INCLUDE_XPATHVARIABLE

class XPathVariable
{
public:
	virtual ~XPathVariable() {}
	/**< Destructor.  A variable is destroyed when the expression or pattern
	     that referenced it is destroyed. */

	class State
	{
	public:
		virtual ~State() {}
		/**< Destructor.  The state object is destroyed by the XPath module
		     only if evaluation is terminated early. */
	};

	enum ValueType
	{
		TYPE_NUMBER = 0x01,
		TYPE_BOOLEAN = 0x02,
		TYPE_STRING = 0x04,
		TYPE_NODESET = 0x08,

		TYPE_ANY = 0x0f
	};

	virtual unsigned GetValueType() = 0;
	/**< Called to check the variable's possible value types.  If the variable
	     can have values of different types, the different type enumerators
	     should be OR:ed together.  If the variable's value type is
	     fundamentally unknown, TYPE_ANY should be returned.

	     @return Possible result types OR:ed together. */

	enum Flags
	{
		FLAG_BLOCKING = 0x01
		/**< Must be reported if the variable can block execution, that is, if
		     its GetValue() implementation can return RESULT_BLOCK. */
	};

	virtual unsigned GetFlags();
	/**< Called to check the function's information requirements.  Return
	     value should be zero or more of the Flags enumerators OR:ed
	     together.  The default implementation returns zero. */

	enum Result
	{
		RESULT_FINISHED,
		/**< The variable read finished. */
		RESULT_PAUSED,
		/**< The variable read paused.  This return value can only be used if
		     the variable is returning a node-set and can produce more nodes
		     right away but chose not to.  When used, the variable type must
		     be signalled first by calling XPathValue::SetNodeSet() on the
		     return value object. */
		RESULT_BLOCKED,
		/**< The variable read blocked.  This will typically cause the whole
		     XPath expression evaluation to block.  If, however, the variable
		     is returning a node-set, and at least one node has already been
		     returned, it is possible that no more nodes are needed, and that
		     the variable read is cancelled (and its State being destroyed)
		     immediately. */
		RESULT_FAILED,
		/**< The variable read failed.  An evaluation error will be signalled
		     from the XPath engine. */
		RESULT_OOM
		/**< The variable read failed due to OOM. */
	};

	virtual Result GetValue(XPathValue &value, XPathExtensions::Context *extensions_context, State *&state) = 0;
	/**< Called to retrieve the variable's value.  Only called once per
	     evaluation, even if the value is used many times.

	     If the variable's value is available immediately, the appropriate
	     functions should be called on 'value' to set it and
	     OpBoolean::IS_TRUE should be returned.

	     If the variable's value is not available (needs to be calculated
	     asynchronously,) OpBoolean::IS_FALSE should be returned.  In that
	     case, 'state' can be set to a state object implemented by the
	     variable's implementation.  The value set to 'state' will be provided
	     in the next call to GetValue(), and will be deleted by the XPath
	     module if the evaluation is terminated before GetValue() is called
	     again.  If the variable's value is a node-set and some of the nodes
	     are known immediately, they can be added to 'value' right away.  The
	     same XPathValue will be used in all related calls to GetValue().

		 If the variable's value cannot be read, OpStatus::ERR should be
		 returned.  This will cause the evaluation to fail.

	     @param value Value that should be set to the variable's value.
	     @param state NULL on the first call.  Can be set to a state object by
	                  the variable's implementation, if OpBoolean::IS_FALSE is
	                  returned.

	     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE, OpStatus::ERR or
	             OpStatus::ERR_NO_MEMORY. */
};

#endif // XPATH_INCLUDE_XPATHVARIABLE
#endif // XPATH_EXTENSION_SUPPORT
