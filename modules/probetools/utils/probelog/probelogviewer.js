/* -*- Mode: Javascript; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

PIXELS_PER_LINE = 20;
FONT_SIZE = 10;
TIME_SCALE_WIDTH = 20;
LOG_TEXT_XPOS = 500;
SYSCALL_MARKER_WIDTH = 15;
LOG_MARKER_WIDTH = 15;
SAT = 100;
VAL = 66;

stream = null;
next_hue = 0;

function Probe(c)
{
    if (c)
		this.color = c;
    else
    {
		this.color = hsv_to_rgb(next_hue, SAT, VAL);
		next_hue = (next_hue + 50) % 360;
    }
}

known_probes = {};

function LogLine(timestamp, text, color, start)
{
    this.timestamp = timestamp;
    this.text = text;
    this.color = color;
    this.start = start;
}

function Entries()
{
    this.entries = [];
    this.re_log = /(\d+\.\d+): *(.*) \((.*)\)/;
}

Entries.prototype.has_entries = function() { return this.entries.length > 0; }

Entries.prototype.add_line = function(line, has_reported_first_error) {
    var m = this.re_log.exec(line);
    if (m)
    {
		var timestamp = m[1];
		var text = m[2];
		var start = m[3] == "BEGIN";

		probe = known_probes[text];
		if (!probe)
			known_probes[text] = probe = new Probe();

		var c = probe.color;

		entry = new LogLine(timestamp, text, c, start);
		this.entries.push(entry);

		return true;
    }
    else if (!has_reported_first_error)
		opera.postError('Failed to parse:' + line);

    return false;
}

Entries.prototype.normalize_timestamps = function() {
    var first_timestamp = this.entries[0].timestamp;
    for (entryidx in this.entries)
		this.entries[entryidx].timestamp -= first_timestamp;
}

Entries.prototype.filter_on_time = function(begin, end) {
    var old_entries = this.entries;
    this.entries = [];

    for (entryidx in old_entries)
    {
		if (old_entries[entryidx].timestamp > begin &&
			old_entries[entryidx].timestamp <end)
		{
			this.entries.push(old_entries[entryidx]);
		}
    }
}

function Metrics() {}

Entries.prototype.compute_metrics = function() {
    var num_entries = this.entries.length;
    var metrics = new Metrics();

    metrics.width = 800;
    if (num_entries > 0)
    {
		var first_timestamp = this.entries[0].timestamp;
		var last_timestamp = this.entries[num_entries-1].timestamp;

		metrics.num_seconds = Math.ceil((last_timestamp - first_timestamp) * 10.0) / 10.0;
		metrics.height = Math.min(600, Math.max(metrics.num_seconds * PIXELS_PER_SECOND, num_entries * PIXELS_PER_LINE));
		metrics.start_time = first_timestamp;

		var text_ypos = 0;

		for (entryidx in this.entries)
		{
			entry = this.entries[entryidx];
			entry.timestamp_ypos = (entry.timestamp - first_timestamp) * PIXELS_PER_SECOND;
			entry.log_ypos = text_ypos + FONT_SIZE;
			text_ypos += PIXELS_PER_LINE;
		}
    }
    else
		metrics.height = 0;

    return metrics;
}

function get_data()
{
    var logfile = document.getElementById('logfile').value;
    if (logfile)
    {
		var http = new XMLHttpRequest();
		http.open('GET', logfile, false);
		http.send();
		return http.responseText;
    }
    else
		return "";
}

function plot_time_scale(ctx, metrics)
{
    ctx.fillStyle = "rgb(127, 127, 127)";
    ctx.lineWidth = 1.0;

    // Find sutable multiplier; find a multiple of 10 that matches

    t = 1
    while (t < PIXELS_PER_SECOND)
		t *= 10;

    var mult = t / 100;
    var pixels_between_bars = PIXELS_PER_SECOND / mult;

    while (pixels_between_bars < 60)
    {
		mult /= 2;
		pixels_between_bars = PIXELS_PER_SECOND / mult;
    }

    for (var i=0; i<=Math.floor(metrics.num_seconds * mult + 1); i++)
    {
        var ypos = i * pixels_between_bars;

        ctx.beginPath();
        ctx.moveTo(0, ypos + 0.5);
        ctx.lineTo(TIME_SCALE_WIDTH, ypos + 0.5);
        ctx.stroke();

		var time = (i / mult + metrics.start_time).toFixed(3);

		ctx.fillText(time + " s", 0, ypos + 2 + FONT_SIZE);
    }
}

function plot_entry(ctx, entry)
{
    ctx.fillStyle = to_rgb(entry.color);
    ctx.strokeStyle = to_rgb(entry.color);

    ctx.beginPath();
    ctx.moveTo(TIME_SCALE_WIDTH, entry.timestamp_ypos + 0.5)
    ctx.lineTo(TIME_SCALE_WIDTH + SYSCALL_MARKER_WIDTH, entry.timestamp_ypos + 0.5)
    ctx.lineTo(LOG_TEXT_XPOS - LOG_MARKER_WIDTH, entry.log_ypos - FONT_SIZE / 2 + 0.5)
    ctx.lineTo(LOG_TEXT_XPOS, entry.log_ypos - FONT_SIZE / 2 + 0.5)
    ctx.stroke()

    ctx.fillText(entry.timestamp.toFixed(4) + ": " + (entry.start ? " \u21D2" : " \u21D0") + " " + entry.text + " ", LOG_TEXT_XPOS, entry.log_ypos)
}

function plot_entries(ctx, entries, metrics)
{
    ctx.fillStyle = "rgb(255,255,255)";
    ctx.fillRect(0, 0, metrics.width, metrics.height);

    plot_time_scale(ctx, metrics);

    ctx.lineWidth = 1.0;

    for (entryidx in entries.entries)
    {
		var entry = entries.entries[entryidx];
        plot_entry(ctx, entry);
    }
}

function update_status(has_reported_first_error, has_entries)
{
    var status_element = document.getElementById('status');
    if (stream)
    {
		var start = parseFloat(document.getElementById('start-sec').value);
		var end = start + parseFloat(document.getElementById('dur').value);
		var px_per_sec = parseFloat(document.getElementById('px-per-sec').value);

		var suffix = has_reported_first_error ? " (<b>with errors<b>)" :
			(!has_entries ? " (<b>no found entries</b>)" : "");

		status_element.innerHTML = "Displaying " + start.toFixed(1) + "s - " + end.toFixed(1) + "s, " + px_per_sec.toFixed(1) + " px/sec" + suffix;
    }
    else
    {
		status_element.innerHTML = "Press <b>'Load file'</b> to load probelog.txt from disk";
    }
}

function main()
{
    var canvas = document.getElementById('canvas');
    canvas.style.display = "none";

    var has_reported_first_error = false;
    var has_entries = false;

    if (stream)
    {
		stream.position = 0;

		PIXELS_PER_SECOND = document.getElementById('px-per-sec').value;

		var entries = new Entries();

		while (line = stream.readLine())
			if (!entries.add_line(line, has_reported_first_error))
				has_reported_first_error = true;

		has_entries = entries.has_entries();

		if (has_entries)
		{
			entries.normalize_timestamps();
			entries.filter_on_time(parseFloat(document.getElementById('start-sec').value),
								   parseFloat(document.getElementById('start-sec').value) +
								   parseFloat(document.getElementById('dur').value));

			var metrics = entries.compute_metrics();

			canvas.style.display = "block";
			canvas.width = metrics.width;
			canvas.height = window.innerHeight - 2; // make room for 1px border

			plot_entries(canvas.getContext('2d'), entries, metrics);
		}
    }

    update_status(has_reported_first_error, has_entries);
}

function cback(file)
{
    stream = file.open(null, "r");

    main();
}

function load()
{
    opera.io.filesystem.browseForFile("probelog", "/", cback, false, false, [ ]);
}

function scaleup()
{
    var px_per_sec = document.getElementById('px-per-sec').value;
    px_per_sec = parseInt(px_per_sec) * 1.5;
    document.getElementById('px-per-sec').value = px_per_sec;

    main();
}

function scaledown()
{
    var px_per_sec = document.getElementById('px-per-sec').value;
    px_per_sec = parseInt(px_per_sec) / 1.5;
    document.getElementById('px-per-sec').value = px_per_sec;

    main();
}

function startup()
{
    var dur = document.getElementById('dur').value;
    var start_sec = document.getElementById('start-sec').value;
    start_sec = parseFloat(start_sec) + parseFloat(dur);
    document.getElementById('start-sec').value = start_sec;

    main();
}

function startdown()
{
    var dur = document.getElementById('dur').value;
    var start_sec = document.getElementById('start-sec').value;
    start_sec = parseFloat(start_sec) - parseFloat(dur);

    if (start_sec < 0)
		start_sec = 0;

    document.getElementById('start-sec').value = start_sec;

    main();
}

function durup()
{
    var dur = document.getElementById('dur').value;
    dur = parseFloat(dur) * 2;
    document.getElementById('dur').value = dur;

    main();
}

function durdown()
{
    var dur = document.getElementById('dur').value;
    dur = parseFloat(dur) / 2;
    document.getElementById('dur').value = dur;

    main();
}

onresize = main;
onload = main;
