/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef OPERASPEEDDIAL_URL

#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/operaspeeddial.h"
#include "modules/bookmarks/speeddial_manager.h"

#include "modules/ecmascript_utils/esasyncif.h"

OP_STATUS OperaSpeedDialURLGenerator::QuickGenerate(URL &url, OpWindowCommander*)
{
	OperaSpeedDial document(url);
	return document.GenerateData();
}

OperaSpeedDial::OperaSpeedDial(URL url)
	: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
{
}

OperaSpeedDial::~OperaSpeedDial()
{
}

OP_STATUS OperaSpeedDial::GenerateData()
{
	RETURN_IF_ERROR(OpenDocument(Str::S_SPEED_DIAL, PrefsCollectionFiles::StyleSpeedDialFile));

	// Localized strings
	// FIXME!! Actually turn these into localized strings
	OpString add_text, enter_address_text, no_thumbnail_text;
	RETURN_IF_ERROR(add_text.Set(UNI_L("Click to add a Web page")));
	RETURN_IF_ERROR(enter_address_text.Set(UNI_L("Enter Web address here")));
	RETURN_IF_ERROR(no_thumbnail_text.Set(UNI_L("Failed to generate thumbnail")));

	OpString line;

	// Scripts
	RETURN_IF_ERROR(line.Append("<script type=\"text/javascript\">\n"));

#if defined SPEEDDIAL_DRAGNDROP || defined SPEEDDIAL_CONTEXT_MENU
	RETURN_IF_ERROR(line.Append(
		"function mc(ev) {\n"
		" return {\n"
		"  x:ev.clientX + document.body.scrollLeft - document.body.clientLeft,\n"
		"  y:ev.clientY + document.body.scrollTop - document.body.clientTop\n"
		" }\n"
		"}\n"
		));
#endif // defined SPEEDDIAL_DRAGNDROP || defined SPEEDDIAL_CONTEXT_MENU

#ifdef SPEEDDIAL_DRAGNDROP
	RETURN_IF_ERROR(line.Append(
		"var dragObject, dragIndex, dragOffset;\n"

		"function drag(ev, obj) {\n"
		" document.body.onmousedown=function(){return false};\n"		// Disable selection while dragging
		" dragIndex = parseInt(obj.id[1]);\n"
		" var dragSource = document.getElementsByClassName('thumbnail-cont')[dragIndex];\n"
		// Get the drag offset
		" var objPos    = pos(dragSource);\n"
		" var mousePos  = mc(ev);\n"
		" dragOffset =  {x:mousePos.x - objPos.x, y:mousePos.y - objPos.y};\n"
		// Set up the drag object
		" dragObject = dragSource.cloneNode(true);\n"
		" dragObject.className = 'drag';\n"
		" dragObject.style.position = 'absolute';\n"
		" dragObject.style.margin = 0;\n"
		" dragObject.style.display = 'none';\n"
		" document.body.appendChild(dragObject);\n"
		"}\n"

		"function drop(index) {\n"
		" if (dragObject) {\n"
		"  document.body.onmousedown=function(){return true};\n"		// Enable text selection again
		"  opera.swapSpeedDials(dragIndex, index);\n"
		" }\n"
		"}\n"

		// OnMouseMove; if we have a dragobject, move it to the mouse pointer
		"document.onmousemove = function(ev) {\n"
		" if (dragObject) {\n"
		"  var mousePos = mc(ev)\n"
		"  dragObject.style.left = (mousePos.x - dragOffset.x) + 'px';\n"
		"  dragObject.style.top  = (mousePos.y - dragOffset.y) + 'px';\n"
		"  dragObject.style.display = 'block';\n"
		" }\n"
		" return true;\n"
		"}\n"

		// OnMouseUp; drop drag object
		"document.onmouseup = function(ev) {\n"
		" if (dragObject) {\n"
		"  var mousePos = mc(ev);\n"
		"  var sds = document.getElementsByClassName('spdial');\n"
		"  for (var i=0; i<sds.length; i++) {\n"
		"   var targPos = pos(sds[i]);\n"
		"   var w = parseInt(sds[i].offsetWidth);\n"
		"   var h = parseInt(sds[i].offsetHeight);\n"
		"	if ((mousePos.x >= targPos.x) && (mousePos.x <= (targPos.x + w)) && (mousePos.y >= targPos.y) && (mousePos.y <= (targPos.y + h)))\n"
		"    drop(i);\n"
		"  }\n"
		"  document.body.removeChild(dragObject);\n"
		"  dragObject = null;\n"
		" }\n"
		"}\n"
	));
#endif

#ifdef SPEEDDIAL_CONTEXT_MENU
	RETURN_IF_ERROR(line.Append(
		"var p;\n"

		"function hidePopup() { document.body.removeChild(p); p = null; }\n"

		"function popup(index, set) {\n"
		" var i = index;\n"
		" var isSet = set;\n"
		" this.trig = function(ev) {\n"
		"  if (p)\n"
		"   document.body.removeChild(p);\n"
		"  p = document.createElement('div');\n"
		"  p.className = 'popup';\n"
		"  var ul = document.createElement('ul');\n"
		"  p.appendChild(ul);\n"
		"  var l = new Array(\n"
		"   {title: 'Edit...', fn: function() { hidePopup(); onSetSpeedDial(i);}},\n"
		"   {title: 'Reload', d: !isSet, fn: function() { hidePopup(); opera.reloadSpeedDial(i);}},\n"
		"   {title: 'Reload every...', d: !isSet, fn: function() { hidePopup(); opera.setSpeedDialReloadInterval(i, 60);}},\n"
		"   {title: 'Clear', d: !isSet, fn: function() { hidePopup(); reset(i);}}\n"
		"  );\n"
		"  for (var j=0;j<l.length;j++) {\n"
		"   var li = document.createElement('li');\n"
		"   li.innerText = l[j].title\n"
		"   if (!l[j].d) \n"
		"    li.onclick = l[j].fn;\n"
		"	else\n"
		"	 li.setAttribute('grayed', '');\n"
		"   ul.appendChild(li);\n"
		"  }\n"
		"  document.body.appendChild(p);\n"
		"  var m = mc(ev);\n"
		"  p.style.left = (m.x + p.offsetWidth >= window.innerWidth ? window.innerWidth - p.offsetWidth : m.x) + 'px';\n"
		"  p.style.top = (m.y + p.offsetHeight >= window.innerHeight ? window.innerHeight - p.offsetHeight : m.y) + 'px';\n"
		"  return false;\n"
		" }\n"
		"}\n"

		"document.onmousedown = function(ev) {\n"
		" var m = mc(ev);\n"
		" if (p && (m.x < p.offsetLeft || m.x >= (p.offsetLeft+p.offsetWidth) || m.y < p.offsetTop || m.y >= (p.offsetTop+p.offsetHeight))) {\n"
		"  hidePopup();\n"
		" }\n"
		"}\n"
		));
#endif // SPEEDDIAL_CONTEXT_MENU

	OP_STATUS st = line.Append(

		"var w, h;\n"

		// Set size of a thumbnail object
		"function ss(o) {\n"
		"	o.style.width = w + 'px';\n"
		"	o.style.height = h + 'px';\n"
		"}\n"

		// Force layout to set correct sizes for thumbnails
		// FIXME! Cleanup hardcoded values
		"function fl() {\n"
		" var s = window.innerWidth <= 460;\n"
		" var t = document.getElementsByTagName('table')[0];\n"
		" w = t.offsetWidth / 3 - (s ? 6 : 44);\n"
		" if (w > 256) w = 256;\n" // Don't scale up
		" h = w*3/4;\n"
		" var sds = document.getElementsByClassName('spdial');\n"
		" for (var i = 0; i < sds.length; i++){\n"
		"   var cont = document.getElementsByClassName('thumbnail-cont')[i]\n"
		"   ss(cont);\n"
		"   var thumb = document.getElementsByClassName('thumbnail')[i];\n"
		"   ss(thumb);\n"
		"	thumb.style.marginLeft = -w/2 + 'px';\n"
		"	thumb.style.marginTop = (s ? 0 : -h/2) + 'px';\n"
		"   sds[i].style.height = (h + (s ? 14 : 38)) + 'px';\n"
		" }\n"
		"}\n"

		// Get the absolute position of an element
		"function pos(obj) {\n"
		" var l=0, t=0;\n"
		" do {\n"
		"  l+=obj.offsetLeft;\n"
		"  t+=obj.offsetTop;\n"
		" } while (obj = obj.offsetParent);\n"
		" return {x:l, y:t};\n"
		"}\n"

		// Called from opera when a speed dial has changed
		"function onUpdate(index, title, url, tnUrl, loading) {\n"
		" var a = document.getElementById('a'+index);\n"
		" var u = document.getElementsByClassName('empty')[index];\n"
		" var sd = document.getElementsByClassName('spdial')[index];\n"
		" if (url == '') {\n"
		"  u.style.display = 'block';\n"
		"  a.style.display = 'none';\n"
		"  document.getElementsByClassName('thumbnail-cont')[index].innerText = '';\n"
		"  sd.onclick = 'onSetSpeedDial('+index+')';\n"
#ifdef SPEEDDIAL_CONTEXT_MENU
		"  sd.oncontextmenu = new popup(index, false).trig;\n"
#endif
		" }\n"
		" else {\n"
		"  if (loading) \n"
		"   sd.setAttribute('loading', tnUrl ? 'tn' : 'notn');\n"
		"  else\n"
		"   sd.removeAttribute('loading');\n"
		"  u.style.display = 'none';\n"
		"  a.style.display = 'block';\n"
		"  document.getElementsByClassName('title-small')[index].innerText = title ? title : url;\n"
		"  document.links[index].href = url;\n"
		"  var nn;\n"
		"  if (tnUrl) {\n"
		"   var nn = document.createElement('img');\n"
		"   nn.src = tnUrl + '?' + new Date().getTime();\n"
		"  }\n"
		"  else {\n"
		"   nn = document.createElement('div');\n"
		"   nn.innerText = loading ? '' : '"
		);
	RETURN_IF_ERROR(st);
	RETURN_IF_ERROR(line.Append(no_thumbnail_text));
	st = line.Append("';\n"
		"  }\n"
		"  nn.className = 'thumbnail-cont';\n"
		"  ss(nn);\n"
		"  var cont = document.getElementsByClassName('thumbnail')[index];\n"
		"  cont.innerText = '';\n"
		"  cont.appendChild(nn);\n"
		"  sd.onclick = '';\n"
#ifdef SPEEDDIAL_CONTEXT_MENU
		"  sd.oncontextmenu = new popup(index, true).trig;\n"
#endif
		" }\n"
		"}\n"

		"function onSetSpeedDial(index) {\n"
		);
	RETURN_IF_ERROR(st);
	RETURN_IF_ERROR(line.AppendFormat(UNI_L(
		" var url = prompt('Enter Web address here');\n"), enter_address_text.CStr()
		));
	RETURN_IF_ERROR(line.Append(
		" if (url != undefined)\n"
		"  opera.setSpeedDial(index, url);\n"
		"}\n"

		"function reset(index) {\n"
		" opera.setSpeedDial(index, '')\n"
		"}\n"

		"function onReloadSpeedDial(index) {\n"
		" opera.reloadSpeedDial(index);\n"
		"}\n"

		"document.onresize = fl;\n"

	));

	// Onload event listener
	st = line.Append(
		"window.addEventListener(\n"
		" 'load',\n"
		" function()\n"
		" {\n"
		"  fl();\n"
		"  opera.connectSpeedDial(onUpdate)\n"
		"  var sds = document.getElementsByClassName('spdial');\n"
		"  var fs = document.getElementsByClassName('full');\n"
		"  for(var i=0; i<sds.length; i++) {\n"
#ifdef SPEEDDIAL_DRAGNDROP
		"   fs[i].onmousedown = function(ev) {\n"
		"     if (ev.button == 0)\n"	// Left mouse button
		"      drag(ev, this);\n"
		"    }\n"
#endif // SPEEDDIAL_DRAGNDROP
#ifdef SPEEDDIAL_CONTEXT_MENU
		"   sds[i].oncontextmenu = new popup(i, false).trig;\n"
#endif // SPEEDDIAL_CONTEXT_MENU
		"  }\n"
	);
	RETURN_IF_ERROR(st);
	RETURN_IF_ERROR(line.Append(
		" },\n"
		" false);\n"
		"</script>\n"
	));

	// End of scripts

	// HTML part
	RETURN_IF_ERROR(line.Append("<body><table>\n"));
	for (unsigned cols = 0; cols < SPEEDDIAL_COLS; cols++)
	{
		RETURN_IF_ERROR(line.Append("<tr>\n"));
		for (unsigned rows = 0; rows < SPEEDDIAL_ROWS; rows++)
		{
			const unsigned index = cols * SPEEDDIAL_ROWS + rows;

			RETURN_IF_ERROR(line.AppendFormat(UNI_L("<td class='spdial' onclick = 'onSetSpeedDial(%i)'>\n"), index));

			// Unassigned
			RETURN_IF_ERROR(
				line.AppendFormat(UNI_L("<div class='empty' style='display: block'>\n<div class='index-big'>%i</div>\n<div class='text-big'>%s</div>\n</div>\n"),
					index + 1,
					add_text.CStr()));

			RETURN_IF_ERROR(line.AppendFormat(UNI_L("<div class='full' id='a%i' style='display: none'>\n"), index));
				RETURN_IF_ERROR(line.AppendFormat(UNI_L(" <div class='index-small'>%i</div>\n"), index+1));

				// The thumbnail image
				RETURN_IF_ERROR(line.AppendFormat(UNI_L(" <a href='dummy' class='thumbnail'><div class='thumbnail-cont'></div></a>\n"), index));

				RETURN_IF_ERROR(line.AppendFormat(UNI_L(" <div class='reset' onClick='reset(%i)'></div>\n"), index));

				// Preview title
				RETURN_IF_ERROR(line.Append(" <div class='title-small'></div>\n"));

			RETURN_IF_ERROR(line.Append("</div>"));
			RETURN_IF_ERROR(line.Append("</td>"));
		}
		RETURN_IF_ERROR(line.Append("</tr>"));
	}
	RETURN_IF_ERROR(line.Append("</table>"));
	RETURN_IF_ERROR(line.AppendFormat(UNI_L("<span class='btn' onclick='for (var i=0;i<%i;i++) {opera.reloadSpeedDial(i)}'>Refresh</span>"), NUM_SPEEDDIALS));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

	return CloseDocument();
}

