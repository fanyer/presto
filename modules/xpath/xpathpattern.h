/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef XPATHPATTERN_H
#define XPATHPATTERN_H

/* Note: You can just include modules/xpath/xpath.h, it pulls in the entire
   XPath API. */

#ifdef XPATH_PATTERN_SUPPORT
#include "modules/xpath/xpath.h"

#ifdef XPATH_INCLUDE_XPATHPATTERN

/** An XPath pattern context object defines a context in which patterns are
    used.  The context object will be used to cache information about pattern
    matches per tree (XMLTreeAccessor,) and because of this, to update such
    cached information when a tree is destroyed.

    A pattern context's life-time is intended to encompass all related pattern
    operations. */
class XPathPatternContext
{
public:
	static void Free(XPathPatternContext *context);
	/**< Deletes the XPathPatternContext object and frees all resources
	     allocated by it.

	     @param context Object to free or NULL. */

	static OP_STATUS Make(XPathPatternContext *&context);
	/**< Creates a pattern context object. */

	virtual void InvalidateTree(XMLTreeAccessor *tree) = 0;
	/**< Invalidates all cached information about nodes from 'tree'.  Must be
	     called when a tree that has ever been used in pattern operations is
	     destroyed, as cached information might otherwise be applied to other
	     trees constructed later that happens to be allocated in the same
	     memory location.  Can be called to invalidate trees that haven't been
	     used in pattern operations, or for which no information has been
	     cached.  If the pattern context is not going to be used any more, it
	     is not necessary to invalidate any trees; destroying the context
	     serves the same purpose.

	     @param tree Tree to invalidate cached information about. */

protected:
	virtual ~XPathPatternContext();
	/**< Destructor.  Do not free XPathPatternContext objects using the delete
	     operator, use the Free function. */
};

/** An XPath pattern object used to match node against patterns.

    To support namespace prefixes in the expression, first create an
    XPathNamespaces object (either create an empty and just add all supported
    namespace prefixes to it or create one from the expression source and the
    set a namespace URI for each namespace prefix found in the expression
    source).

    XPath patterns are primarily used in XSL Transformations to select
    templates to apply.  See section
      <a href="http://www.w3.org/TR/xslt.html#patterns">5.2 Patterns</a>
    in the XSL Transformations specification for more information
    about the special rules governing the patterns.

    All XPathPattern objects created must be freed using the
    XPathPattern::Free() function.  They can be reused (that is, matched)
    any number of times. */
class XPathPattern
{
public:
	static void Free(XPathPattern *pattern);
	/**< Deletes the XPathPattern object and frees all resources
	     allocated by it.

	     @param pattern Object to free or NULL. */

	class PatternData
	{
	public:
		PatternData();
		/**< Constructor.  Sets all members to NULL. */

		const uni_char *source;
		/**< Source code of the expression.  Cannot be NULL. */

		XPathNamespaces *namespaces;
		/**< Collection of prefix:uri bindings available.  Can be NULL. */

#ifdef XPATH_EXTENSION_SUPPORT
		XPathExtensions *extensions;
		/**< Collection of extension functions and variables available.  Can
		     be NULL. */
#endif // XPATH_EXTENSION_SUPPORT

#ifdef XPATH_ERRORS
		OpString *error_message;
		/**< If non-NULL, storage used to generate error message for errors
		     detected during parsing or compilation of pattern.  The string
		     will be cleared first if it is not empty.  If no error is
		     detected, the string is left unchanged.  The error message is
		     typically an unformatted single line string. */
#endif // XPATH_ERRORS
	};

