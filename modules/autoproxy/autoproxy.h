/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA 2001-2004.
 */

#ifndef AUTOPROXY_H
#define AUTOPROXY_H

#include "modules/url/url2.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/url/loadhandler/url_lh.h"

/** Encoding of a suspended computation. */
class JSProxyPendingContext
{
public:
	JSProxyPendingContext( ES_Runtime*, ES_Context*, Comm*, BOOL );
		/**<Create a suspended execution state. */

	~JSProxyPendingContext();
		/**<Destroy a suspended execution state, releasing the context (but
	        not the runtime). */

	ES_Runtime  *runtime;
	ES_Context	*context;			///< Context containing suspension
	Comm		*comm;				///< Output: Non-0 if DNS lookup, 0 if timeout
	BOOL		lookup_succeeded;	///< Input: TRUE if lookup succeeded
};


/** Main interface class to the autoproxy code. */
class URL_AutoProxyConfig_LoadHandler : public URL_LoadHandler
{
private:
	JSProxyPendingContext *pending_request;

private:
	OP_STATUS GetScriptURL(URL& resolved_url);
	CommState ExecuteScript();
	void DisableProxyAndFail(int msg);
	BOOL3 MaybeLoadProxyScript();

public:
	URL_AutoProxyConfig_LoadHandler(URL_Rep *url_rep, MessageHandler *mh);

	virtual void      HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual CommState Load();
	virtual void      ProcessReceivedData();
	virtual unsigned  ReadData(char *buffer, unsigned buffer_len);
	virtual void      EndLoading(BOOL force=FALSE);
};


/** ECMAScript function object for the dnsResolve() function. */
class ProxyConfigDnsResolve : public EcmaScript_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};


/** ECMAScript function object for the myHostname() function. */
class ProxyConfigMyHostname : public EcmaScript_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};


/** ECMAScript function object for the shExpMatch() function. */
class ProxyConfigShExpMatch : public EcmaScript_Object
{
private:
	BOOL		ShExpMatch( const uni_char *str, const uni_char *pat );
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};


#ifdef SELFTEST
/** ECMAScript function object for the debugAPC() function, used only
  * in SELFTEST builds.
  */
class DebugAPC : public EcmaScript_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);

	static void LogNumberOfLoads();
};
#endif // SELFTEST


/** General context for proxy computation. */
class JSProxyConfig 
{
public:
	static JSProxyConfig* Create( URL &url, OP_STATUS& stat );
		/**< Constructor when script has to be read from a URL.  
	         Returns NULL on error, with ERR (script error) or ERR_NO_MEMORY. */

	static JSProxyConfig*	Create( const ES_ProgramText *source, int elts, OP_STATUS& stat, URL* srcurl );
		/**< Constructor when script is available as program text.
			 'srcurl', if not NULL, can be used for error reporting.
			 Returns NULL on error, with ERR (script error) or ERR_NO_MEMORY. */

	static void Destroy(JSProxyConfig* apc);
		/**< Destructor.  Frees all resources owned by the object.  APC may be NULL. */

	OP_STATUS FindProxyForURL( const uni_char *url, const uni_char *host, uni_char **result, JSProxyPendingContext **pending );
		/**< Start proxy lookup.
		  *
		  * @param url (in) Pointer to a string encoding the URL to look up.  Caller
		  *            retains ownership and can delete it.
		  * @param host (in) Pointer to a string encoding the host part of that URL.  
		  *            Caller retains ownership and can delete it.
		  * @param result (out) Location that receives pointer to heap-allocated character 
		  *               string.  NULL unless the call succeeds.  Caller receives ownership
		  *               of the string.
		  * @param pending (out) Location that receives pointer to heap-allocated suspension
		  *                state.  NULL unless the call suspends.  Caller receives ownership
		  *                of the state.
		  * @return OpStatus::OK on result or suspension, some error code otherwise.
		  */
	
