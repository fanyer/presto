/* Created by Mathieu Henri <p01@opera.com>
 * Grabbed from http://people.opera.com/p01/codingGuidelines/createToc.js
 * Modified by Peter Krefting <peter@opera.com>
 */

/*
 *	createToc
 *	build a UL with the 'Table Of Content' based on the H2-6 and append it right before the first H2 or the element #toc
 */
function createToc()
{
	//	grab/create the h2 #toc
	var tocH2	= document.getElementById('toc');

	//	grab all the nodes
	var all			= document.body.getElementsByTagName('*'),
		tocUl		= document.createElement('ul'),
		i			= 0,
		tocIds		= {},
		nHeaders	= [0,0,0,0,0,0,0],
		foundToc	= false;


	//	crawl the h2-6
	while( currentElement = all[i++] )
	{
		//	not an H2-6
		if( !currentElement.nodeName.match( /H[2-6]/ ) )
			continue;

		//	toc
		if( !tocIds[currentElement.textContent] )
			tocIds[currentElement.textContent]=0

		if (!currentElement.id || currentElement.id == "")
			currentElement.id	= currentElement.textContent +(tocIds[currentElement.textContent]?'_'+tocIds[currentElement.textContent]:'');

		var level = parseInt(currentElement.nodeName.slice(1));

		nHeaders[level]++;
		for( var j=level+1; j<7; j++ )
			nHeaders[j]=0;

		var newLabel = nHeaders.slice( 2, level+1 ).join( '.' )+'. '+currentElement.textContent;

		tocUl.appendChild( document.createElement('li') );
		tocUl.lastChild.appendChild( document.createElement('a') );
		tocUl.lastChild.lastChild.href = '#'+ currentElement.id;
		tocUl.lastChild.lastChild.appendChild( document.createTextNode( newLabel ) );
		tocUl.lastChild.lastChild.style.paddingLeft = (level - 1)+'em';

		tocIds[currentElement.textContent]++;
	}
	tocUl.id	= 'tocUl';


	//	insert the tocUl after the tocH2
	if( tocH2.nextSibling )
		document.body.insertBefore( tocUl, tocH2.nextSibling );
	else
		document.body.appendChild( tocUl );

	return true;
}


/*
 *	onload initialization
 */
onload = function()
{
	createToc();
}

