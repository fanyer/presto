/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) Opera Software ASA 2001-2004.
*/

/** @mainpage Automatic proxy configuration
 *
 * The interface function ::CreateAutoProxyLoadHandler() creates a 
 * URL_LoadHandler <b>h</b> that determines the proxy for a URL.
 *
 * <b>h.Load()</b> starts loading a configuration script and waits for it 
 * through callbacks on <b>h.HandleCallback()</b>.  When the script has 
 * been loaded, <b>h.Load()</b> is called again and runs the script to obtain
 * proxy information.  The proxy information is posted to the URL layer.
 *
 * The proxy configuration code may cache resources internally.  These
 * resources can be released by calling ::ReleaseAutoProxyResources().
 *
 *
 * \section errorhandling Error handling
 * 
 * Here are summaries about the possible error conditions and the actions
 * taken.  When it is stated that MSG_COMM_LOADING_FAILED is signalled on the
 * comm layer (really on the message handler in the loadhandler), the
 * assumption is that the comm layer will do something useful with it, normally
 * presenting an error message in some way.
 *
 * Out-of-memory
 *   - OOM is signalled with MemoryManager::RaiseError
 *   - MSG_COMM_LOADING_FAILED with ERR_COMM_INTERNAL_ERROR is signalled on 
 *     the comm layer
 *
 * Bad script URL
 *   - Proxy configuration is disabled
 *   - MSG_COMM_LOADING_FAILED with ERR_COMM_INTERNAL_ERROR is signalled on 
 *     the comm layer
 *
 * Script loading fails
 *   - Proxy configuration is disabled
 *   - MSG_COMM_LOADING_FAILED is signalled with either ERR_SSL_ERROR_HANDLED 
 *     or ERR_AUTO_PROXY_CONFIG_FAILED
 *
 * Script can't be compiled
 *   - Proxy configuration is disabled
 *   - Error message is printed on ECMAScript console
 *   - Either URL_AutoProxyConfig_LoadHandler::Load returns COMM_REQUEST_FAILED,
 *     or MSG_COMM_LOADING_FAILED with ERR_AUTO_PROXY_CONFIG_FAILED is signalled
 *     on the comm layer
 *
 * A call to FindProxyForURL() throws an exception
 *   - As for scripts that can't be compiled
 *
 * A call to FindProxyForURL() does not terminate
 *   - As for scripts that can't be compiled
 */

/** @file autoproxy_public.h
 *
 * Public API for Automatic proxy configuration through JavaScript.
 *
 * @author Lars T Hansen  (lth@opera.com)
 */

#ifndef AUTOPROXY_PUBLIC_H
#define AUTOPROXY_PUBLIC_H

class URL_LoadHandler;
class URL_Rep;

URL_LoadHandler* CreateAutoProxyLoadHandler(URL_Rep *url_rep, MessageHandler *mh);
	/**< Creates a proxy-determining URL_LoadHandler for a particular URL.
	 *
	 * @param url_rep (in) The URL_Rep for which to determine a proxy, if any
	 * @param mh (in) A message handler that receives notifications of the
	 *           results of the proxy computation
	 * @return A load handler, or NULL on OOM 
	 */

BOOL ReleaseAutoProxyResources();
	/**< Cleans up any global resources held onto by the proxyconf code.
	 *
	 * Can be called repeatedly, eg, OOM handling can call this after stopping
	 * loading to release global resources.
	 *
	 * @return TRUE if resources could be released; FALSE if they were not allocated
	 *         or could not be released because they were in use.
	 */

#endif // AUTOPROXY_PUBLIC_H

/* eof */
