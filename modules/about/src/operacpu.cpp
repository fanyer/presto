/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#include "core/pch.h"

#ifdef CPUUSAGETRACKING

#include "modules/about/operacpu.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"


//       PAGE                                   Last 5s   30s    120s
//        <-  Page title (truncated if needed):   12%     43%     21%

// virtual
OP_STATUS OperaCPU::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_IDABOUT_CPU, PrefsCollectionFiles::StyleCPUFile));
#else // _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_IDABOUT_CPU));
#endif // _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenBody());

	TempBuffer line;
	RETURN_IF_ERROR(line.Append("<div id=commandblock><a id=togglepause unselectable='on' href='javascript:togglePauseUpdate()'>"));
	OpString string_pause_label;
	if (OpStatus::IsError(g_languageManager->GetString(Str::S_OPERACPU_PAUSE_LABEL, string_pause_label)) || string_pause_label.IsEmpty())
		RETURN_IF_ERROR(string_pause_label.Set("Pause--")); // FIXME - L10N
	RETURN_IF_ERROR(line.Append(string_pause_label.CStr(), string_pause_label.Length()));
	RETURN_IF_ERROR(line.Append("</a> &nbsp;&nbsp;<a id=faster unselectable='on' href='javascript:fasterUpdates()'>"));
	OpString string_faster_label;
	if (OpStatus::IsError(g_languageManager->GetString(Str::S_OPERACPU_FASTER_LABEL, string_faster_label)) || string_faster_label.IsEmpty())
		RETURN_IF_ERROR(string_faster_label.Set("Faster--")); // FIXME - L10N
	RETURN_IF_ERROR(line.Append(string_faster_label.CStr(), string_faster_label.Length()));
	RETURN_IF_ERROR(line.Append("</a> &nbsp;&nbsp;<a id=slower unselectable='on' href='javascript:slowerUpdates()'>"));
	OpString string_slower_label;
	if (OpStatus::IsError(g_languageManager->GetString(Str::S_OPERACPU_SLOWER_LABEL, string_slower_label)) || string_slower_label.IsEmpty())
		RETURN_IF_ERROR(string_slower_label.Set("Slower--")); // FIXME - L10N
	RETURN_IF_ERROR(line.Append(string_slower_label.CStr(), string_slower_label.Length()));
	RETURN_IF_ERROR(line.Append("</a></div>"));

	RETURN_IF_ERROR(line.Append("<div id=datablock></div>"));
	RETURN_IF_ERROR(line.Append("<div id=graphblock></div>"));
	const char* script =
		"window.svgns = 'http://www.w3.org/2000/svg';"
		"window.sort_column = 0;"
		"window.sort_direction = 0;"
		"window.triangle_point_up = '\\u25B2';"
		"window.triangle_point_down = '\\u25BC';"
		"window.triangle_neutral = '\\u25B7';"
		"window.arrow_left = '\\u2190';"
		"window.update_timer_id = undefined;"
		"window.update_gap = 2000;"
		"window.SHORT_TIME = 5;"
		"window.MEDIUM_TIME = 30;"
		"window.LONG_TIME = 120;"
		"window.measurement_columns = [window.SHORT_TIME, window.MEDIUM_TIME, window.LONG_TIME];"

		"if (opera.getLocaleString) {"
		"window.STRING_PROCESS = opera.getLocaleString('S_OPERACPU_PROCESS_LABEL');"
		"window.STRING_LAST_X_SECONDS = opera.getLocaleString('S_OPERACPU_LAST_X_SECONDS_LABEL');"
		"window.STRING_TOTAL_LABEL = opera.getLocaleString('S_OPERACPU_TOTAL_LABEL');"
		"window.STRING_PAUSE_LABEL = opera.getLocaleString('S_OPERACPU_PAUSE_LABEL');"
		"window.STRING_RESUME_LABEL = opera.getLocaleString('S_OPERACPU_RESUME_LABEL');"
		"window.STRING_FASTER_LABEL = opera.getLocaleString('S_OPERACPU_FASTER_LABEL');"
		"window.STRING_SLOWER_LABEL = opera.getLocaleString('S_OPERACPU_SLOWER_LABEL');"
		"}"
		"if (!window.STRING_PROCESS) window.STRING_PROCESS = 'Process';"
		"if (!window.STRING_LAST_X_SECONDS) window.STRING_LAST_X_SECONDS = 'Last # Seconds';"
		"if (!window.STRING_TOTAL_LABEL) window.STRING_TOTAL_LABEL = 'Total';"
		"if (!window.STRING_PAUSE_LABEL) window.STRING_PAUSE_LABEL = 'Pause';"
		"if (!window.STRING_RESUME_LABEL) window.STRING_CONTINUE_LABEL = 'Resume';"
		"if (!window.STRING_FASTER_LABEL) window.STRING_FASTER_LABEL = 'Faster updates';"
		"if (!window.STRING_SLOWER_LABEL) window.STRING_SLOWER_LABEL = 'Slower updates';"

		"function spreadLowCompressHigh(number) {"
		"return Math.log(number * (Math.E-1) + 1);"
		"} /* end spreadLowCompressHigh */\n"

		"function setPercentStyle(elem, percent, active) {"
		"if (percent < 0.5) return;"
		"var low = active ? [0xff, 0xee, 0xcc] : [0xff, 0xf8, 0xee];"
		"var high = active ? [0xff, 0x99, 0x00] : [0xff, 0xc8, 0x7f];"
		"var color='';"
		"for (var i = 0; i < 3; i++) {"
		"if (color != '') color += ',';"
		// A color function that spreads out low values more than high
		// values. There can't be many high values at the same time anyway.
		"var colorScalePosition = percent/100;"
		"colorScalePosition = spreadLowCompressHigh(colorScalePosition);"
		"color += low[i] + Math.round(colorScalePosition*(high[i]-low[i]));"
		"}"
		"var style='background-color:rgb('+color+');';"
		"elem.setAttribute('style',style);"
		"} /* end setPercentStyle */\n"

		"function createDisplayNameNode(displayName, id, activatable) {"
		"var wrapper = document.createElement('span');"
		"var prefix_cell = document.createElement('span');"
		"prefix_cell.style.display = 'inline-block';"
		"prefix_cell.style.width = '1.5em';"
		"prefix_cell.appendChild(createPrefixCellContents(id, activatable));"
		"wrapper.appendChild(prefix_cell);"
		"wrapper.appendChild(document.createTextNode(displayName));"
		"return wrapper;"
		"} /* end createDisplayNameNode */\n"

		"function createPrefixCellContents(id, activatable) {"
		"if (!activatable)"
		"return document.createTextNode('');"
		"var wrapper = document.createElement('span');"
		"wrapper.appendChild(document.createTextNode(window.arrow_left));"
		"wrapper.setAttribute('onclick', 'opera.activateCPUUser('+id+')');"
		"wrapper.style.cursor = 'pointer';"
		"return wrapper;"
		"} /* end createPrefixCellContents */\n"

		"function createLine(process_data, active) {"
		"var tr = document.createElement('tr');"
		"tr.setAttribute('class', active ? 'active' : 'dead');"
		"if (active) {"
		"tr.setAttribute('onclick', 'window.active_id = ' + process_data.id + ';updateTable()');"
		"}"
		"var td_display_name = document.createElement('td');"
		"var display_name = process_data.displayName;"
		"if (!display_name) display_name = '' + process_data.id;"
		"td_display_name.appendChild(createDisplayNameNode(display_name, process_data.id, active && process_data.activatable));"
		"td_display_name.setAttribute('class', 'desc');"
		"tr.appendChild(td_display_name);"
		"for (var j = 0; j < process_data.length; j++) {"
		"var td_percent = document.createElement('td');"
		"var percent = Math.round(100*process_data[j]);"
		"td_percent.appendChild(document.createTextNode(percent + '%'));"
		"setPercentStyle(td_percent, percent, active);"
		"td_percent.setAttribute('class', 'p');"
		"tr.appendChild(td_percent);"
		"} /* end for j */"
		"return tr;"
		"} /* end createLine */\n"

		"function getSortIndicator(column) {"
		"if (sort_direction != 0 && sort_column == column) {"
		"return sort_direction > 0 ? window.triangle_point_down : window.triangle_point_up;"
		"}"
		"return ' ' + window.triangle_neutral;"
		"} /* end getSortIndicator */\n"

		"function sort_function(row_a, row_b) {"
		"if (sort_column == 0) {"
		"var a_name = row_a.cells[0].textContent;"
		"var b_name = row_b.cells[0].textContent;"
		"return a_name.localeCompare(b_name);"
		"} else {"
		"var a_value = parseInt(row_a.cells[sort_column].textContent);"
		"var b_value = parseInt(row_b.cells[sort_column].textContent);"
		"return a_value - b_value;"
		"}"
		"} /* end sort_function */\n"

		"function sortTable(rows) {"
		"if (sort_direction == 0) return;"
		"rows.sort(sort_function);"
		"if (sort_direction == 1)"
		"rows.reverse();"
		"} /* end sortTable */\n"

		"function changeSort(column_no) {"
		"if (sort_column == column_no) {"
		"if (sort_direction == 1)"
		"sort_direction = -1;"
		"else sort_direction++"
		"} else {"
		"sort_column = column_no;"
		"sort_direction = 1;"
		"}"
		"updateTable();"
		"} /* end changeSort */\n"

		"function addSortClickListener(th, column_no) {"
		"th.onclick=function() {changeSort(column_no);};"
		"} /* end addSortClickListener */\n"

		"function updateTable() {"
		"try {"
		"var data = opera.getCPUUsage(measurement_columns);"
		"var headers = data.columnHeaders;"
		"var table = document.createElement('table');"
		"var tr1 = document.createElement('tr');"
		"var th_column0 = document.createElement('th');"
		"th_column0.setAttribute('rowspan', '2');"
		"th_column0.setAttribute('class', 'processlabel');"
		"th_column0.appendChild(document.createTextNode(STRING_PROCESS + getSortIndicator(0)));"
		"th_column0.setAttribute('unselectable', 'on');"
		"addSortClickListener(th_column0, 0);"
		"tr1.appendChild(th_column0);"
		"table.appendChild(tr1);"
		"var th_long = document.createElement('th');"
		"th_long.setAttribute('colspan', measurement_columns.length);"
		"th_long.setAttribute('class', 'phlong');"
		"th_long.appendChild(document.createTextNode(STRING_LAST_X_SECONDS));"
		"tr1.appendChild(th_long);"
		"var tr2 = document.createElement('tr');"
		"/* First cell used by cell from previous line. */"
		"for (var k = 0; k < measurement_columns.length; k++) {"
		"var th = document.createElement('th');"
		"th.appendChild(document.createTextNode(measurement_columns[k] + getSortIndicator(1+k)));"
		"th.setAttribute('class', 'ph');"
		"th.setAttribute('unselectable', 'on');"
		"addSortClickListener(th, 1+k);"
		"tr2.appendChild(th);"
		"} /* end for k */"
		"table.appendChild(tr2);"

		"var tr_total = document.createElement('tr');"
		"tr_total.setAttribute('class', 'total');"
		"var th_total_label = document.createElement('th');"
		"th_total_label.appendChild(document.createTextNode(STRING_TOTAL_LABEL));"
		"tr_total.appendChild(th_total_label);"
		"for (var n = 0; n < data.total.length; n++) {"
		"var th_percent = document.createElement('th');"
		"var percent = Math.round(100*data.total[n]);"
		"th_percent.appendChild(document.createTextNode(percent + '%'));"
		"setPercentStyle(th_percent, percent, true);"
		"th_percent.setAttribute('class', 'ph');"
		"tr_total.appendChild(th_percent);"
		"} /* end for k */"
		"table.appendChild(tr_total);"

		"var rows = new Array;"
		"var new_data = new Array;"
		"for (var i = 0; i < data.length; i++) {"
		"new_data[data[i].id] = data[i];"
		"if (window.old_data && window.old_data[0])"
		"delete window.old_data[0][data[i].id];"
		"var tr = createLine(data[i], true);"
		"rows.push(tr);"
		"} /* end for i */"
		"/* dead processes */"
		"if (window.old_data) {"
		"for (var dead_block = 0; dead_block < window.old_data.length; dead_block++) {"
		"for (var dead_id in window.old_data[dead_block]) {"
		"var tr = createLine(window.old_data[dead_block][dead_id], false);"
		"rows.push(tr);"
		"} /* end for dead_id */"
		"} /* end for dead_block */"
		"} else { /* end if old_data */"
		"window.old_data = new Array;"
		"}"
		"delete window.old_data[2];"
		"window.old_data.unshift(new_data);"
		"sortTable(rows);"
		"for (var m = 0; m < rows.length; m++) {"
		"table.appendChild(rows[m]);"
		"} /* end for m */"
		"var div = document.getElementById('datablock');"
		"div.innerHTML = '';"
		"div.appendChild(table);"
		"var graph_div = document.getElementById('graphblock');"
		"graph_div.innerHTML = '';"
		"graph_div.appendChild(createGraph(window.active_id));"
		"updateSpeedButtons();"
		"clearTimeout(window.update_timer_id);"
		"window.update_timer_id = setTimeout(updateTable, window.update_gap);"
		"} catch (e) { alert(e); }"
		"} /* end updateTable */\n"

		"function createGraph(id) {"
		"if (id === undefined) return document.createTextNode('');"
		"var samples = opera.getCPUUsageSamples(id);"
		"if (samples === undefined) return document.createTextNode('');"
		"var svg = document.createElementNS(svgns, 'svg');"
		"svg.setAttribute('width', '100%');"
		"svg.setAttribute('height', '100px');"
		"svg.setAttribute('viewBox', '0 0 1.1 1.1');"
		"svg.setAttribute('preserveAspectRatio', 'none');"
		"var line = document.createElementNS(svgns, 'polyline');"
		"var origin_x = 0.05; var origin_y = 1.05;"
		"var background = document.createElementNS(svgns, 'rect');"
		"background.setAttribute('x', origin_x);"
		"background.setAttribute('y', origin_y - 1);"
		"background.setAttribute('height', 1.0);"
		"background.setAttribute('width', '1.0');"
		"background.setAttribute('fill', 'black');"
		"background.setAttribute('vector-effect', 'non-scaling-stroke');"
		"background.setAttribute('stroke-width', 2.5);"
		"background.setAttribute('stroke', 'green');"
		"svg.appendChild(background);"
		"for (var grid_line_pos = 0.25; grid_line_pos < 1.0; grid_line_pos += 0.25) {"
		"var grid_line = document.createElementNS(svgns, 'line');"
		"grid_line.setAttribute('x1', origin_x);"
		"grid_line.setAttribute('x2', origin_x + 1.0);"
		"grid_line.setAttribute('y1', origin_y - grid_line_pos);"
		"grid_line.setAttribute('y2', origin_y - grid_line_pos);"
		"grid_line.setAttribute('stroke-dasharray', '1.5 3');"
		"grid_line.setAttribute('stroke-width', '1.5');"
		"grid_line.setAttribute('stroke', 'green');"
		"grid_line.setAttribute('vector-effect', 'non-scaling-stroke');"
		"svg.appendChild(grid_line);"
		"}"
		"var points = '';"
		"for (var i = 0; i < samples.length; i++) {"
		"if (samples[i] >= 0)"
		"points += ' ' + (origin_x + (i / samples.length)) + ',' + (origin_y - samples[i]);"
		"}"
		"line.setAttribute('points', points);"
		"line.setAttribute('fill', 'none');"
		"line.setAttribute('stroke', 'green');"
		"line.setAttribute('stroke-width', '1.5');"
		"line.setAttribute('vector-effect', 'non-scaling-stroke');"
		"svg.appendChild(line);"
		"return svg;"
		"} /* end createGraph */\n"

		"function togglePauseUpdate() {"
		"if (window.update_timer_id) {"
		"clearTimeout(window.update_timer_id);"
		"delete window.update_timer_id;"
		"document.getElementById('togglepause').textContent = window.STRING_RESUME_LABEL;"
		"} else {"
		"updateTable();"
		"document.getElementById('togglepause').textContent = window.STRING_PAUSE_LABEL;"
		"}"
		"} /* end togglePauseUpdate */\n"

		"function updateSpeedButton(buttonId, newTime) {"
		"var button = document.getElementById(buttonId);"
		"var enable = newTime != window.update_gap;"
		"if (!button.hrefBackup) button.hrefBackup = button.href;"
		"if (enable) {"
		"button.href = button.hrefBackup;"
		"button.title = Math.round(newTime / 1000) + ' s';"
		"} else {"
		"button.removeAttribute('href');"
		"button.removeAttribute('title');"
		"}"
		"} /* end updateSpeedButton */\n"

		"function updateSpeedButtons() {"
		"updateSpeedButton('slower', getSlowerUpdateTime());"
		"updateSpeedButton('faster', getFasterUpdateTime());"
		"} /* end updateSpeedButtons */\n"

		"function getFasterUpdateTime() {"
		"if (window.update_gap > 250)"
		"return window.update_gap * 2 / 3;"
		"return window.update_gap;"
		"} /* end getFasterUpdateTime */\n"

		"function getSlowerUpdateTime() {"
		"if (window.update_gap < 30000)"
		"return window.update_gap * 3 / 2;"
		"return window.update_gap;"
		"} /* end getSlowerUpdateTime */\n"

		"function fasterUpdates() {"
		"window.update_gap = getFasterUpdateTime();"
		"updateTable();"
		"} /* end fasterUpdates */\n"

		"function slowerUpdates() {"
		"window.update_gap = getSlowerUpdateTime();"
		"updateTable();"
		"} /* end slowerUpdates */\n"

		"window.onload = updateTable;";
	RETURN_IF_ERROR(line.Append("<script>"));
	RETURN_IF_ERROR(line.Append(script));
	RETURN_IF_ERROR(line.Append("</script>"));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.GetStorage()));

	return CloseDocument();
}

#endif // CPUUSAGETRACKING