	static OP_STATUS Make(XPathPattern *&pattern, const PatternData &data);
	/**< Creates an XPathPattern object from the information stored in 'data'.
	     The pattern must be a node-set expression matching the limited
	     pattern syntax defined by XSLT 1.0.  If API_XPATH_ERRORS has been
	     imported and errors are detected, an error message is generated into
	     'data.error_message' if non-NULL.  If NULL, no error message is
	     generated, but error detection is of course unaffected.

	     The created pattern is owned by the caller and must be freed using
	     the XPathPattern::Free() function.  The pattern object itself is
	     stateless (except for cached information,) and can be used any number
	     of times, even multiple times in parallell.

	     @param pattern Set to the created XPathPattern object.
	     @param data Pattern data.

	     @return OpStatus::OK, OpStatus::ERR if an error is detected and
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS DEPRECATED(Make(XPathPattern *&pattern, const uni_char *source, XPathNamespaces *namespaces, XPathExtensions *extensions));
	/**< Creates an XPathPattern object from the given string using the given
	     XPathNamespaces object to resolve any namespace prefixes.  The
	     pattern must be a location path that only uses the axes 'children'
	     and 'attributes'.

	     @param pattern Set to the created XPathPattern object.
	     @param source The pattern source string.
	     @param namespaces Collection of namespace prefix/URI pairs.

	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or
	             OpStatus::ERR on parse errors or if the pattern is
	             not a valid XSLT pattern. */

	static OP_BOOLEAN GetNextAlternative(const uni_char *&alternative, unsigned &alternative_length, const uni_char *&source);
	/**< Given a string on the form "pattern|pattern|..." in 'source', sets
	     'alternative' to point at the beginning of the first alternative
	     pattern, sets 'alternative_length' to the length of that alternative
	     pattern and updates 'source' to point at at the beginning of the next
	     alternative pattern.  If a suitable alternative pattern is found,
	     OpBoolean::IS_TRUE is returned.

	     If 'source' is an empty string, OpBoolean::IS_FALSE is returned.  If
	     there is a syntax error in 'source', OpStatus::ERR is returned.  Note
	     that not all errors are detected.  Use XPathPattern::Make on the
	     returned substring to be sure the alternative is a valid pattern.  If
	     an error is detected, parsing the whole (remaining) string in
	     'source' as a pattern using XPathPattern::Make() will produce an
	     error message detailing the error.

	     Suitable for iterating through the alternative patterns in an XSLT
	     pattern attribute (such as the 'match' attribute of the 'template'
	     element.)

	     @param alternative Set to the beginning of a pattern.
	     @param alternative_length Set to the length of the pattern pointed at
	                               by 'alternative'.
	     @param source Should point at a XSLT pattern string, updated upon
	                   successful return (though it will still point into the
	                   same string.)

	     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE or OpStatus::ERR. */

	class Search
	{
	public:
		static OP_STATUS DEPRECATED(Make(Search *&search, XPathPatternContext *context, XPathNode *node, XPathPattern **patterns, unsigned patterns_count));
		/**< Creates an object that matches the given node and all of its
		     descendants against each of the specified patterns and returns
		     the nodes that matched at least one pattern in document order.
		     This function assumes ownership of 'node' regardless of whether
		     it succeeds or not. */

		static OP_STATUS Make(Search *&search, XPathPatternContext *context, XPathPattern **patterns, unsigned patterns_count);
		/**< Creates an object that can be used to search for nodes matching
		     any of the patterns in a tree.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

		static void Free(Search *search);
		/**< Deletes the Search object and frees all resources allocated by it.

		     @param search Object to free or NULL. */

		virtual OP_STATUS Start(XPathNode *node) = 0;
		/**< Start search in the tree containing 'node'.  Any element or
		     attribute node in that tree can be matched, regardless of their
		     relation to 'node'.  Normally, 'node' should simply be the root
		     node of the tree.

		     This function assumes ownership of 'node'.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

#ifdef XPATH_EXTENSION_SUPPORT
		virtual void SetExtensionsContext(XPathExtensions::Context *context) = 0;
		/**< Set the extensions context for this evaluation.  Will be passed
		     to any XPathFunction or XPathVariable implementation used during
		     the evaluation.  The object is owned by the calling code and will
		     not be deallocated or otherwise manipulated by the XPath engine.

		     If not set, or set to NULL, the corresponding argument to
		     extension implementations will be NULL. */
#endif // XPATH_EXTENSION_SUPPORT