	OP_STATUS FindProxyForURL( JSProxyPendingContext *state, uni_char **result);
		/**< Continue suspended proxy lookup.
		  *
		  * @param state (in/out) Pointer to suspension state.  If call suspends again it 
		  *              may be updated.  Caller retains ownership even if call does
		  *              not suspend again.
		  * @param result (out) Location that receives pointer to heap-allocated character 
		  *               string.  NULL unless the call succeeds.  Caller receives ownership
		  *               of the string.
		  * @return OpStatus::OK on result or suspension, some error code otherwise.
		  */

public:
	// These variables are only used inside the dynamic scope of FindProxyForURL.
	Comm		*pending_comm_object;	///< Non-0 if suspended to do name lookup
	BOOL		is_dns_retry;			///< TRUE if we're reentering after dns suspend

private:
	/** Return status from call to JSProxyConfig::ExecuteProgram. */
	enum APC_Status
	{
		APC_Error,		///< Program failed, and did not return a value
		APC_OOM,		///< Program failed due to OOM, and did not return a value
		APC_Suspend,	///< Program did not complete, may be waiting for network
		APC_OK			///< Program finished, and returned a value
	};

	/** Behavior control for JSProxyConfig::ExecuteProgram */
	enum SuspensionBehavior 
	{
		NO_SUSPEND,		///< Execution must not suspend; allow it to run for a while
		ALLOW_SUSPEND	///< Execution may suspend when a timeslice is exhausted or for other reasons
	};

	JSProxyConfig();
	~JSProxyConfig();

	void InitializeHostObjectsL();
		/**< Install function objects in the global environment. */

	APC_Status ExecuteProgram( ES_Context *context, uni_char **result, SuspensionBehavior sb );
		/**< Run the computation in the context to yield a result, or until it suspends or fails.
		  *
		  * @param context (in) The computation
		  * @param result (out) Location that receives pointer to heap-allocated character 
		  *               string.  NULL unless the call succeeds.  Caller receives ownership
		  *               of the string.
		  * @param sb (in) ALLOW_SUSPEND iff suspension is allowed; if not, execution will be
		  *                cut off after a fairly large number of steps and an error code
		  *                returned.
		  * @return Status code for the computation.
		  */

public:
	// This function is not part of public API, though it's defined
	// in "public" section. It's a callback function that needs
	// to be provided to ES_Runtime::ExecuteContext().
	// It is defined here for grouping purposes, but can be detached
	// from the class if needed.
	static BOOL ESRuntimeOutOfTime(void* abort_time);

private:
	ES_Runtime				*runtime;		///< ECMAScript runtime used for all evaluations
	double					js_abort_time;	///< Interrupt after this time
};

