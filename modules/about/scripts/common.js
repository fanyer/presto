function $( id )
{
	return document.getElementById( id );
}
HTMLElement.prototype.$ = function( name )
{
	return this.getElementById( id );
}
HTMLElement.prototype.$$ = function( name )
{
	return this.getElementsByTagName( name );
}
HTMLElement.prototype.removeChildren = function()
{
	while( this.firstChild )
		this.removeChild( this.firstChild );
}
String.prototype.trim = function()
{
	return this.replace( /^\s+|\s+$/g, '' );
}



/*
 *	sanitizerWhiteList
 *	tags + attributes and method to validate them
 */
HTMLElement.prototype.sanitizerWhiteList =
{
	'A':
	{
		'HREF':function()
		{
			this.value = this.value.replace( /\s/g, '' );
			return null!=this.value.match( /https?:\/\//i );
		}
	}
	,'IMG':
	{
		'SRC':function()
		{
			//this.value = this.value.replace( /\s/g, '' );
			return null!=this.value.match( /http:\/\//i );
		}
	}
	,'STRONG':{}
	,'P':{}
	,'EM':{}
	,'H1':{}
	,'H2':{}
	,'H3':{}
	,'H4':{}
	,'H5':{}
	,'H6':{}
	,'BR':{}

	,'HR':{}
	,'TABLE':{}
	,'TR':{}
	,'TH':{}
	,'TD':{}
}
HTMLElement.prototype.sanitizerWhiteListWithoutImages =
{
	'A':
	{
		'HREF':function()
		{
			this.value = this.value.replace( /\s/g, '' );
			return null!=this.value.match( /https?:\/\//i );
		}
	}
	,'STRONG':{}
	,'P':{}
	,'EM':{}
	,'H1':{}
	,'H2':{}
	,'H3':{}
	,'H4':{}
	,'H5':{}
	,'H6':{}
	,'BR':{}

	,'HR':{}
	,'TABLE':{}
	,'TR':{}
	,'TH':{}
	,'TD':{}
}



/**
 *	Prototype extension to simplify adding and sanitizing markup to an element.
 *	arguments = some strings containing the HTML markup to sanitize and append
 */
HTMLElement.prototype.appendMarkup = function( whiteList )
{
	var hasWhiteList = 'object'==typeof(whiteList);

	var	dummy = document.createElement('div');
	dummy.innerHTML = '<hr/>'+Array.prototype.slice.call(arguments,hasWhiteList?1:0).join('')
		.replace( /&gt;/g, '>' )
		.replace( /&lt;/g, '<' )
		.replace( /&amp;/g, '&' )
		.replace( /&nbsp;/g, ' ' )
		.replace( /&quot;/g, '"' )
		.replace( /((\\[nr])+|[\r\n]+)/g, '<br/>' )
		.replace( /<!\[CDATA\[(.*)\]\]>/gi, '$1' ) +'<hr/>' //before.replace( /(<!\[CDATA\[)(.*)(\]\]>)/gi, ' @ $2 @ ' )

	dummy.sanitize( hasWhiteList?whiteList:null );

	//	append DOM tree of dummy in this and remove the empty nodes
	while( dummy.firstChild )
		if( ''==dummy.firstChild.textContent.trim()
			&& 'IMG'!=dummy.firstChild.nodeName.toUpperCase()
			&& 'BR'	!=dummy.firstChild.nodeName.toUpperCase()
//			&& 'HR'	!=dummy.firstChild.nodeName.toUpperCase()
		)
			dummy.removeChild( dummy.firstChild );
		else
			this.appendChild( dummy.firstChild.cloneNode( true ) ),dummy.removeChild( dummy.firstChild );
}




/**
 *	Prototype extension to sanitize the attributes and HTMLElements
 *	whiteList = { 'NODENAME_0':['attribute_0','attribute_1'] }
 */
HTMLElement.prototype.sanitize = function( whiteList )
{
	var whiteList = whiteList||this.sanitizerWhiteList;

	// recurse
	var i=0, currentNode;
	while( currentNode=this.childNodes[i] )
	{
		var nodeName	= currentNode.nodeName.toUpperCase();
		if( currentNode.nodeType!=1 )
		{
			// not an ELEMENT_NODE -> keep it
			i++;
			continue;
		}
		else if( 'undefined'!=typeof(whiteList[nodeName]) )
		{
			//	sanitize the attributes
			for( var j=currentNode.attributes.length,attribute; attribute=currentNode.attributes[--j]; )
				if( 'undefined'	==typeof(whiteList[nodeName][attribute.name.toUpperCase()])
					|| false	==whiteList[nodeName][attribute.name.toUpperCase()].apply(attribute) )
						currentNode.removeAttribute( attribute.name );

			// in the nodeNamesWhiteList -> recurse
			currentNode.sanitize( whiteList );
			i++;
			continue;
		}

		// not in the nodeNamesWhiteList -> push the content here and throw away the currentNode
		while( currentNode.firstChild )
		{
			this.insertBefore( currentNode.firstChild.cloneNode( true ), currentNode );
			currentNode.removeChild( currentNode.firstChild );
		}
		this.removeChild( currentNode );
	}
}