		virtual void SetCostLimit(unsigned limit) = 0;
		/**< Sets a limit on how much work a single call to GetNextNode() can
		     perform.  If the limit is reached before the called function
		     finished, it will pause and return OpBoolean::IS_FALSE.  An
		     excessively low value will increase over-head, but anything in
		     the range 512-1024 and up will probably achieve completely
		     acceptable speed and over-head.  The default cost limit is
		     controlled by a tweak. */

		virtual unsigned GetLastOperationCost() = 0;
		/**< Returns the cost of the last call to GetNextNode(). */

		virtual OP_BOOLEAN GetNextNode(XPathNode *&node) = 0;
		/**< Retrieve the next matching node.  If no more matching nodes were
		     found, 'node' is set to NULL.

		     If OpBoolean::IS_FALSE is returned the searching was paused.
		     This is handled the same way as paused expression evaluations, so
		     see the documentation for XPathExpression::Evaluate for more
		     details. */

#ifdef XPATH_ERRORS
		virtual const uni_char *GetErrorMessage() = 0;
		/**< Returns a string describing the failure if the search failed.  If
		     the search hasn't failed (yet) NULL is returned.  The string is
		     owned by the search object and will remain valid until it is
		     destroyed.

		     @return Error message or NULL. */

		virtual const uni_char *GetFailedPatternSource() = 0;
		/**< Returns the full source code of the pattern (among the patterns
		     specified in the call to Make()) that caused the operation to
		     fail.  If the operation hasn't failed (yet) NULL is returned.
		     The string is owned by the pattern object and will remain valid
		     until it is destroyed.

		     @return Pattern source or NULL. */
#endif // XPATH_ERRORS

		virtual void Stop() = 0;
		/**< Stop searching and reset the search object for future reuse. */

	protected:
		virtual ~Search ();
		/**< Destructor.  Do not free XPathPattern::Search objects using the
		     delete operator, use the Free function. */
	};

	class Count
	{
	public:
		/** Values for the 'level' argument to Make(). */
		enum Level
		{
			LEVEL_SINGLE,
			/**< Counts preceding siblings that match any of the 'count'
			     patterns of the first node on the ancestor-or-self axis
				 that match any of the 'count' patterns up to the first
				 ancestor that matches any of the 'from' patterns. */

			LEVEL_MULTIPLE,
			/**< Counts preceding siblings that match any of the 'count'
			     patterns of all nodes on the ancestor-or-self axis that
				 match any of the 'count' patterns up to the first
				 node that matches any of the 'from' patterns. */

			LEVEL_ANY
			/**< Counts preceding nodes that match any of the 'count'
			     patterns up the the first node that matches any of the
				 'from' patterns. */
		};

		static OP_STATUS Make(Count *&count, XPathPatternContext *context, Level level, XPathNode *node, XPathPattern **count_patterns, unsigned count_patterns_count, XPathPattern **from_patterns, unsigned from_patterns_count);
		/**< Creates an object that counts the nodes that match the 'count'
		     patterns.  The exact way it does this is determined by the
		     'level' argument.  If zero 'count' patterns are provided, a
		     single pattern that matches nodes of the same type and with the
		     same expanded name as 'node' is used.  If zero 'from' patterns
		     are provided, a single pattern that matches no nodes is used.

		     This function assumes ownership of 'node' regardless of whether
		     it succeeds or not.

		     This function is typically used to implement the xsl:number
		     element in XSLT.

		     @param level How to count.
		     @param node Node to start counting from.
		     @param count Array of patterns.
		     @param count_count Number of patterns in 'count'.
		     @param from Array of patterns.
		     @param from_count Number of patterns in 'from'.
		     @param match_key This callback will be called to determine
		                      whether a node would be included in the node-set
		                      returned by the key extension function defined
		                      in XSLT 1.0.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

		static void Free(Count *count);
		/**< Deletes the Count object and frees all resources allocated by it.

		     @param count Object to free or NULL. */

#ifdef XPATH_EXTENSION_SUPPORT
		virtual void SetExtensionsContext(XPathExtensions::Context *context) = 0;
		/**< Set the extensions context for this evaluation.  Will be passed
		     to any XPathFunction or XPathVariable implementation used during
		     the evaluation.  The object is owned by the calling code and will
		     not be deallocated or otherwise manipulated by the XPath engine.

		     If not set, or set to NULL, the corresponding argument to
		     extension implementations will be NULL. */
#endif // XPATH_EXTENSION_SUPPORT