struct AutoProxyGlobals
{
	AutoProxyGlobals()
		: proxycfg(NULL)
		, pending_requests(0)
		, settings_have_changed(FALSE)
		, current_implementation(NULL)
#ifdef SELFTEST
		, number_of_loads_from_url(0)
#endif
	{
		unsigned i=0;
		proxy_conf_lib[i++] = UNI_L("function OPERA$init(){OPERA$daynumber={SUN:0,MON:1,TUE:2,WED:3,THU:4,FRI:5,SAT:6");
		proxy_conf_lib[i++] = UNI_L("};OPERA$monthnumber={JAN:0,FEB:1,MAR:2,APR:3,MAY:4,JUN:5,JUL:6,AUG:7,SEP:8,OCT:9");
		proxy_conf_lib[i++] = UNI_L(",NOV:10,DEC:11};ProxyConfig=new Object();ProxyConfig.bindings=new Array();OPERA$");
		proxy_conf_lib[i++] = UNI_L("init=undefined;}function OPERA$getCurrentTime(use_UTC){var now=new Date();if(use");
		proxy_conf_lib[i++] = UNI_L("_UTC)return{day:now.getUTCDay(),date:now.getUTCDate(),month:now.getUTCMonth(),ye");
		proxy_conf_lib[i++] = UNI_L("ar:now.getUTCFullYear(),hour:now.getUTCHours(),min:now.getUTCMinutes(),sec:now.g");
		proxy_conf_lib[i++] = UNI_L("etUTCSeconds()};else return{day:now.getDay(),date:now.getDate(),month:now.getMon");
		proxy_conf_lib[i++] = UNI_L("th(),year:now.getFullYear(),hour:now.getHours(),min:now.getMinutes(),sec:now.get");
		proxy_conf_lib[i++] = UNI_L("Seconds()};}function isPlainHostName(host){return host.indexOf('.')==-1;}functio");
		proxy_conf_lib[i++] = UNI_L("n dnsDomainIs(host,domain){var lh=host.length;var ld=domain.length;return lh>=ld");
		proxy_conf_lib[i++] = UNI_L("&&host.substring(lh-ld)==domain;}function localHostOrDomainIs(host,hostdom){if(h");
		proxy_conf_lib[i++] = UNI_L("ost==hostdom)return true;if(host.indexOf('.')!=-1)return false;lh=host.length;lh");
		proxy_conf_lib[i++] = UNI_L("d=hostdom.length;return lh<=lhd&&host==hostdom.substring(0,lh);}function isResol");
		proxy_conf_lib[i++] = UNI_L("vable(host){if(dnsResolve(host))return true;else return false;}function isInNet(");
		proxy_conf_lib[i++] = UNI_L("host,pattern,mask){if(!host.match(/^\\\\d+\\\\.\\\\d+\\\\.\\\\d+\\\\.\\\\d+$/))host=dnsResolve");
		proxy_conf_lib[i++] = UNI_L("(host);if(!host)return false;h=host.split('.');p=pattern.split('.');m=mask.split");
		proxy_conf_lib[i++] = UNI_L("('.');for(i=0;i<4;i++)if((h[i]&m[i])!=(p[i]&m[i]))return false;return true;}func");
		proxy_conf_lib[i++] = UNI_L("tion dnsResolve(host){var result=undefined;while(result===undefined)result=OPERA");
		proxy_conf_lib[i++] = UNI_L("$dnsResolve(host);return result;}function myIpAddress(){var attempt=dnsResolve(O");
		proxy_conf_lib[i++] = UNI_L("PERA$myHostname());return attempt?attempt:'127.0.0.1';}function dnsDomainLevels(");
		proxy_conf_lib[i++] = UNI_L("host){var n=0;var idx=0;while((idx=host.indexOf('.',idx))!=-1){n++;idx++;}return");
		proxy_conf_lib[i++] = UNI_L(" n;}function weekdayRange(){var wd1=false,wd2=false,GMT=false;switch(arguments.l");
		proxy_conf_lib[i++] = UNI_L("ength){case 1:wd1=arguments[0];break;case 2:wd1=arguments[0];if(arguments[1]=='G");
		proxy_conf_lib[i++] = UNI_L("MT')GMT=true;else wd2=arguments[1];break;case 3:if(arguments[2]!='GMT')return fa");
		proxy_conf_lib[i++] = UNI_L("lse;GMT=true;wd1=arguments[0];wd2=arguments[1];break;}var now=OPERA$getCurrentTi");
		proxy_conf_lib[i++] = UNI_L("me(GMT);if(!wd1||!wd1 in OPERA$daynumber)return false;wd1=OPERA$daynumber[wd1];i");
		proxy_conf_lib[i++] = UNI_L("f(!wd2||!wd2 in OPERA$daynumber)return now.day==wd1;wd2=OPERA$daynumber[wd2];if(");
		proxy_conf_lib[i++] = UNI_L("wd1<=wd2)return now.day>=wd1&&now.day<=wd2;else return now.day>=wd1||now.day<=wd");
		proxy_conf_lib[i++] = UNI_L("2;}function dateRange(){var timezone=false;function isDay(x){return typeof(x)=='");
		proxy_conf_lib[i++] = UNI_L("number'&&0<x&&x<=31&&isFinite(x)&&Math.floor(x)==x;}function isMonth(x){return x");
		proxy_conf_lib[i++] = UNI_L(" in OPERA$monthnumber;}function isYear(x){return typeof(x)=='number'&&x>31&&isFi");
		proxy_conf_lib[i++] = UNI_L("nite(x)&&Math.floor(x)==x;}function onearg(a){var now=OPERA$getCurrentTime(timez");
		proxy_conf_lib[i++] = UNI_L("one);if(isDay(a))return now.date==a;else if(isMonth(a))return now.month==OPERA$m");
		proxy_conf_lib[i++] = UNI_L("onthnumber[a];else if(isYear(a))return now.year==a;else return false;}function t");
		proxy_conf_lib[i++] = UNI_L("woarg(a,b){var now=OPERA$getCurrentTime(timezone);if(isDay(a)&&isDay(b))return a");
		proxy_conf_lib[i++] = UNI_L("<=b?(a<=now.date&&now.date<=b):(a<=now.date||now.date<=b);else if(isMonth(a)&&is");
		proxy_conf_lib[i++] = UNI_L("Month(b)){var m1=OPERA$monthnumber[a],m2=OPERA$monthnumber[b];return m1<=m2?(m1<");
		proxy_conf_lib[i++] = UNI_L("=now.month&&now.month<=m2):(m1<=now.month||now.month<=m2);}else if(isYear(a)&&is");
		proxy_conf_lib[i++] = UNI_L("Year(b))return a<=now.year&&now.year<=b;else return false;}function fourarg(a,b,");
		proxy_conf_lib[i++] = UNI_L("c,d){var now=OPERA$getCurrentTime(timezone);if(isDay(a)&&isMonth(b)&&isDay(c)&&i");
		proxy_conf_lib[i++] = UNI_L("sMonth(d)){var d1=a,m1=OPERA$monthnumber[b],d2=c,m2=OPERA$monthnumber[d];if(m1<m");
		proxy_conf_lib[i++] = UNI_L("2||m1==m2&&d1<=d2)return(now.month>m1||now.month==m1&&now.date>=d1)&&(now.month<");
		proxy_conf_lib[i++] = UNI_L("m2||now.month==m2&&now.date<=d2);else return(now.month>m1||now.month==m1&&now.da");
		proxy_conf_lib[i++] = UNI_L("te>=d1)||(now.month<m2||now.month==m2&&now.date<=d2);}else if(isMonth(a)&&isYear");
		proxy_conf_lib[i++] = UNI_L("(b)&&isMonth(c)&&isYear(d)){var m1=OPERA$monthnumber[a],y1=b,m2=OPERA$monthnumbe");
		proxy_conf_lib[i++] = UNI_L("r[c],y2=d;return(now.year>y1||now.year==y1&&now.month>=m1)&&(now.year<y2||now.ye");
		proxy_conf_lib[i++] = UNI_L("ar==y2&&now.month<=m2);}else return false;}function sixarg(a,b,c,d,e,f){var now=");
		proxy_conf_lib[i++] = UNI_L("OPERA$getCurrentTime(timezone);if(isDay(a)&&isMonth(b)&&isYear(c)&&isDay(d)&&isM");
		proxy_conf_lib[i++] = UNI_L("onth(e)&&isYear(f)){var m1=OPERA$monthnumber[b],m2=OPERA$monthnumber[e];var d1=a");
		proxy_conf_lib[i++] = UNI_L(",d2=d;var y1=d,y2=f;return(now.year>y1||now.year==y1&&(now.month>m1||now.month==");
		proxy_conf_lib[i++] = UNI_L("m1&&now.date>=d1))&&(now.year<y1||now.year==y2&&(now.month<m2||now.month==m2&&no");
		proxy_conf_lib[i++] = UNI_L("w.date<=d2));}else return false;}switch(arguments.length){case 2:if(arguments[1]");
		proxy_conf_lib[i++] = UNI_L("!='GMT')return twoarg(arguments[0],arguments[1]);timezone=true;case 1:return one");
		proxy_conf_lib[i++] = UNI_L("arg(arguments[0]);case 3:if(arguments[2]!='GMT')return false;timezone=true;retur");
		proxy_conf_lib[i++] = UNI_L("n twoarg(arguments[0],arguments[1]);case 5:if(arguments[4]!='GMT')return false;t");
		proxy_conf_lib[i++] = UNI_L("imezone=true;case 4:return fourarg(arguments[0],arguments[1],arguments[2],argume");
		proxy_conf_lib[i++] = UNI_L("nts[3]);case 7:if(arguments[6]!='GMT')return false;timezone=true;case 6:return s");
		proxy_conf_lib[i++] = UNI_L("ixarg(arguments[0],arguments[1],arguments[2],arguments[3],arguments[4],arguments");
		proxy_conf_lib[i++] = UNI_L("[5]);default:return false;}}function timeRange(){var timezone=false;function isH");
		proxy_conf_lib[i++] = UNI_L("our(x){return typeof(x)=='number'&&0<=x&&x<24&&isFinite(x)&&Math.floor(x)==x;}fu");
		proxy_conf_lib[i++] = UNI_L("nction isMinSec(x){return typeof(x)=='number'&&0<=x&&x<60&&isFinite(x)&&Math.flo");
		proxy_conf_lib[i++] = UNI_L("or(x)==x;}function onearg(h){var now=OPERA$getCurrentTime(timezone);return isHou");
		proxy_conf_lib[i++] = UNI_L("r(h)&&h==now.hour;}function twoarg(h1,h2){var now=OPERA$getCurrentTime(timezone)");
		proxy_conf_lib[i++] = UNI_L(";if(isHour(h1)&&isHour(h2))return h1<=h2?now.hour>=h1&&now.hour<=h2:now.hour>=h1");
		proxy_conf_lib[i++] = UNI_L("||now.hour<=h2;else return false;}function fourarg(h1,m1,h2,m2){var now=OPERA$ge");
		proxy_conf_lib[i++] = UNI_L("tCurrentTime(timezone);if(isHour(h1)&&isMinSec(m1)&&isHour(h2)&&isMinSec(m2)){if");
		proxy_conf_lib[i++] = UNI_L("(h1<=h2)return(h1<now.hour||h1==now.hour&&m1<=now.min)&&(now.hour<h2||now.hour==");
		proxy_conf_lib[i++] = UNI_L("h2&&now.min<=m2);else return(h1<now.hour||h1==now.hour&&m1<=now.min)||(now.hour<");
		proxy_conf_lib[i++] = UNI_L("h2||now.hour==h2&&now.min<=m2);}else return false;}function sixarg(h1,m1,s1,h2,m");
		proxy_conf_lib[i++] = UNI_L("2,s2){var now=OPERA$getCurrentTime(timezone);var after_start,before_end;if(isHou");
		proxy_conf_lib[i++] = UNI_L("r(h1)&&isMinSec(m1)&&isMinSec(s1)&&isHour(h2)&&isMinSec(m2)&&isMinSec(s2)){if(h1");
		proxy_conf_lib[i++] = UNI_L("<=h2)return(h1<now.hour||h1==now.hour&&(m1<now.min||m1==now.min&&s1<=now.sec))&&");
		proxy_conf_lib[i++] = UNI_L("(now.hour<h2||now.hour==h2&&(now.min<m2||now.min==m2&&now.sec<=s2));else return(");
		proxy_conf_lib[i++] = UNI_L("h1<now.hour||h1==now.hour&&(m1<now.min||m1==now.min&&s1<=now.sec))||(now.hour<h2");
		proxy_conf_lib[i++] = UNI_L("||now.hour==h2&&(now.min<m2||now.min==m2&&now.sec<=s2));}else return false;}swit");
		proxy_conf_lib[i++] = UNI_L("ch(arguments.length){case 2:if(arguments[1]!='GMT')return twoarg(arguments[0],ar");
		proxy_conf_lib[i++] = UNI_L("guments[1]);timezone=true;case 1:return onearg(arguments[0]);case 3:if(arguments");
		proxy_conf_lib[i++] = UNI_L("[2]!='GMT')return false;timezone=true;return twoarg(arguments[0],arguments[1]);c");
		proxy_conf_lib[i++] = UNI_L("ase 5:if(arguments[4]!='GMT')return false;timezone=true;case 4:return fourarg(ar");
		proxy_conf_lib[i++] = UNI_L("guments[0],arguments[1],arguments[2],arguments[3]);case 7:if(arguments[6]!='GMT'");
		proxy_conf_lib[i++] = UNI_L(")return false;timezone=true;case 6:return sixarg(arguments[0],arguments[1],argum");
		proxy_conf_lib[i++] = UNI_L("ents[2],arguments[3],arguments[4],arguments[5]);default:return false;}}");
		proxy_conf_lib_length = i;
		OP_ASSERT(proxy_conf_lib_length == 82);
	}

