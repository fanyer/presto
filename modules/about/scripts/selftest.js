//-------------------------------------------------
//VARIABLES!
//-------------------------------------------------
var g_numSelectedTests = 0;
var g_timeoutId = null;

//-------------------------------------------------
//LAYOUT!
//-------------------------------------------------
// Called at startup
function OnLoad()
{
	window.onresize=SetSize;
	SetSize();

	//Default host and fieldvalues for publish is set here!
	document.publishToServer.publishToHost.value = "http://opera.internal...";
	document.publishToServer.publishOutputAsField.value = "outputFromSelftest";

	//If other default values are requested for different fields, they can be set from here!
}

// Called at startup and resize
function SetSize()
{
	var output = document.getElementById("output");
	output.style.height = document.body.offsetHeight - header.offsetHeight - 30;
	output.style.width  = document.body.offsetWidth - 160;

	var testselector = document.getElementById("testselector");
	testselector.style.height = document.body.offsetHeight - header.offsetHeight - 30;
	testselector.style.width = 155;

	document.height = document.body.offsetHeight;
}

// Called when the state of a checkbox is changed
function OnSelectModuleChangedEvent(chk)
{
	if(chk)
		g_numSelectedTests++;
	else
		g_numSelectedTests--;

	// Enable / disable argument input
	document.f.a.disabled = g_numSelectedTests > 0;
}

// Called when enabling testdata override
function OnOverridePathChangedEvent(chk)
{
	// Enable / disable directory input
	if(chk)
		document.f.testdata.disabled = false;
	else
		document.f.testdata.disabled = true;
}
// Called so that we scroll to output
function OnScroll()
{
	if (document.f.s.checked)
		document.getElementById("output").lastChild.scrollIntoView(true);
}

//-------------------------------------------------
//TEST INTEGRATION
//-------------------------------------------------
// Construct array with extra test parameters
function MakeExtraParameters()
{
	var extraParameters = new Array();
		if (document.f.outputfile.value.length > 0)
		extraParameters.push('--test-output=' + document.f.outputfile.value);

		if (document.f.runmanual.checked)
		extraParameters.push('--test-manual');

		/*
		if (document.f.machine.checked)
		extraParameters.push('--test-spartan-readable');
		*/

		if (document.f.override.checked)
		extraParameters.push('--test-testdata=' + document.f.testdata.value);

	return extraParameters;
}

function RunAllOrSelected(onlyRunSelected)
{
	var modulelist = "";
	var elements = document.selectNodes("//div[@id='testselector']/descendant::input[not(@id='runalltests')]");
	for (var i=0, element; element = elements[i]; i++)
	{
		if(element.checked || !onlyRunSelected)
		{
			modulelist += element.id;
			modulelist += ",";
		}
	}


	// Get rid of last ","
	modulelist = modulelist.slice(0, modulelist.length - 1);

	opera.selftestrun.apply(null, [document, OnSelftestResult, "-test-module=" + modulelist].concat(MakeExtraParameters()));
}

//-------------------------------------------------
//TEST OUTPUT
//-------------------------------------------------
function AddText(text)
{
	var scrollie = document.getElementById("output").lastChild;
	var currentNode = scrollie.parentNode.insertBefore(document.createElement("div"), scrollie);
	currentNode.appendChild(document.createTextNode(text));
	SetSize()
}

function AddTextNow()
{
	var text = opera.selftestoutput();
	g_timeoutId = null;
	if (text)
		AddText(text);
	SetSize();
}

//-------------------------------------------------
//RUN + CALLBACK
//-------------------------------------------------
function OnRun()
{
	document.f.r.disabled = true;
	startTime = (new Date).getTime();

	if (document.getElementById("runalltests").checked){
		RunAllOrSelected(false);
	} else if (g_numSelectedTests > 0){
		RunAllOrSelected(true);
	} else {
		opera.selftestrun.apply(null,[document, OnSelftestResult].concat(document.f.a.value.split(/\s+/), MakeExtraParameters()));
	}
}
function OnSelftestResult(done)
{
	if (!done)
	{
		if (g_timeoutId == null)
			g_timeoutId = setTimeout(AddTextNow, 100);
	}
	else
	{
		AddTextNow();
		AddText('Total time: ' + String(((new Date).getTime() - startTime) / 1000) + " seconds.\n");
		document.f.r.disabled = false;
	}

	if (document.f.s.checked){
		var scrollie = document.getElementById("output").lastChild;
		scrollie.scrollIntoView(true);
	}
}

//-------------------------------------------------
//CLEAR OUR VIEW
//-------------------------------------------------
function OnClearOutput()
{
	var node = document.getElementById("output");
	var new_node = node.cloneNode(false);
	var scrollie = document.getElementById("output").lastChild;
	new_node.appendChild(scrollie);
	node.parentNode.replaceChild(new_node, node);
}
//-------------------------------------------------
//PUBLISH:
//-------------------------------------------------
function OnPublish()
{
	//Find output text!
	var output = document.getElementById("output");
	var outputText = "";

	var outputChild = output.firstChild;
	while ( outputChild != output.lastChild )
	{
		outputText = outputText + outputChild.innerText + "\n";
		outputChild = outputChild.nextSibling;
	}

	//Find where and how to send
	var host = document.publishToServer.publishToHost.value;
	var field = document.publishToServer.publishOutputAsField.value;

	//Debug
	//AddText(host);
	//AddText(field);
	//AddText(outputText);

	//Add secret submit element
	var submitElement = document.createElement("input");
	submitElement.setAttribute("name",field);
	submitElement.setAttribute("type","hidden");
	submitElement.setAttribute("value",outputText);
	document.publishToServer.appendChild(submitElement);

	//Publish
	document.publishToServer.method = "POST";
	document.publishToServer.action = host;
	document.publishToServer.submit();

	//Clean!
	document.publishToServer.removeChild(submitElement);
}