		virtual void SetCostLimit(unsigned limit) = 0;
		/**< Sets a limit on how much work a single call to GetResult() can
		     perform.  If the limit is reached before the called function
		     finished, it will pause and return OpBoolean::IS_FALSE.  An
		     excessively low value will increase over-head, but anything in
		     the range 512-1024 and up will probably achieve completely
		     acceptable speed and over-head.  The default cost limit is
		     controlled by a tweak. */

		virtual unsigned GetLastOperationCost() = 0;
		/**< Returns the cost of the last call to GetResult(). */

		virtual OP_BOOLEAN GetResultLevelled(unsigned &numbers_count, unsigned *&numbers) = 0;
		/**< 'Numbers' will be set to an allocated array of node counts.
		     'Number' will be set to the number of elements in that array.
		     The array needs to be deallocated by the caller using the
		     delete[] operator.

		     If OpBoolean::IS_FALSE is returned the counting was paused.  This
		     is handled the same way as paused expression evaluations, so see
		     the documentation for XPathExpression::Evaluate for more details.

		     @param number Set to the number of elements in the array in
		                   'numbers'.
		     @param numbers Set to an allocated array.  The array must be
		                    freed by the caller using the delete[] operator.

		     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE, OpStatus::ERR or
		             OpStatus::ERR_NO_MEMORY. */

		virtual OP_BOOLEAN GetResultAny(unsigned &number) = 0;
		/**< 'Numbers' will be set to an allocated array of node counts.
		     'Number' will be set to the number of elements in that array.
		     The array needs to be deallocated by the caller using the
		     delete[] operator.

		     If OpBoolean::IS_FALSE is returned the counting was paused.  This
		     is handled the same way as paused expression evaluations, so see
		     the documentation for XPathExpression::Evaluate for more details.

		     @param number Set to the number of elements in the array in
		                   'numbers'.
		     @param numbers Set to an allocated array.  The array must be
		                    freed by the caller using the delete[] operator.

		     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE, OpStatus::ERR or
		             OpStatus::ERR_NO_MEMORY. */

#ifdef XPATH_ERRORS
		virtual const uni_char *GetErrorMessage() = 0;
		/**< Returns a string describing the failure if the count failed.  If
		     the count hasn't failed (yet) NULL is returned.  The string is
		     owned by the count object and will remain valid until it is
		     destroyed.

		     @return Error message or NULL. */

		virtual const uni_char *GetFailedPatternSource() = 0;
		/**< Returns the full source code of the pattern (among the patterns
		     specified in the call to Make()) that caused the operation to
		     fail.  If the operation hasn't failed (yet) NULL is returned.
		     The string is owned by the pattern object and will remain valid
		     until it is destroyed.

		     @return Pattern source or NULL. */
#endif // XPATH_ERRORS

	protected:
		virtual ~Count ();
		/**< Destructor.  Do not free XPathPattern::Count objects using the
		     delete operator, use the Free function. */
	};

	class Match
	{
	public:
		static OP_STATUS Make(Match *&match, XPathPatternContext *context, XPathNode *node, XPathPattern **patterns, unsigned patterns_count);
		/**< Creates an object that matches the specified node against the
		     supplied patterns until one of them matches.  This function
		     assumes ownership of 'node' regardless of whether it succeeds or
		     not. */

		static void Free(Match *match);
		/**< Deletes the Match object and frees all resources allocated by it.

		     @param match Object to free or NULL. */

#ifdef XPATH_EXTENSION_SUPPORT
		virtual void SetExtensionsContext(XPathExtensions::Context *context) = 0;
		/**< Set the extensions context for this evaluation.  Will be passed
		     to any XPathFunction or XPathVariable implementation used during
		     the evaluation.  The object is owned by the calling code and will
		     not be deallocated or otherwise manipulated by the XPath engine.

		     If not set, or set to NULL, the corresponding argument to
		     extension implementations will be NULL. */
#endif // XPATH_EXTENSION_SUPPORT