OperaSpeedDialCallback::~OperaSpeedDialCallback()
{
	if (g_speed_dial_manager)
		g_speed_dial_manager->RemoveSpeedDialListener(this);
}

void OperaSpeedDialCallback::GCTrace(ES_Runtime *runtime)
{
	if (m_callback)
		runtime->GCMark(m_callback);
}

OP_STATUS OperaSpeedDialCallback::OnSpeedDialChanged(SpeedDial* speed_dial)
{
	OpString tn_full_resolved;

	OpString url, title;
	RETURN_IF_ERROR(speed_dial->GetUrl(url));
	RETURN_IF_ERROR(speed_dial->GetTitle(title));

#ifdef CORE_THUMBNAIL_SUPPORT
	if (speed_dial->GetThumbnail() && *speed_dial->GetThumbnail())
	{
		OpString tn_full;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_THUMBNAIL_FOLDER, tn_full));
		RETURN_IF_ERROR(tn_full.Append(speed_dial->GetThumbnail()));

		OP_MEMORY_VAR BOOL successful;
		RETURN_IF_LEAVE(successful = g_url_api->ResolveUrlNameL(tn_full, tn_full_resolved));
		if (!successful)
			return OpStatus::ERR;
	}
#endif

	return Call(speed_dial->GetPosition(), title.CStr(), url.CStr(), tn_full_resolved.CStr(), speed_dial->IsLoading());
}

