/**
 * Adds an item with provided values to address book.
 * Returns the item.
 */
function add_address_book_item(title, eMail, homePhone, mobilePhone,  workPhone)
{
  var item = new Widget.PIM.AddressBookItem;
  if(!item)
  {
    cleanup_added_items();
    throw new Error("Failed to create AddressBookItem");
  }
  item.title = title;
  item.eMail = eMail;
  item.homePhone = homePhone;
  item.mobilePhone = mobilePhone;
  item.workPhone = workPhone;

  var id = Widget.PIM.addAddressBookItem(item);
  return Widget.PIM.getAddressBookItem(id);
}

/**
 * Removes items from device's address book.
 */
function remove_address_book_items(items)
{
  for (i in items)
  {
    Widget.PIM.deleteAddressBookItem(items[i].addressBookItemId);
  }
}

/**
 *Test if a file at given path exists. Fails the selftest if it doesnt
 */
function check_file_exists(virtual_path, should_exist)
{
	var file;
	try
	{
		file = Widget.Device.getFile(virtual_path);
	}
	catch (e)
	{
		file = null;
	}
	if (should_exist && !file)
	{
		output("File " + virtual_path + " does't exist\n");
		return false;
	}
	if (!should_exist && file)
	{
		output("File " + virtual_path + " exist but it shouldn't\n");
		return false;
	}
	return true;
}
