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