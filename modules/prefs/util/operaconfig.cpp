/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef OPERACONFIG_URL

#include "modules/util/opstring.h"
#include "modules/util/opfile/opfile.h"

#include "modules/prefs/prefsapitypes.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#include "modules/prefs/util/operaconfig.h"

#include "modules/pi/ui/OpUiInfo.h"

#ifdef _KIOSK_MANAGER_
# include "adjunct/quick/managers/KioskManager.h"
# include "adjunct/quick/Application.h"
#endif

OperaConfig::~OperaConfig()
{
	OP_DELETEA(m_settings);
}

OP_STATUS OperaConfig::GenerateData()
{
	// Check if opera:config support is disabled
	if (!g_pccore->GetIntegerPref(PrefsCollectionCore::OperaConfigEnabled))
		return OpStatus::ERR_NOT_SUPPORTED;

	// If running in kiosk mode and opera:config is not disabled, at least
	// prohibit the user from changing anything.
#ifdef _KIOSK_MANAGER_
	BOOL disable_settings = KioskManager::GetInstance()->GetEnabled();
#else
	static const BOOL disable_settings = FALSE;
#endif

	unsigned int len;
	OP_STATUS rc;
	TRAP_AND_RETURN_VALUE_IF_ERROR(rc, m_settings = g_prefsManager->GetPreferencesL(TRUE, len), rc);

	OpString line;

	// Set up document and create the HEAD section
#ifdef _LOCALHOST_SUPPORT_
	OpenDocument(Str::S_CONFIG_TITLE, PrefsCollectionFiles::StyleConfigFile);
#else
	OpenDocument(Str::S_CONFIG_TITLE);
#endif

	// Add the script magic
	// Author: Tarquin Wilton-Jones 
	OpString settings_saved, no_settings_saved, need_restart;
	RETURN_IF_ERROR(JSSafeLocaleString(&settings_saved, Str::S_SETTINGS_SAVED));
	RETURN_IF_ERROR(JSSafeLocaleString(&no_settings_saved, Str::S_NO_SETTINGS_SAVED));
	RETURN_IF_ERROR(JSSafeLocaleString(&need_restart, Str::S_CONFIG_RESTART));
#ifndef USE_MINIMAL_OPERA_CONFIG
	OpString no_information;
	RETURN_IF_ERROR(JSSafeLocaleString(&no_information, Str::S_CONFIG_NO_INFORMATION));
#endif

	line.Append(" <script type=\"text/javascript\">\n"
				/* Reset a preference to default.
				 * Parameters:
				 *   t = The button pressed, i = id for the fieldset
				 */
				"function d(t,n)\n"
				"{\n"
				" var f=document.getElementById('s'+n);\n"
				" var s=f.getElementsByTagName('legend')[0].firstChild.nodeValue;\n"
				" var k=t.parentNode.parentNode.getElementsByTagName('label')[0].firstChild.nodeValue;\n"
				" var i=t.parentNode.getElementsByTagName('input')[0];\n"
				" if(i.type=='checkbox')\n"
				" {\n"
				"  i.checked=(opera.getPreferenceDefault(s,k)=='1');\n"
				" }\n"
				" else if(i.type=='file')\n"
				" {\n"
				"  i.value=\"\\\"\"+opera.getPreferenceDefault(s,k)+\"\\\"\";\n"
				" }\n"
				" else\n"
				" {\n"
				"  i.value=opera.getPreferenceDefault(s,k);\n"
				" }\n"
				"}\n"

				/* Submit or reset a fieldset.
				 * Parameters:
				 *   t = The button pressed, a = action code
				 * Actions:
				 *   0 = reset, 1 = save
				 */
				"function o(t,a)\n"
				"{\n"
#ifdef _KIOSK_MANAGER_
				     );
	if (!disable_settings) { line.Append(
#endif
				" var f=t.parentNode.parentNode;\n"
				" var s=f.getElementsByTagName('legend')[0].firstChild.nodeValue;\n"
				" var i=f.getElementsByTagName('input');\n"
				" var isd=f.className.match(/\\bdirty\\b/);\n"
				" for(var n=0,m,us='';m=i[n];n++)\n"
				" {\n"
				"  var k=m.parentNode.parentNode.getElementsByTagName('label')[0].firstChild.nodeValue;\n"
				"  if(a)\n"
				"  {\n"
				"   var v=(m.type=='checkbox')?(m.checked?'1':'0'):m.value;\n"
				"   if((!isd||(m.parentNode.parentNode.className=='wasmatch'))&&(v!=opera.getPreference(s,k))"
				"&&(((m.type=='file')&&!m.hasAttribute('required'))||m.checkValidity()))\n"
				"   {\n"
				"    opera.setPreference(s,k,v);\n"
				"    us+='\\n'+k;\n"
				"   }\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"   if(m.type=='checkbox')\n"
				"   {\n"
				"    m.checked=(opera.getPreference(s,k)!='0');\n"
				"   }\n"
				"   else\n"
				"   {\n"
				"    m.value=opera.getPreference(s,k);\n"
				"   }\n"
				"  }\n"
				" }\n"
				" if(a)\n"
				" {\n"
				"  if(us!='')\n"
				"  {\n"
				"   opera.commitPreferences();\n"
				"   alert(\"");
	line.Append(            settings_saved);
	line.Append(                         "\\n\"+us+\"\\n\\n");
	line.Append(                                           need_restart);
	line.Append(                                                      "\");\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"   alert(s+\"\\n");
	line.Append(              no_settings_saved);
	line.Append(                              "\");\n"
				"  }\n"
				" }\n"
#ifdef _KIOSK_MANAGER_
				      );
	}
	line.Append(
#endif
				"}\n"


				/* Magic for making stuff appear or disappear
				 */
				"window.addEventListener(\n"
				" 'load',\n"
				" function()\n"
				" {\n"
				"  var il={};\n"
				"  document.getElementById('showall').parentNode.className='hasscript';\n"
				"  var al=document.getElementsByTagName('legend');\n"
				"  for(var i=0,j;j=al[i];i++)\n"
				"  {\n"
				"   j.parentNode.className='notexpanded';\n"
				"   il[j.firstChild.nodeValue.replace(/\\s+/g,'').toLowerCase()]=j;\n"
				"   j.addEventListener('click',function()\n"
				"   {\n"
				"    if(this.parentNode.anyhits&&!this.parentNode.className.match(/\\bdirty\\b/))\n"
				"    {\n"
				"     this.parentNode.className='dirty expanded';\n"
				"     return;\n"
				"    }\n"
				"    this.parentNode.className=(this.parentNode.className=='expanded')?'notexpanded':'expanded';\n"
				"    document.getElementById('showall').getElementsByTagName('input')[0].checked=false;\n"
				"   window.chkbxtck=false;\n"
				"   },false);\n"
				"  }\n"
				"  var tb=document.getElementById('showall').getElementsByTagName('input')[0];\n"
				"  tb.addEventListener('click',function()\n"
				"  {\n"
				"   window.chkbxtck=this.checked;\n"
				"   var sf=document.getElementById('searchbox').getElementsByTagName('input')[0];\n"
				"   var flds=document.getElementsByTagName('fieldset');\n"
				"   sf.value=sf.defaultValue;\n"
				"   for(var i=0,j;j=flds[i];i++)\n"
				"   {\n"
				"    j.className=this.checked?'expanded':'notexpanded';\n"
				"    j.anyhits=false;\n"
				"   }\n"
				"  },false);\n"
				"  var sf=document.getElementById('searchbox').getElementsByTagName('input')[0];\n"
				"  sf.addEventListener('focus',function()\n"
				"  {\n"
				"   if(this.value==this.defaultValue)\n"
				"   {\n"
				"    this.value='';\n"
				"   }\n"
				"  },false);\n"
				"  sf.addEventListener('blur',function()\n"
				"  {\n"
				"   if(!this.value)\n"
				"   {\n"
				"    this.value=this.defaultValue;\n"
				"   }\n"
				"  },false);\n"
				// Perform the actual search
				"  function s(e)\n"
				"  {\n"
				"   if((e.type=='keyup')&&(this!=kwd))\n"
				"   {\n"
				"    return;\n"
				"   }\n"
				"   var tc=document.getElementById('showall').getElementsByTagName('input')[0];\n"
				"   if((e.type=='change')&&tc.checked&&!window.chkbxtck)\n"
				"   {\n"
				"    return;\n"
				"   }\n"
				"   window.chkbxtck=false;\n"
				"   tc.checked=false;\n"
				"   tc.disabled=true;\n"
				"   var flds=document.getElementsByTagName('fieldset');\n"
				"   var lc=this.value.toLowerCase();\n"
				"   for(var i=0,j,k=false;j=flds[i];i++)\n"
				"   {\n"
				"    j.anyhits=false;\n"
				"    if(lc&&(j.getElementsByTagName('legend')[0].firstChild.nodeValue.toLowerCase().indexOf(lc)+1))\n"
				"    {\n"
				"     j.anyhits=true;\n"
				"    }\n"
				"    var rows=j.getElementsByTagName('tr');\n"
				"    for(var n=0,m;m=rows[n];n++)\n"
				"    {\n"
				"     var val1=m.cells[0].firstChild.firstChild.nodeValue+'\\n' +\n"
				"       m.cells[1].getElementsByTagName('input')[0].value;\n"
				"     if(lc&&(val1.toLowerCase().indexOf(lc)+1))\n"
				"     {\n"
				"      if(!m.className||m.className=='match')\n"
				"      {\n"
				"       m.className='wasmatch';\n"
				"      }\n"
				"      j.anyhits=true;\n"
				"     }\n"
				"     else\n"
				"     {\n"
				"      if(m.className)\n"
				"      {\n"
				"       m.className='';\n"
				"      }\n"
				"     }\n"
				"    }\n"
				"    if(j.anyhits)\n"
				"    {\n"
				"     k=true;\n"
				"    }\n"
				"    var nv=lc?(j.anyhits?'dirty expanded':'dirty'):'notexpanded';\n"
				"    if(nv!=j.className)\n"
				"    {\n"
				"     j.className=nv;\n"
				"    }\n"
				"    if(!lc)\n"
				"    {\n"
				"     j.anyhits = false;\n"
				"    }\n"
				"   }\n"
				"   document.getElementById('nohits').className=(k||!lc)?'':'nonefound';\n"
				"   tc.disabled=false;\n"
				"  }\n"
				"  var kwd;\n"
				"  document.addEventListener('keydown',function(e){kwd=e.target;},false);\n"
				"  sf.addEventListener('keyup',s,false);\n"
				"  sf.addEventListener('change',s,false);\n"
				"  if(location.hash)\n"
				"  {\n"
				"   var sf=location.hash.replace(/^#/,'');\n"
				"   var tf=sf.split('|');\n"
				"   if(tf[0])\n"
				"   {\n"
				"    tf[0]=unescape(tf[0]).toLowerCase().replace(/\\s+/g,'');\n"
				"   }\n"
				"   if(il[tf[0]])\n"
				"   {\n"
				"    var oe=document.createEvent('MouseEvents');\n"
				"    oe.initMouseEvent('click',false,true,window,0,0,0,0,0,false,false,false,false,0,il[tf[0]]);\n"
				"    il[tf[0]].dispatchEvent(oe);\n"
				"    var st=il[tf[0]];\n"
				"    if(tf[1])\n"
				"    {\n"
				"     tf[1]=unescape(tf[1]).toLowerCase().replace(/\\s+/g,'');\n"
				"     var ar=il[tf[0]].parentNode.getElementsByTagName('label');\n"
				"     for(var ro=0,ra;ra=ar[ro];ro++)\n"
				"     {\n"
				"      if(ra.firstChild.nodeValue.replace(/\\s+/g,'').toLowerCase()==tf[1])\n"
				"      {\n"
				"       st=ra.parentNode;\n"
				"       ra.parentNode.parentNode.getElementsByTagName('input')[0].focus();\n"
				"       ra.parentNode.parentNode.className='match';\n"
				"       break;\n"
				"      }\n"
				"     }\n"
				"    }\n"
				"    var posl=0;\n"
				"    while(st)\n"
				"    {\n"
				"     posl+=st.offsetTop;\n"
				"     st=st.offsetParent;\n"
				"    }\n"
				"    window.scrollTo(0,posl-4);\n"
				"   }\n"
				"   else\n"
				"   {\n"
				"    var sb=document.getElementById('searchbox').getElementsByTagName('input')[0];\n"
				"    sb.value=unescape(sf);\n"
				"    sb.focus();\n"
				"    var oe=document.createEvent('HTMLEvents');\n"
				"    oe.initEvent('change',false,true);\n"
				"    sb.dispatchEvent(oe);\n"
				"   }\n"
				"  }\n"
				" },\n"
				" false\n"
				");\n"
#ifndef USE_MINIMAL_OPERA_CONFIG
				/* Scroll-to-position for internal links
				 */
				"function sct(ln)\n"
				"{\n"
				" var lps=0,sln=ln;\n"
				" ln=ln.parentNode;\n"
				" while(ln)\n"
				" {\n"
				"  lps+=ln.offsetTop;\n"
				"  ln=ln.offsetParent;\n"
				" }\n"
				" window.scrollTo(0,lps-4);\n"
				" sln.parentNode.parentNode.getElementsByTagName('input')[0].focus();\n"
				"};\n"

				"var xhr=new XMLHttpRequest(),queued=[],helpdisp;\n"
				"function hlp(oLink,n)\n"
				"{\n"
				" var lps,ln=oLink.parentNode;\n"
				" if(!helpdisp)\n"
				" {\n"
				"  helpdisp=document.getElementById('helpdisp');\n"
				" }\n"
				" queued=[\n"
				"  document.getElementById('s'+n).getElementsByTagName('legend')[0].firstChild.nodeValue,\n"
				"  oLink.parentNode.parentNode.getElementsByTagName('label')[0].firstChild.nodeValue,\n"
				"  oLink.parentNode.parentNode\n"
				" ];\n"
				" lps=ln.offsetHeight;\n"
				" while(ln)\n"
				" {\n"
				"  lps+=ln.offsetTop;\n"
				"  ln=ln.offsetParent;\n"
				" }\n"
				" helpdisp.style.top=lps+'px';\n"
				" helpdisp.className='showtip';\n"
				" if(xhr.readyState==0)\n"
				" {\n"
				" 	xhr.onreadystatechange=function()\n"
				" 	{\n"
				" 	 if(this.readyState==4)\n"
				" 	 {\n"
				" 	  proq(this.responseXML);\n"
				" 	 }\n"
				" 	};\n"
				"  xhr.open('GET','http://help.opera.com/operaconfig/?version='+encodeURIComponent(opera.version())+'&opsys='+encodeURIComponent(navigator.platform)+'&lang='+encodeURIComponent(navigator.language),true);\n"
				"  xhr.send(null);\n"
				" }\n"
				" else if(xhr.readyState==4)\n"
				" {\n"
				"  proq(xhr.responseXML);\n"
				" }\n"
				" return false;\n"
				"}\n"
				"function proq(xhr)\n"
				"{\n"
				" if(xhr)\n"
				" {\n"
				"  var p=xhr.evaluate('//x:caption[normalize-space(text())=\\'['+queued[0]+']\\']/parent::x:table//*[normalize-space(text())=\\''+queued[1]+'\\']/ancestor::x:tr/x:td[position()=2]',xhr,function (a) { return 'http://www.w3.org/1999/xhtml'; },XPathResult.FIRST_ORDERED_NODE_TYPE,null).singleNodeValue;\n"
				"  if(p)\n"
				"  {\n"
				"   while(helpdisp.lastChild.firstChild)\n"
				"   {\n"
				"    helpdisp.lastChild.removeChild(helpdisp.lastChild.firstChild);\n"
				"   }\n"
				"   for(var i=0;i<p.childNodes.length;i++)\n"
				"   {\n"
				"    helpdisp.lastChild.appendChild(cpy(p.childNodes[i]));\n"
				"   }\n"
				"   return;\n"
				"  }\n"
				" }\n"
				" helpdisp.lastChild.textContent='");
	line.Append(no_information);
	line.Append("';\n"
				"}\n"
				"function clhlp()\n"
				"{\n"
				" if(helpdisp)\n"
				" {\n"
				"  helpdisp.className='';\n"
				" }\n"
				"}\n"
				"function cpy(n)\n"
				"{\n"
				" var l;\n"
				" if(n.nodeType==1)\n"
				" {\n"
				"  if(n.tagName.match(/^(a|address|b|bdo|blockquote|br|cite|code|dd|del|dfn|div|dl|dt|em|hr|i|ins|kbd|li|ol|p|pre|q|samp|span|strong|sub|sup|ul|var)$/i))\n"
				"  {\n"
				"   l=document.createElement(n.tagName);\n"
				"   if(n.tagName.toLowerCase()=='a'&&n.hasAttribute('href')&&!n.getAttribute('href').match(/^(#|(opera|javascript|mailto|news|nntp|gopher|wais):)/))\n"
				"   {\n"
				"    l.setAttribute('href',(n.getAttribute('href').match(/^(https?|ftp):\\/\\//)?'':'http://www.opera.com')+n.getAttribute('href'));\n"
				"    l.setAttribute('target','_blank');\n"
				"   }\n"
				"  }\n"
				"  else\n"
				"  {\n"
				"   l=document.createDocumentFragment();\n"
				"  }\n"
				"  for(var i=0;i<n.childNodes.length;i++)\n"
				"  {\n"
				"   l.appendChild(cpy(n.childNodes[i]));\n"
				"  }\n"
				"  return l;\n"
				" }\n"
				" if(n.nodeType==3)\n"
				" {\n"
				"  return document.createTextNode(n.nodeValue);\n"
				" }\n"
				" return document.createTextNode('');\n"
				"}\n"
				"Node.prototype.hasPart = function (l)\n"
				"{\n"
				" while(l)\n"
				" {\n"
				"  if(l==this)\n"
				"  {\n"
				"   return true;\n"
				"  }\n"
				"  l=l.parentNode;\n"
				" }\n"
				" return false;\n"
				"};\n"
				"function acl(e)\n"
				"{\n"
				" if(!e.target||(e.type!='resize'&&e.target==document)||(e.type=='focus'&&(e.target==document.documentElement||e.target==document.body))||(queued[2]&&queued[2].hasPart(e.target))||(helpdisp&&helpdisp.hasPart(e.target)))\n"
				" {\n"
				"  return;\n"
				" }\n"
				" clhlp();\n"
				"}\n"
				"document.addEventListener('resize',acl,true);\n"
				"document.addEventListener('focus',acl,true);\n"
				"document.addEventListener('click',acl,true);\n"
#endif // !USE_MINIMAL_OPERA_CONFIG
				" </script>\n"
				"</head>\n"
				"<body id=\"opera-config\">\n"
				"<h1>");
	AppendLocaleString(&line, Str::S_CONFIG_TITLE);
	line.Append("</h1>\n");
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	line.Set("<div>\n"
				" <p id=\"help\">\n"
				"  <a href=\""
#if defined _NO_HELP_ || defined PREFS_OPERACONFIG_EXTERNAL_HELP
				"http://redir.opera.com/www.opera.com/support/usingopera/operaini/"
#else
				"opera:/help/config.html"
#endif
				"\" target=\"_blank\">");
	AppendLocaleString(&line, Str::DI_IDHELP);
	line.Append("</a>\n"
	            " </p>\n"
				" <p id=\"showall\">\n"
				"  <label><input type=\"checkbox\"> ");
	AppendLocaleString(&line, Str::S_CONFIG_SHOW_ALL);
	line.Append("</label>\n"
	            " </p>\n"
				" <p id=\"searchbox\">\n"
				"  <label><input type=\"text\" value=\"");
	AppendLocaleString(&line, Str::S_QUICK_FIND);
	line.Append("\"></label>\n"
				" </p>\n"
				"</div>\n");
	
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
	
	// Captions for buttons; these are re-used, so only get them once
	OpString save_button, reset_button, defaults_button;
	g_languageManager->GetString(Str::SI_SAVE_BUTTON_TEXT, save_button);
	g_languageManager->GetString(Str::SI_IDFORM_RESET, reset_button);
	g_languageManager->GetString(Str::S_CONFIG_DEFAULT, defaults_button);

	// We need log(256)/log(10) * sizeof(value) characters to store
	// the number, adding one for the rounding, one for the sign
	// and one for the terminator. 53/22 is a good approximation
	// of this.
	static const int formidlen = 3 + sizeof (int) * 53 / 22;
	char secid[formidlen];  /* ARRAY OK 2009-04-23 adame */
	char formid[formidlen]; /* ARRAY OK 2009-04-23 adame */

	// Iterate over the preferences to generate the big list of settings
	const char *cursection = NULL;
	BOOL ignore = FALSE, hasenabled = FALSE;
	int formnum = 0, secnum = 0;
	for (unsigned int i = 0; i < len; ++ i)
	{
		if (!i || op_strcmp(cursection, m_settings[i].section))
		{
			cursection = m_settings[i].section;
			// Ignore certain sections:
			//  RM* (ERA settings not publically known)
			//  State (these are state information, not settings)
			//  Opera Sync Server (policy decision)
			if ((ignore = ((cursection[0] == 'R' && cursection[1] == 'M')
#if defined PREFS_HAVE_DESKTOP_UI || defined PREFS_HAVE_UNIX_PLUGIN
						   || !op_strcmp(cursection, "State")
#endif
#ifdef SUPPORT_DATA_SYNC
						   || !op_strcmp(cursection, "Opera Sync Server")
#endif
						  )))
			{
				continue;
			}

			// Id number for this section
			op_itoa(secnum ++, secid, 10);

			// Start a new fieldset for this section
			line.Set("<fieldset id=\"s");
			line.Append(secid);
			line.Append("\">\n <legend>");
			line.Append(m_settings[i].section);
			line.Append("</legend>\n <table>\n");
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

			hasenabled = FALSE;
		}

		// Skip entry if we're in an ignored section
		if (ignore)
		{
			continue;
		}

		// Id number for this key
		op_itoa(formnum ++, formid, 10);

		line.Set("  <tr>\n   <th><label for=\"f");
		line.Append(formid);
		line.Append("\">");
		line.Append(m_settings[i].key);
		line.Append("</label></th>\n   <td>\n    <input type=\"");

		// Flags
		BOOL required = FALSE;

		// Bug#196702: File names should be quoted, to be consistent with
		// what Opera does when you select a value in the file selector.
		BOOL quote_value = FALSE;
		
		switch (m_settings[i].type)
		{
		default:
			OP_ASSERT(!"Unknown data type");
			// FALLTHROUGH
		case prefssetting::string:
			line.Append("text\" class=\"string");
			break;

		case prefssetting::directory:
			line.Append("text\" class=\"directory");
			break;

		case prefssetting::integer:
			line.Append("number\" class=\"integer\" step=\"1");
			break;

		case prefssetting::boolean:
			line.Append("checkbox");
			break;

		case prefssetting::requiredfile:
			required = TRUE;
		case prefssetting::file:
			quote_value = TRUE;
			line.Append("file\" class=\"file");
			break;

		case prefssetting::color:
			line.Append("color\" class=\"color");
			break;
		}

		if (m_settings[i].type == prefssetting::boolean)
		{
			// Checkbox
			if (m_settings[i].value.CStr()[0] != '0')
			{
				line.Append("\" checked=\"checked");
			}
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
			line.Empty();
		}
		else
		{
			// Normal value
			line.Append("\" value=\"");
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
			line.Empty();
			if (quote_value)
			{
				line.Append("&quot;"); // Gets added both before and after
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
			}
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, m_settings[i].value));
		}

		line.Append("\" id=\"f");
		line.Append(formid);

		if (required)
		{
			line.Append("\" required=\"required");
		}

		if (disable_settings
# ifdef PREFSFILE_CASCADE
		    || !m_settings[i].enabled
# endif
		   )
		{
			line.Append("\" disabled=\"disabled\">\n");
		}
		else
		{
			line.Append("\">\n    <button type=\"button\" onclick=\"d(this,");
			line.Append(secid);
			line.Append(")\">");
			line.Append(defaults_button);
			line.Append("</button>\n");

			hasenabled = TRUE;
		}

#ifndef USE_MINIMAL_OPERA_CONFIG
		// Scroll-to-position link
		line.Append("    <a href=\"#");
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

		// Kill all spaces in the link anchor name
		line.Set(m_settings[i].section);
		line.Append("|");
		line.Append(m_settings[i].key);
		uni_char *source = line.CStr(), *dest = source;
		do
		{
			if (*source != ' ')
			{
				*(dest ++) = *source;
			}
			++ source;
		} while(*source);
		*dest = 0;
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

		line.Set("\" onclick=\"sct(this);\">\xBB</a>\n");
		line.Append("<a href=\"#\" onclick=\"return hlp(this,");
		line.Append(secid);
		line.Append(")\">?</a>\n");
#endif // !USE_MINIMAL_OPERA_CONFIG
		line.Append("</td>\n  </tr>\n");
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

		if (i == len - 1 || op_strcmp(cursection, m_settings[i + 1].section))
		{
			// Add OK and Reset button for section. On some platforms,
			// the OK button should be on the right and on some it should
			// be on the left.

			if (g_op_ui_info->DefaultButtonOnRight())
			{
				line.Set(" </table>\n"
						" <div>\n"
						"  <button type=\"button\" onclick=\"o(this,0)\">");
				line.Append(reset_button);
				line.Append("</button>\n"
							"  <button type=\"button\" onclick=\"o(this,1)\"");
				if (hasenabled)
					line.Append(">");
				else
					line.Append(" disabled=\"disabled\">");
				line.Append(save_button);
				line.Append("</button>\n"
							" </div>\n"
							"</fieldset>\n");
			}
			else
			{
				line.Set(" </table>\n"
						" <div>\n"
						"  <button type=\"button\" onclick=\"o(this,1)\"");
				if (hasenabled)
					line.Append(">");
				else
					line.Append(" disabled=\"disabled\">");
				line.Append(save_button);
				line.Append("</button>\n"
							"  <button type=\"button\" onclick=\"o(this,0)\">");
				line.Append(reset_button);
				line.Append("</button>\n"
							" </div>\n"
							"</fieldset>\n");
			}

			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
		}
	}

	// String to display if search fails
	line.Set("<p id=\"nohits\">");
	AppendLocaleString(&line, Str::S_NO_MATCHES_FOUND);
	line.Append("</p>\n");
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

#ifndef USE_MINIMAL_OPERA_CONFIG
	line.Set("<div id=\"helpdisp\"><div><a href=\"#\" onclick=\"clhlp();return false;\">X</a></div><div>");
	AppendLocaleString(&line, Str::S_CONFIG_LOADING);
	line.Append("</div></div>");
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
#endif // !USE_MINIMAL_OPERA_CONFIG

	// Finish the document and close the document
	return CloseDocument();
}

#endif // OPERACONFIG_URL