OP_STATUS OperaSpeedDialCallback::OnSpeedDialRemoved(SpeedDial* speed_dial)
{
	return Call(speed_dial->GetPosition(), NULL, NULL, NULL, FALSE);
}

OP_STATUS OperaSpeedDialCallback::Call(int index,
									   const uni_char* title,
									   const uni_char* url,
									   const uni_char* thumbnail_url,
									   BOOL is_loading)
{
	ES_Value argv[5]; /* ARRAY OK 2008-07-11 marcusc */
	argv[0].type = VALUE_NUMBER;
	argv[0].value.number = index;
	argv[1].type = VALUE_STRING;
	argv[1].value.string = title ? title : UNI_L("");
	argv[2].type = VALUE_STRING;
	argv[2].value.string = url ? url : UNI_L("");
	argv[3].type = thumbnail_url ? VALUE_STRING : VALUE_UNDEFINED;
	argv[3].value.string = thumbnail_url ? thumbnail_url : UNI_L("");
	argv[4].type = VALUE_BOOLEAN;
	argv[4].value.boolean = !!is_loading;

	RETURN_IF_MEMORY_ERROR( m_ai->CallFunction(m_callback, NULL, 5, argv) );

	return OpStatus::OK;
}

#endif // OPERASPEEDDIAL_URL
