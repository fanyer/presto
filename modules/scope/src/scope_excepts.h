/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Macros for returning errors from Scope commands.
*/

#ifndef SCOPE_EXCEPTS_H
#define SCOPE_EXCEPTS_H

/**
 * Respond to a Scope command with the specified status and error description.
 * This will cause the function to return with OpStatus::ERR. The Scope error
 * will be sent to the client once the current command is finished processing.
 *
 * @param service A pointer to the OpScopeService.
 * @param status Which Scope status to return on errors (e.g. BadRequest).
 * @param message Static unicode string to return to the client.
 */
#define SCOPE_RESPONSE(service, status, message) \
	do { \
		return service->SetCommandError(status, message); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE
 *
 * This macro supports one argument for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE
 */
#define SCOPE_RESPONSE1(service, status, message, arg0) \
	do { \
		OpScopeTPError error(status); \
		RETURN_IF_ERROR(error.SetFormattedDescription(message, arg0)); \
		return service->SetCommandError(error); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE
 *
 * This macro supports two arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE
 */
#define SCOPE_RESPONSE2(service, status, message, arg0, arg1) \
	do { \
		OpScopeTPError error(status); \
		RETURN_IF_ERROR(error.SetFormattedDescription(message, arg0, arg1)); \
		return service->SetCommandError(error); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE
 *
 * This macro supports three arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE
 */
#define SCOPE_RESPONSE3(service, status, message, arg0, arg1, arg2) \
	do { \
		OpScopeTPError error(status); \
		RETURN_IF_ERROR(error.SetFormattedDescription(message, arg0, arg1, arg2)); \
		return service->SetCommandError(error); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE
 *
 * This macro assumes OpScopeTPHeader::BadRequest as the error status.
 *
 * @see SCOPE_RESPONSE
 */
#define SCOPE_BADREQUEST(service, message) \
	SCOPE_RESPONSE(service, OpScopeTPHeader::BadRequest, message)

/**
 * @copydoc SCOPE_BADREQUEST
 *
 * This macro supports one argument for printf-style string formatting.
 *
 * @see SCOPE_BADREQUEST
 */
#define SCOPE_BADREQUEST1(service, message, arg0) \
	SCOPE_RESPONSE1(service, OpScopeTPHeader::BadRequest, message, arg0)

/**
 * @copydoc SCOPE_BADREQUEST
 *
 * This macro supports two arguments for printf-style string formatting.
 *
 * @see SCOPE_BADREQUEST
 */
#define SCOPE_BADREQUEST2(service, message, arg0, arg1) \
	SCOPE_RESPONSE2(service, OpScopeTPHeader::BadRequest, message, arg0, arg1)

/**
 * @copydoc SCOPE_BADREQUEST
 *
 * This macro supports three arguments for printf-style string formatting.
 *
 * @see SCOPE_BADREQUEST
 */
#define SCOPE_BADREQUEST3(service, message, arg0, arg1, arg2) \
	SCOPE_RESPONSE3(service, OpScopeTPHeader::BadRequest, message, arg0, arg1, arg2)

/**
 * If the expression is an error, record the status and message as
 * a scope error in the specified service and return OpStatus::ERR.
 * The scope error will be sent to the client once the current command
 * is finished processing.
 *
 * If the expression is a memory error, the error message will
 * not be used, and the memory error will be returned instead.
 *
 * @param expr The expression that maybe evaluates to an error.
 * @param service A pointer to the OpScopeService.
 * @param status Which Scope status to return on errors (e.g. BadRequest).
 * @param message Static unicode string to return to the client.
 */
#define SCOPE_RESPONSE_IF_ERROR(expr, service, status, message) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
			return RETURN_IF_ERROR_TMP; \
		else if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			SCOPE_RESPONSE(service, status, message); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE_IF_ERROR
 *
 * This macro supports one argument for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_ERROR
 */
#define SCOPE_RESPONSE_IF_ERROR1(expr, service, status, message, arg0) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
			return RETURN_IF_ERROR_TMP; \
		else if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			SCOPE_RESPONSE1(service, status, message, arg0); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE_IF_ERROR
 *
 * This macro supports two arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_ERROR
 */
#define SCOPE_RESPONSE_IF_ERROR2(expr, service, status, message, arg0, arg1) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
			return RETURN_IF_ERROR_TMP; \
		else if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			SCOPE_RESPONSE2(service, status, message, arg0, arg1); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE_IF_ERROR
 *
 * This macro supports three arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_ERROR
 */
#define SCOPE_RESPONSE_IF_ERROR3(expr, service, status, message, arg0, arg1, arg2) \
	do { \
		OP_STATUS RETURN_IF_ERROR_TMP = expr; \
		if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
			return RETURN_IF_ERROR_TMP; \
		else if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
			SCOPE_RESPONSE3(service, status, message, arg0, arg1, arg2); \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE_IF_ERROR
 *
 * This macro assumes OpScopeTPHeader::BadRequest as the error status.
 *
 * @see SCOPE_RESPONSE_IF_ERROR
 */
#define SCOPE_BADREQUEST_IF_ERROR(expr, service, message) \
	SCOPE_RESPONSE_IF_ERROR(expr, service, OpScopeTPHeader::BadRequest, message)

/**
 * @copydoc SCOPE_BADREQUEST_IF_ERROR
 *
 * Supports one argument for printf-style formatting.
 *
 * @see SCOPE_BADREQUEST_IF_ERROR
 */
#define SCOPE_BADREQUEST_IF_ERROR1(expr, service, message, arg0) \
	SCOPE_RESPONSE_IF_ERROR1(expr, service, OpScopeTPHeader::BadRequest, message, arg0)

/**
 * @copydoc SCOPE_BADREQUEST_IF_ERROR
 *
 * Supports two arguments for printf-style formatting.
 *
 * @see SCOPE_BADREQUEST_IF_ERROR
 */
#define SCOPE_BADREQUEST_IF_ERROR2(expr, service, message, arg0, arg1) \
	SCOPE_RESPONSE_IF_ERROR2(expr, service, OpScopeTPHeader::BadRequest, message, arg0, arg1)

/**
 * @copydoc SCOPE_BADREQUEST_IF_ERROR
 *
 * Supports three arguments for printf-style formatting.
 *
 * @see SCOPE_BADREQUEST_IF_ERROR
 */
#define SCOPE_BADREQUEST_IF_ERROR3(expr, service, message, arg0, arg1, arg2) \
	SCOPE_RESPONSE_IF_ERROR3(expr, service, OpScopeTPHeader::BadRequest, message, arg0, arg1, arg2)

/**
 * If the expression evaluates to FALSE, record the status and message as
 * a scope error in the specified service and return OpStatus::ERR.
 * The scope error will be sent to the client once the current command
 * is finished processing.
 *
 * @param expr The expression that maybe evaluates to FALSE.
 * @param service A pointer to the OpScopeService.
 * @param status Which Scope status to return on errors (e.g. BadRequest).
 * @param message Static unicode string to return to the client.
 */
#define SCOPE_RESPONSE_IF_FALSE(expr, service, status, message) \
	do { \
		if (!(expr)) \
			return service->SetCommandError(status, message); \
	} while (0)


/**
 * @copydoc SCOPE_RESPONSE_IF_FALSE
 *
 * This macro supports one argument for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_FALSE
 */
#define SCOPE_RESPONSE_IF_FALSE1(expr, service, status, message, arg0) \
	do { \
		if (!(expr)) \
		{ \
			OpScopeTPError error(status); \
			RETURN_IF_ERROR(error.SetFormattedDescription(message, arg0)); \
			return service->SetCommandError(error); \
		} \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE_IF_FALSE
 *
 * This macro supports two arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_FALSE
 */
#define SCOPE_RESPONSE_IF_FALSE2(expr, service, status, message, arg0, arg1) \
	do { \
		if (!(expr)) \
		{ \
			OpScopeTPError error(status); \
			RETURN_IF_ERROR(error.SetFormattedDescription(message, arg0, arg1)); \
			return service->SetCommandError(error); \
		} \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE_IF_FALSE
 *
 * This macro supports three arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_FALSE
 */
#define SCOPE_RESPONSE_IF_FALSE3(expr, service, status, message, arg0, arg1, arg2) \
	do { \
		if (!(expr)) \
		{ \
			OpScopeTPError error(status); \
			RETURN_IF_ERROR(error.SetFormattedDescription(message, arg0, arg1, arg2)); \
			return service->SetCommandError(error); \
		} \
	} while (0)

/**
 * @copydoc SCOPE_RESPONSE_IF_FALSE
 *
 * This macro assumes OpScopeTPHeader::BadRequest as the error status.
 *
 * @see SCOPE_RESPONSE_IF_FALSE
 */
#define SCOPE_BADREQUEST_IF_FALSE(expr, service, message) \
	SCOPE_RESPONSE_IF_FALSE(expr, service, OpScopeTPHeader::BadRequest, message)

/**
 * @copydoc SCOPE_BADREQUEST_IF_FALSE
 *
 * Supports one argument for printf-style formatting.
 *
 * @see SCOPE_BADREQUEST_IF_FALSE
 */
#define SCOPE_BADREQUEST_IF_FALSE1(expr, service, message, arg0) \
	SCOPE_RESPONSE_IF_FALSE1(expr, service, OpScopeTPHeader::BadRequest, message, arg0)

/**
 * @copydoc SCOPE_BADREQUEST_IF_FALSE
 *
 * Supports two arguments for printf-style formatting.
 *
 * @see SCOPE_BADREQUEST_IF_FALSE
 */
#define SCOPE_BADREQUEST_IF_FALSE2(expr, service, message, arg0, arg1) \
	SCOPE_RESPONSE_IF_FALSE2(expr, service, OpScopeTPHeader::BadRequest, message, arg0, arg1)

/**
 * @copydoc SCOPE_BADREQUEST_IF_FALSE
 *
 * Supports three arguments for printf-style formatting.
 *
 * @see SCOPE_BADREQUEST_IF_FALSE
 */
#define SCOPE_BADREQUEST_IF_FALSE3(expr, service, message, arg0, arg1, arg2) \
	SCOPE_RESPONSE_IF_FALSE3(expr, service, OpScopeTPHeader::BadRequest, message, arg0, arg1, arg2)

/**
 * If the expression evaluates to NULL, record the status and message as
 * a scope error in the specified service and return OpStatus::ERR.
 * The scope error will be sent to the client once the current command
 * is finished processing.
 *
 * @param expr The expression that maybe evaluates to NULL.
 * @param service A pointer to the OpScopeService.
 * @param status Which Scope status to return on errors (e.g. BadRequest).
 * @param message Static unicode string to return to the client.
 */
#define SCOPE_RESPONSE_IF_NULL(expr, service, status, message) \
	SCOPE_RESPONSE_IF_FALSE((expr) != NULL, service, status, message)

/**
 * @copydoc SCOPE_RESPONSE_IF_NULL
 *
 * This macro supports one argument for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_NULL
 */
#define SCOPE_RESPONSE_IF_NULL1(expr, service, status, message, arg0) \
	SCOPE_RESPONSE_IF_FALSE1((expr) != NULL, service, status, message, arg0)

/**
 * @copydoc SCOPE_RESPONSE_IF_NULL
 *
 * This macro supports two arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_NULL
 */
#define SCOPE_RESPONSE_IF_NULL2(expr, service, status, message, arg0, arg1) \
	SCOPE_RESPONSE_IF_FALSE2((expr) != NULL, service, status, message, arg0, arg1)

/**
 * @copydoc SCOPE_RESPONSE_IF_NULL
 *
 * This macro supports three arguments for printf-style string formatting.
 *
 * @see SCOPE_RESPONSE_IF_NULL
 */
#define SCOPE_RESPONSE_IF_NULL3(expr, service, status, message, arg0, arg1, arg2) \
	SCOPE_RESPONSE_IF_FALSE3((expr) != NULL, service, status, message, arg0, arg1, arg2)

/**
 * @copydoc SCOPE_RESPONSE_IF_NULL
 *
 * This macro assumes OpScopeTPHeader::BadRequest as the error status.
 *
 * @see SCOPE_RESPONSE_IF_NULL
 */
#define SCOPE_BADREQUEST_IF_NULL(expr, service, message) \
	SCOPE_RESPONSE_IF_NULL(expr, service, OpScopeTPHeader::BadRequest, message)

/**
 * @copydoc SCOPE_RESPONSE_IF_NULL
 *
 * Supports one argument for printf-style formatting.
 *
 * @see SCOPE_RESPONSE_IF_NULL
 */
#define SCOPE_BADREQUEST_IF_NULL1(expr, service, message, arg0) \
	SCOPE_RESPONSE_IF_NULL1(expr, service, OpScopeTPHeader::BadRequest, message, arg0)

/**
 * @copydoc SCOPE_RESPONSE_IF_NULL
 *
 * Supports two arguments for printf-style formatting.
 *
 * @see SCOPE_RESPONSE_IF_NULL
 */
#define SCOPE_BADREQUEST_IF_NULL2(expr, service, message, arg0, arg1) \
	SCOPE_RESPONSE_IF_NULL2(expr, service, OpScopeTPHeader::BadRequest, message, arg0, arg1)

/**
 * @copydoc SCOPE_RESPONSE_IF_NULL
 *
 * Supports three arguments for printf-style formatting.
 *
 * @see SCOPE_RESPONSE_IF_NULL
 */
#define SCOPE_BADREQUEST_IF_NULL3(expr, service, message, arg0, arg1, arg2) \
	SCOPE_RESPONSE_IF_NULL3(expr, service, OpScopeTPHeader::BadRequest, message, arg0, arg1, arg2)

#endif // SCOPE_EXCEPTS_H