	URL configuration_script_url;
		/**<If settings_have_changed==FALSE this is the URL for the script
			currently encoded in the proxycfg object.  Otherwise, it is the
			URL for the new script.  */

	JSProxyConfig *proxycfg;
		/**<This is a cached proxy config object, with a cached ES_Runtime.
			Caching it reduces the cost of running proxy configuration.
			
			It is not obviously correct to cache this object, because they
			share a runtime with global variables that may be changed by
			the script.  Thus the lookup for one script may affect the lookup
			for another script. */

	int pending_requests;
		/**<Counter for pending requests.  The cached proxy config object
			can only be deleted if this is 0. */

	BOOL settings_have_changed;
		/**<TRUE iff changed settings have been deteced but the changes have
			not yet been reflected in the proxycfg object. */

	JSProxyConfig *current_implementation;
		/**<Global context object.  Only used inside a call to FindProxyForURL;
			host functions called from the script can save state through the
			object this variable points to.	*/

	int number_of_loads_from_url;
		/**< Number of times a configuration script was loaded from a URL (and
			 not from an ES_ProgramText, say).  */

	const uni_char* proxy_conf_lib[82];
	/**<  * APC JavaScript library. Used in autoproxy_lib.cpp */

	unsigned int proxy_conf_lib_length;
	/**<  * Length of the proxy_conf_lib array. */
};

#endif // AUTOPROXY_H

/* eof */
