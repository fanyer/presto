/* Used in the two test documents.
   In case with keeping focus, we set display 'none' for iframe, inner scrollable, outer scrollable
   then we set display 'block' or 'inline' in reverse order.
   In case of test with bluring we do blur the input in the iframe then do the same as above,
   but skip the iframe display property changes. */

var skip_iframe_undisplay = false;

function blur_input()
{
	var iframe = document.getElementById("iframe");
	var input = iframe.contentWindow.document.getElementById("input");
	input.blur();
	setTimeout("second();",10);
	skip_iframe_undisplay = true;
}

function first()
{
	var element = document.getElementById("iframe");
	element.style.display = 'none';
	setTimeout("second();",50);
}

function second()
{
	var element = document.getElementById("inner_scrollable");
	element.style.display = 'none';
	setTimeout("third();",50);
}

function third()
{
	var element = document.getElementById("outer_scrollable");
	element.style.display = 'none';
	setTimeout("fourth();",50);
}

function fourth()
{
	var element = document.getElementById("outer_scrollable");
	element.style.display = 'block';
	setTimeout("fifth();",50);
}

function fifth()
{
	var element = document.getElementById("inner_scrollable");
	element.style.display = 'block';
	if (!skip_iframe_undisplay)
		setTimeout("sixth();",50);
	else
		setTimeout("final();",50);
}

function sixth()
{
	var element = document.getElementById("iframe");
	element.style.display = 'inline';
	setTimeout("final();",50);
}

function final()
{
	element = document.getElementById("dummy");
	element.setAttribute("id", "function_executed");
}