		virtual void SetCostLimit(unsigned limit) = 0;
		/**< Sets a limit on how much work a single call to
		     GetMatchedPattern() can perform.  If the limit is reached before
		     the called function finished, it will pause and return
		     OpBoolean::IS_FALSE.  An excessively low value will increase
		     over-head, but anything in the range 512-1024 and up will
		     probably achieve completely acceptable speed and over-head.  The
		     default cost limit is controlled by a tweak. */

		virtual unsigned GetLastOperationCost() = 0;
		/**< Returns the cost of the last call to GetMatchedPattern(). */

		virtual OP_BOOLEAN GetMatchedPattern(XPathPattern *&pattern) = 0;
		/**< Checks if any of the patterns matched, and if so, returns which
		     pattern did.  If no pattern matched, 'pattern' is set to NULL.

		     If OpBoolean::IS_FALSE is returned the match was paused.  This
		     is handled the same way as paused expression evaluations, so see
		     the documentation for XPathExpression::Evaluate for more details.

		     @return OpBoolean::IS_TRUE, OpBoolean::IS_FALSE, OpStatus::ERR or
		             OpStatus::ERR_NO_MEMORY. */

#ifdef XPATH_ERRORS
		virtual const uni_char *GetErrorMessage() = 0;
		/**< Returns a string describing the failure if the match failed.  If
		     the match hasn't failed (yet) NULL is returned.  The string is
		     owned by the match object and will remain valid until it is
		     destroyed.

		     @return Error message or NULL. */

		virtual const uni_char *GetFailedPatternSource() = 0;
		/**< Returns the full source code of the pattern (among the patterns
		     specified in the call to Make()) that caused the operation to
		     fail.  If the operation hasn't failed (yet) NULL is returned.
		     The string is owned by the pattern object and will remain valid
		     until it is destroyed.

		     @return Pattern source or NULL. */
#endif // XPATH_ERRORS

	protected:
		virtual ~Match ();
		/**< Destructor.  Do not free XPathPattern::Match objects using the
		     delete operator, use the Free function. */
	};

	virtual float GetPriority() = 0;
	/**< Calculate the default priority of a XSLT template with this
	     pattern.  For more information, see
	       <a href="http://www.w3.org/TR/xslt.html#conflict">5.5 Conflict Resolution for Template Rules</a>.

	     @return 0.5, 0, -0.25 or -0.5 depending on the pattern. */

#ifdef XPATH_NODE_PROFILE_SUPPORT
	enum ProfileVerdict
	{
		PATTERN_WILL_MATCH_PROFILED_NODES,
		/**< Pattern is guaranteed to match any node included by the node
		     profile.  This means that the pattern will match all nodes
		     returned by an expression that claims to return nodes included by
		     the node profile. */

		PATTERN_CAN_MATCH_PROFILED_NODES,
		/**< Pattern can, but is not guaranteed to, match any node included by
		     the node profile. */

		PATTERN_DOES_NOT_MATCH_PROFILED_NODES
		/**< Pattern is guaranteed to not match any node included by the node
		     profile.  This means that the pattern will not match any node
		     returned by an expression that claims to return nodes included by
		     the node profile. */
	};

	virtual ProfileVerdict GetProfileVerdict(XPathNodeProfile *profile) = 0;
	/**< Determines if the pattern will, can, or cannot match nodes included
	     by the specified profile. */
#endif // XPATH_NODE_PROFILE_SUPPORT

protected:
	virtual ~XPathPattern();
	/**< Destructor.  Do not free XPathPattern objects using the delete
	     operator, use the Free function. */
};

#endif // XPATH_INCLUDE_XPATHPATTERN
#endif // XPATH_PATTERN_SUPPORT
#endif // XPATHPATTERN_H
