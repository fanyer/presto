function jil_check_property(object, property_name, type, default_value_type, readonly)
{
	var verify = function(exp)
	{
		if (!exp)
			throw "verify failed";
	}
	if (!default_value_type)
		default_value_type = 'undefined';
	verify(property_name in object);
	verify(typeof(object[property_name]) == default_value_type);
	if (!readonly)
	{
		// standard assignmets
		object[property_name] = null;
		verify(object[property_name] === null);
		object[property_name] = undefined;
		verify(object[property_name] == null); // don't differentiate between null and undefined - so == and not ===
		switch(type)
		{
			case 'boolean':
				object[property_name] = true;
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == true);
				object[property_name] = false;
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == false);
				// auto conversions
				object[property_name] = 1;
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == true);
				object[property_name] = 0;
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == false);
				object[property_name] = "string";
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == true);
				object[property_name] = "";
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == false);
				break;
			case 'string':
				object[property_name] = "test";
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == "test");
				object[property_name] = "";
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == "");
				// auto conversions
				object[property_name] = 1;
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == "1");
				object[property_name] = 0;
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == "0");
				object[property_name] = true;
				verify(typeof(object[property_name]) == type);
				verify(object[property_name] == "true");
				break;
			default:
				object[property_name] = eval("new " + type)
				verify(object[property_name] instanceof eval(type));
		}
	}
	else
	{
		default_value = object[property_name];
		object[property_name] = null;
		verify(object[property_name] == default_value);
		delete object[property_name];
		verify(object[property_name] == default_value);
		object[property_name] = undefined;
		verify(object[property_name] == default_value);
	}
}

/**
 * Adds an item with provided values to address book.
 * Returns the item.
 */
function add_calendar_item(name, notes, startDate, endDate, alarmed, alarmDate, recurrence)
{
	var item = new Widget.PIM.CalendarItem;
	if(!item)
	{
		cleanup_added_items();
		ST_failed("Failed to create AddressBookItem", "A", "B");
	}
	item.eventName = name;
	item.eventNotes = notes;
	if (typeof startDate != 'undefined')
		item.eventStartTime = startDate;
	if (typeof endDate != 'undefined')
		item.eventEndTime = endDate;
	if (typeof alarmed != 'undefined')
		item.alarmed = alarmed;
	if (typeof alarmDate != 'undefined')
		item.alarmDate = alarmDate;
	if (typeof recurrence != 'undefined')
		item.eventRecurrence = recurrence;

	var id = Widget.PIM.addCalendarItem(item);
	return Widget.PIM.getCalendarItem(id);
}

/**
 * Removes items from device's address book.
 */
function remove_calendar_items(items)
{
	for (i in items)
		Widget.PIM.deleteCalendarItem(items[i].calendarItemId);
}
/* used when debugging */
function pp(item)
{
	output("CalendarItem = [\n" +
	"name = "+ item.eventName +"\n" +
	"notes = "+ item.eventNotes +"\n" +
	"startDate = "+ item.eventStartTime +"\n" +
	"endDate = "+ item.eventEndTime +"\n" +
	"alarmed = "+ item.alarmed +"\n" +
	"alarmDate = "+ item.alarmDate +"\n" +
	"recurrence = "+ item.eventRecurrence +"\n" +
	"]\n"
	)
}


function check_add_remove_calendar_item(fails, name, notes, startDate, endDate, alarmed, alarmDate, recurrence)
{
	var item = new Widget.PIM.CalendarItem
	if (typeof name != 'undefined')
		item.eventName = name
	if (typeof notes != 'undefined')
		item.eventNotes = notes
	if (typeof startDate != 'undefined')
		item.eventStartTime = startDate
	if (typeof endDate != 'undefined')
		item.eventEndTime = endDate
	if (typeof alarmed != 'undefined')
		item.alarmed = alarmed
	if (typeof alarmDate != 'undefined')
		item.alarmDate = alarmDate
	if (typeof recurrence != 'undefined')
		item.eventRecurrence = recurrence

	var add_failed = false
	var id;
	try
	{
		id = Widget.PIM.addCalendarItem(item)
	}
	catch (e)
	{
		add_failed = true
	}
	if (add_failed)
		return fails;

	var retrieved_item = Widget.PIM.getCalendarItem(id)

	var report_error = function(property_name, expected_value) {
		output('expected: ' + expected_value + ', got ' + property_name + ' = ' + retrieved_item[property_name] + '\n');
	}

	Widget.PIM.deleteCalendarItem(id)
	if ((name && retrieved_item.eventName != name) || (!name && retrieved_item.eventName != null)) {
		report_error('eventName', name);
		return false;
	}
	if ((notes && retrieved_item.eventNotes != notes) || (!notes && retrieved_item.eventNotes != null)) {
		report_error('eventNotes', notes);
		return false;
	}
	if ((startDate && (retrieved_item.eventStartTime - startDate != 0)) || (!startDate && (Math.abs((new Date).getTime() - retrieved_item.eventStartTime.getTime()) > 5000)) ) {
		report_error('eventStartTime', startDate);
		return false;
	}
	if ((endDate && (retrieved_item.eventEndTime - endDate != 0)) || (!endDate && (retrieved_item.eventEndTime - retrieved_item.eventStartTime != 0))) {
		report_error('eventEndTime', endDate);
		return false;
	}
	if (!alarmed != !retrieved_item.alarmed) {
		report_error('alarmed', alarmed);
		return false;
	}
	if (alarmDate && (retrieved_item.alarmDate - alarmDate != 0)) {
		report_error('alarmDate', alarmDate);
		return false;
	}
	if (!alarmDate && (retrieved_item.alarmDate - retrieved_item.eventStartTime != 0)) {
		report_error('alarmDate', retrieved_item.eventStartTime);
		return false;
	}
	if ((recurrence && retrieved_item.eventRecurrence != recurrence) || (!recurrence && retrieved_item.eventRecurrence != Widget.PIM.EventRecurrenceTypes.NOT_REPEAT)) {
		report_error('eventRecurrence', recurrence);
		return false;
	}
	return true;
}

