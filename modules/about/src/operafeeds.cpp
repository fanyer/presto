/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Arne Martin Guettler
*/

#include "core/pch.h"

#if defined WEBFEEDS_DISPLAY_SUPPORT && defined _LOCALHOST_SUPPORT_

#include "modules/about/operafeeds.h"
#include "modules/webfeeds/webfeeds_api.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/htmlify.h"

OP_STATUS OperaFeeds::GenerateData()
{
#ifdef USE_ABOUT_TEMPLATES
	return GenerateDataFromTemplate(PrefsCollectionFiles::TemplateWebFeedsDisplay);
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_MINI_MY_FEEDS, PrefsCollectionFiles::StyleWebFeedsDisplay));

	#ifndef OPFILE_HAVE_SCRIPT_FOLDER
	// Backwards compatibility fix
	# define OPFILE_SCRIPT_FOLDER OPFILE_HOME_FOLDER
	#endif

	// Scripts
	OpFile common_js, substance_js;
	RETURN_IF_ERROR(common_js.Construct(UNI_L("common.js"), OPFILE_SCRIPT_FOLDER));
	RETURN_IF_ERROR(substance_js.Construct(UNI_L("substance.js"), OPFILE_SCRIPT_FOLDER));
	const uni_char *script_template =
		UNI_L("<script type=\"text/javascript\" src=\"%s\"></script>\n");
	OpString common_js_url, substance_js_url;
	TRAPD(rc, g_url_api->ResolveUrlNameL(common_js.GetFullPath(), common_js_url);
	          g_url_api->ResolveUrlNameL(substance_js.GetFullPath(), substance_js_url));
	RETURN_IF_ERROR(rc);
	uni_char *htmlified_common = HTMLify_string(common_js_url.CStr());
	if (!htmlified_common)
		return OpStatus::ERR_NO_MEMORY;
	uni_char *htmlified_substance = HTMLify_string(substance_js_url.CStr());
	if (!htmlified_substance)
	{
		delete[] htmlified_common;
		return OpStatus::ERR_NO_MEMORY;
	}
	m_url.WriteDocumentDataUniSprintf(script_template, htmlified_common);
	delete[] htmlified_common;
	m_url.WriteDocumentDataUniSprintf(script_template, htmlified_substance);
	delete[] htmlified_substance;

	// Add the ID of the preview feed, if any
	OpString line;
	line.Reserve(3072);
	RETURN_IF_ERROR(line.Set("<script type=\"text/javascript\">feedsManager.previewedFeedId = "));
	if (m_preview_feed)
	{
		RETURN_IF_ERROR(line.AppendFormat(UNI_L("%d"), m_preview_feed->GetId()));
	}
	else
	{
		RETURN_IF_ERROR(line.Append("Infinity"));
	}
	RETURN_IF_ERROR(line.Append(";</script>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	RETURN_IF_ERROR(OpenBody());

	RETURN_IF_ERROR(line.Set(
		"<!-- feedEntries -->\n"
		"<div id='feedEntries'>\n"
		"<div class='header'>\n"
		"<h1>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_SELECT_FEED));
		// FIXME: Remove align
	RETURN_IF_ERROR(line.Append(
		"<img src='' alt='logo' align='right' /></h1>\n"
		"<p id='feedEntriesInfos'>"));
	RETURN_IF_ERROR(line.Append(UNI_L("\x2026")));
	RETURN_IF_ERROR(line.Append(
		"</p>\n"
		"</div>\n"
		"<div class='controls'>\n"
		"<a href='#feedsList' class='handheld'>&lt;&lt;</a>\n"));
#if defined WEB_HANDLERS_SUPPORT && defined WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
	RETURN_IF_ERROR(
	line.Append("<select id='readers' class='readersSelect' "
		"onchange='feedsManager.feedReaderChoice()'></select>\n")
	);
	RETURN_IF_ERROR(
	line.Append("<script>\n"
		"var select  = document.getElementById('readers');\n"
		"feedsManager.populateFeedReadersList(select);\n"
		"</script>\n")
	);
	RETURN_IF_ERROR(line.AppendFormat("<input type='button' id='subscribeButton' class='readersSubscribeButton' value='"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MINI_FEED_SUBSCRIBE));
	RETURN_IF_ERROR(line.Append("' onclick='feedsManager.subscribeFeed()' />\n"));
	RETURN_IF_ERROR(line.AppendFormat("<input type='button' id='deleteButton' class='readersSubscribeButton' value='"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_DELETE));
	RETURN_IF_ERROR(line.Append("' onclick='feedsManager.deleteFeedReader();' disabled />\n"));
#else
	RETURN_IF_ERROR(line.Append("<a href='#subscriptionToggle' id='feedEntriesSubscription' ><span>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MINI_FEED_UNSUBSCRIBE));
	RETURN_IF_ERROR(line.Append("</span><span>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MINI_FEED_SUBSCRIBE));
	RETURN_IF_ERROR(line.Append("</span></a>\n"));
#endif // WEB_HANDLER_SUPPORT && defined WEBFEEDS_ADD_REMOVE_EXTERNAL_READERS
	RETURN_IF_ERROR(line.Append(
		"<form id='feedEntriesControls' action=''>\n"
		"<fieldset>\n"    "<legend></legend>\n"
		"<input type='checkbox'   name='unread' /> <label for='unread'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_UNREAD_FLAG));
	RETURN_IF_ERROR(line.Append(
		"</label>\n"
		"<input type='checkbox'   name='flagged'/> <label for='flagged'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_FLAGGED_FLAG));
	RETURN_IF_ERROR(line.Append(
		"</label>\n"
		"<label for='search'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_SEARCH_CAPTION));
	RETURN_IF_ERROR(line.Append(
		"<input type='text' name='search' value=''/></label>\n"
		"</fieldset>\n"
		"</form>\n"
		"</div>\n"
		"<div id='feedEntriesWrapper'>\n"
		"<table id='feedEntriesTable' cellspacing='0' cellpadding='0'>\n"
		"<thead id='feedEntriesHeader'>\n"
		"<tr><th>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MESSAGE_FLAG_TITLE));
	RETURN_IF_ERROR(line.Append("</th><th>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_FEED_TITLE));
	RETURN_IF_ERROR(line.Append("</th><th>"));
	// FIXME: Disambiguate string?
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::SI_DIRECTORY_COLOUMN_TIME));
	RETURN_IF_ERROR(line.Append(
		"</th></tr>\n"
		"</thead>\n"
		"<tbody id='feedEntriesTableBody'></tbody>\n"
		"<tbody id='noFeedEntries'>\n"
		"<tr><td></td><td scolspan='3'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MINI_NO_FEED_ITEMS_FOUND));
	RETURN_IF_ERROR(line.Append(
		"</td><td></td></tr>\n"
		"</tbody>\n"
		"</table>\n"
		"</div>\n"
		"</div>\n"

		"<!-- feedEntry -->\n"
		"<div id='feedEntry'>\n"
		"<div class='header'><h1>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_SELECT_ARTICLE));
	RETURN_IF_ERROR(line.Append(
		"</h1></div>\n"
		"<div class='controls' id='feedEntryControls'>\n"
		"<a href='#feedEntries' class='handheld'>&lt;&lt;</a>\n"
		"<a href='#feedEntryToggleFlag' id='feedEntryToggleFlag'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_FLAG_MESSAGE));
	RETURN_IF_ERROR(line.Append(
		"</a>\n"
		"<a href='#' id='feedEntryReadFullArticle'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_READ_ARTICLE));
	RETURN_IF_ERROR(line.Append("</a>\n"
		"</div>\n"
		"<div id='feedEntryContent' class='content'></div>\n"
		"</div>\n"

		"<!-- feedsList -->\n"
		"<div id='feedsList'>\n"
		"<div class='header'><h1>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MINI_MY_FEEDS));
	RETURN_IF_ERROR(line.Append("</h1></div>\n"
  		"<div id='feedsListWrapper'>\n"
		"<ul id='feedsListUl'>\n"
		"<li id='smartGroup_preview' class='smartGroup'><a href='#feedEntries'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_PREVIEW_MESSAGE));
	RETURN_IF_ERROR(line.Append(
		"</a><ul>\n"
		"</ul></li><li id='smartGroup_unviewed' class='smartGroup'><a href='#feedEntries'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_UNVIEWED_MESSAGES));
	RETURN_IF_ERROR(line.Append(
		"</a>\n"
		"</li><li id='smartGroup_flagged' class='smartGroup'><a href='#feedEntries'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_FLAGGED_FLAG));
	RETURN_IF_ERROR(line.Append(
		"</a>\n"
		"</li></ul>\n"
		"</div>\n"
		"</div>\n"

		"<!-- settings -->\n"
		"<div id='settings'>\n"
		"<div class='header'><h1>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_SETTINGS_CAPTION));
	RETURN_IF_ERROR(line.Append(
		"<span></span></h1></div>\n"
		"<div class='controls' id='feedEntryControls'>\n"
		"<a href='#feedsList' class='handheld'>&lt;&lt;</a>\n"
		"</div>\n"
		"<div>\n"
		"<form id='settingsForm' action=''>\n"
		"<fieldset>\n"
		"<legend></legend>\n"
		"<label for='feedName'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_RENAME_FEED));
	RETURN_IF_ERROR(line.Append(
		"</label> <input type='text' name='feedName' value='' /><hr/>\n"
		"<label for='moveToNewGroup'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_MOVE_TO_NEW_GROUP));
	RETURN_IF_ERROR(line.Append(
		"</label> <input name='moveToNewGroup' type='text' value=''></input><hr/>\n"
		"<label for='moveToGroup'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_MOVE_TO_EXISTING_GROUP));
	RETURN_IF_ERROR(line.Append(
		"</label> <select name='moveToGroup'></select><hr/>\n"
		"<label for='updateInterval'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_UPDATE_FREQUENCY));
	RETURN_IF_ERROR(line.Append(
		"</label> <select name='updateInterval'>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	static const int update[] = { 5, 15, 30, 60, 180, 360, 720, 1440 };
	{
		OpString every_minutes, every_hour, every_hours, every_day;
		TRAPD(rc,
			g_languageManager->GetStringL(Str::S_EVERY_X_MINUTES, every_minutes);
			g_languageManager->GetStringL(Str::S_EVERY_HOUR, every_hour);
			g_languageManager->GetStringL(Str::S_EVERY_X_HOURS, every_hours);
			g_languageManager->GetStringL(Str::S_EVERY_DAY, every_day));
		RETURN_IF_ERROR(rc);

		for (size_t i = 0; i < ARRAY_SIZE(update); ++ i)
		{
			RETURN_IF_ERROR(line.Set("<option value='"));
			RETURN_IF_ERROR(line.AppendFormat(UNI_L("%d'>"), update[i]));
			if (update[i] < 60)
				RETURN_IF_ERROR(line.AppendFormat(every_minutes.CStr(), update[i]));
			else if (update[i] == 60)
				RETURN_IF_ERROR(line.Append(every_hour));
			else if (update[i] < 1440)
				RETURN_IF_ERROR(line.AppendFormat(every_hours.CStr(), update[i] / 60));
			else if (update[i] == 1440)
				RETURN_IF_ERROR(line.Append(every_day));
			//else
			//	RETURN_IF_ERROR(line.AppendFormat(every_days.CStr(), update[i] / 1440));
			RETURN_IF_ERROR(line.Append("</option>\n"));
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
		}
	}

	RETURN_IF_ERROR(line.Set(
		"</select><hr/>\n"
		"<label for='maxEntries'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MAXIMUM_ARTICLES));
	RETURN_IF_ERROR(line.Append(
		"</label> <select name='maxEntries'>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	static const int articles[] = { 10, 20, 50, 100 };
	for (size_t j = 0; j < ARRAY_SIZE(articles); ++ j)
	{
		m_url.WriteDocumentDataUniSprintf(UNI_L("<option value='%d'>%d</option>\n"), articles[j]);
	}

	RETURN_IF_ERROR(line.Set(
		"</select><hr/>\n"
		"<label for='maxAge'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_MAXAGE_ARTICLES));
	RETURN_IF_ERROR(line.Append(
		"</label> <select name='maxAge'>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	static const int age[] = { 10080, 20160, 40320 };
	{
		OpString every_week, every_weeks;
		TRAPD(rc,
			g_languageManager->GetStringL(Str::S_EVERY_WEEK, every_week);
			g_languageManager->GetStringL(Str::S_EVERY_X_WEEKS, every_weeks));
		RETURN_IF_ERROR(rc);

		for (size_t k = 0; k < ARRAY_SIZE(age); ++ k)
		{
			RETURN_IF_ERROR(line.Set("<option value='"));
			RETURN_IF_ERROR(line.AppendFormat(UNI_L("%d'>"), age[k]));
			if (age[k] == 10080)
				RETURN_IF_ERROR(line.Append(every_week));
			else
				RETURN_IF_ERROR(line.AppendFormat(every_weeks.CStr(), age[k] / 10080));
			line.Append("</option>");
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
		}
	}

	RETURN_IF_ERROR(line.Set(
		"<option value='Infinity'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_TIME_FOREVER));
	RETURN_IF_ERROR(line.Append(
		"</option>\n"
		"</select><hr/>\n"
		"<label for='showImages'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_SHOW_IMAGES_ARTICLES));
	RETURN_IF_ERROR(line.Append(
		"</label> <input type='checkbox' name='showImages' />\n"
		"<button id='cancelSettings'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::DI_IDCANCEL));
	RETURN_IF_ERROR(line.Append(
		"</button>\n"
		"<button id='validateSettings'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::SI_SAVE_BUTTON_TEXT));
	RETURN_IF_ERROR(line.Append(
		"</button>\n"
		"</fieldset>\n"
		"</form>\n"
		"</div>\n</div>\n"
		"<!-- darkBox -->\n<div id='darkBox'></div>\n"
		"<!-- strings -->\n<div id='strings'>\n<span id='unsubscribeConfirm'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_UNSUBSCRIBE_QUERY));
	RETURN_IF_ERROR(line.Append(
		"</span>\n"
		"<span id='subscribeConfirm'>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::S_WEBFEEDS_SUBSCRIBE_QUERY));
	RETURN_IF_ERROR(line.Append(
		"</span>\n"
		"<span id='defaultGroup'>misc</span>\n"
		"<span id='previewGroup'>preview</span>\n"
		"</div>\n"
		"</div>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	return CloseDocument();
#endif // USE_ABOUT_TEMPLATES
}

#endif
