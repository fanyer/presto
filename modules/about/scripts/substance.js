var globals = {}

globals.selectedStyle = 'color:#333;background:#dd7;border-top:1px solid #994;margin-top:-0px;';
globals.forceRedraw = function(){ document.body.style.display = 'none';document.body.style.display = ''; }




/*
 *	feedsManager
 */
var	feedsManager = new function()
{
	var	that		= this;


	this.FEED_STATUS =
	{
		STATUS_OK					:0
		,STATUS_ABORTED				:1
		,STATUS_REFRESH_POSTPONED	:2
		,STATUS_NOT_MODIFIED		:3
		,STATUS_OOM					:4
		,STATUS_SERVER_TIMEOUT		:5
		,STATUS_LOADING_ERROR		:6
		,STATUS_PARSING_ERROR		:7

		,0							:{type:'ok',	constant:'STATUS_OK',					text:'ok'}
		,1							:{type:'error',	constant:'STATUS_ABORTED',				text:'aborted'}
		,2							:{type:'ok',	constant:'STATUS_REFRESH_POSTPONED',	text:'refresh postponed'}
		,3							:{type:'ok',	constant:'STATUS_NOT_MODIFIED',			text:'not modified'}
		,4							:{type:'error',	constant:'STATUS_OOM',					text:'out of memory'}
		,5							:{type:'error',	constant:'STATUS_SERVER_TIMEOUT',		text:'server time out'}
		,6							:{type:'error',	constant:'STATUS_LOADING_ERROR',		text:'loading error'}
		,7							:{type:'error',	constant:'STATUS_PARSING_ERROR',		text:'parsing error'}
	}

	this.FEED_ENTRY_STATUS =
	{
		STATUS_UNREAD				:0
		,STATUS_READ				:1
		,STATUS_ERROR				:2
		,0							:{type:'ok',	constant:'STATUS_UNREAD',				text:'unread'}
		,1							:{type:'ok',	constant:'STATUS_READ',					text:'read'}
		,2							:{type:'error',	constant:'STATUS_ERROR',				text:'error'}
	}

	this.metaDataSeparator	= '{SEPARATOR-GUID-94803865-3691-4a26-83e6-c1f7f461bbb7}';


	/*
	 *	populateFeedReadersList
	 *	populates the feed readers list
	 */
	this.populateFeedReadersList = function( )
	{
		for ( var i=0; i < $('readers').length; ++i )
			$('readers').remove(i);
		$('readers').appendChild( new Option("Opera", "Opera") );
		for ( var i=0; i < opera.feedreaders.length; i++ )
		{
			var readerEntry = opera.feedreaders[i];
			var externalReaderSubscribeUrl = readerEntry.getTargetURL( location.href ) || '';
			if ( externalReaderSubscribeUrl !== '' )
			{
				$('readers').appendChild( new Option(readerEntry.name, externalReaderSubscribeUrl) );
			}
		}
	}

	/*
	 *	subscribeFeed
	 *	subscribe the feed (with Opera or chosen reader)
	 */
	this.subscribeFeed = function( )
	{
		if ( $('readers').value === "Opera" )
		{
			this.toggleSubscription();
			if (this.selectedFeedIds.length == 0)
				$('subscribeButton').disabled = true;
		}
		else
			window.open( $('readers').value );
	}

	/*
	 *	feedReaderChoice
	 *	called when feed readers drop down is changed
	 */
	this.feedReaderChoice = function( )
	{
		if ( $('readers').value === "Opera")
		{
			$('deleteButton').disabled = true;
			if (this.selectedFeedIds.length == 0)
			{
				$('subscribeButton').disabled = true;
				return;
			}
			var feed = opera.feeds.getFeedById( this.selectedFeedIds[0] );
			if ( feed.isSubscribed )
				$('subscribeButton').value = opera.getLocaleString( 'S_MINI_FEED_UNSUBSCRIBE' );
			else
				$('subscribeButton').value = opera.getLocaleString( 'S_MINI_FEED_SUBSCRIBE' );
		}
		else
		{
			$('subscribeButton').disabled = false;
			$('deleteButton').disabled = false;
			$('subscribeButton').value = opera.getLocaleString( 'S_MINI_FEED_SUBSCRIBE' );
		}
	}

	/*
	 *	deleteFeedReader
	 *	deletes choen reader
	 */
	this.deleteFeedReader = function( )
	{
		var index = $('readers').selectedIndex;
		delete opera.feedreaders[index - 1];
		$('readers').remove(index);
		if (this.selectedFeedIds.length == 0)
			$('subscribeButton').disabled = true;
		if ($('readers').value === "Opera")
			$('deleteButton').disabled = true;
	}

	/*
	 *	populateFeedsList
	 *	[create and] populate the feeds group
	 */
	this.populateFeedsList = function( feed )
	{
		//	get feedsListUl
		if( 'undefined'==typeof(this.feedsListUl) )
			this.feedsListUl = $('feedsListUl');
		if( !this.feedsListUl )
			return false;


		//	get	groupName & feedName
		var	meta		= feed.userDefinedTitle.split( feedsManager.metaDataSeparator )
			,feedName	= meta.pop()
			,groupName	= meta.pop();

		if( 0<meta.length )
			groupName	= meta.shift();


		//	grab|create group
		var	groupId			= ('smartGroup_preview'==groupName?groupName:'feedsGroup_'+groupName)
			,groupHandle	= $( groupId );


		//	new group
		if( null==groupHandle )
		{
			groupHandle		= document.createElement( 'li' )
			var	groupA		= document.createElement( 'a' )
				,groupUl	= document.createElement( 'ul' );

			if( 'undefined'==typeof(this.settingsFormHandle) )
				this.settingsFormHandle = $('settingsForm');

			this.settingsFormHandle.moveToGroup.options[ this.settingsFormHandle.moveToGroup.options.length ] = new Option( groupName, groupName );


			groupHandle.id	= groupId;
			groupA.href		= '#feedEntries';
			groupA.appendChild( document.createTextNode( groupName ) );
			groupHandle.appendChild( groupA );
			groupHandle.appendChild( groupUl );
			this.feedsListUl.appendChild( groupHandle );
		}


		//	create feed entry in the group
		var	groupUl			= groupHandle.lastChild
			,feedId			= 'feed_'+ feed.id
			,feedHandle		= $( feedId );

		if( null==feedHandle )
		{
			feedHandle			= document.createElement( 'li' )
			var	feedSettingsA	= document.createElement( 'a' )
				,feedTitleA		= document.createElement( 'a' );

//			if( feed.isSubscribed )
			{
				feedHandle.id		= feedId;
				feedSettingsA.href	= '#settings';
					feedSettingsA.appendChild( document.createTextNode( 'settings' ) );
				feedHandle.appendChild( feedSettingsA );
				feedHandle.appendChild( document.createTextNode(' ') );
			}

			feedTitleA.href		= '#feedEntries';
				feedTitleA.appendChild( document.createTextNode( feedName ) );
				feedTitleA.title = feedName;
			if( ''!=feed.icon )
				feedTitleA.style.backgroundImage = 'url('+ feed.icon +')';
			feedHandle.appendChild( feedTitleA );

			groupUl.appendChild( feedHandle );
		}
	}
	/*
	 *	handleFeedsListClick
	 *	all things clicky in the feedsList
	 */
	this.handleFeedsListClick = function( event )
	{
		var src		= event.srcElement
			,dst
			,feedEntryId
			,isInit   = 'undefined'!=typeof(this.firstFeedEntryFound);

		while( null!=document && !src.hasAttribute('href') )
			src = src.parentNode;

		if( document==src )
			return false;

		dst = src.getAttribute('href');
		while( null==src.getAttribute('id') )
			src = src.parentNode;

		feedEntryId = src.getAttribute('id');

		this.populateFeedEntries( feedEntryId );
		this.filterFeedEntries( true );


        if( !isInit && 'undefined'!=typeof(this.firstFeedEntryFound) )
        {
        	var evt = document.createEvent( 'UIEvents' );
        	evt.initUIEvent( 'click', true, true, window, 1 );
        	$(this.firstFeedEntryFound).firstChild.nextSibling.firstChild.dispatchEvent( evt );
        }


		if( '#settings'==dst )
		{
			var	tmp  	= feedEntryId.split( '_' )
				,type	= tmp.shift()
				,id		= tmp.pop()
				,feed	= opera.feeds.getFeedById( id );

			if( !feed || !feed.isSubscribed )
			{
				event.preventDefault();
				return false;
			}


			var meta		= feed.userDefinedTitle.split( this.metaDataSeparator )
				,feedName	= meta.pop()
				,groupName	= meta.pop()

			var tmp = $('settings').$$('h1')[0];
			tmp.title = tmp.lastChild.firstChildnodeValue = feedName;


			if( 'undefined'==typeof(this.settingsFormHandle) )
				this.settingsFormHandle = $('settingsForm');

			//	populate form
			this.settingsFormHandle.feedName.value			= feedName;
			this.settingsFormHandle.moveToNewGroup.value	= '';
			this.settingsFormHandle.showImages.checked		= feed.showImages;

			//	select group
			for( var i=0;currentOption=this.settingsFormHandle.moveToGroup.options[i]; i++ )
			{
				currentOption.selected = currentOption.value==groupName;
				currentOption.style.color = currentOption.selected?'#000':'#999';
			}

			//	select maxAge
			for( var i=0;currentOption=this.settingsFormHandle.maxAge.options[i]; i++ )
			{
				currentOption.selected = parseFloat(currentOption.value)==(feed.maxAge||opera.feeds.maxAge);
				currentOption.style.fontWeight = currentOption.selected?'bold':'normal';
			}

			//	select maxEntries
			for( var i=0;currentOption=this.settingsFormHandle.maxEntries.options[i]; i++ )
			{
				currentOption.selected = parseInt(currentOption.value)==(feed.maxEntries||opera.feeds.maxEntries);
				currentOption.style.fontWeight = currentOption.selected?'bold':'normal';
			}

			//	select updateInterval
			for( var i=0;currentOption=this.settingsFormHandle.updateInterval.options[i]; i++ )
			{
				currentOption.selected = parseInt(currentOption.value)==(feed.updateInterval||opera.feeds.updateInterval);
				currentOption.style.fontWeight = currentOption.selected?'bold':'normal';
			}

		}

		return false;
	}
	/*
	 *	handleEntriesListClick
	 *	all things clicky in the entriesList
	 */
	this.handleEntriesListClick = function( event )
	{
		var src		= event.srcElement
			,feedAndEntryId;


		while( 'TD'!=src.nodeName )
			src = src.parentNode;

		dst = src;
		while( 'TR'!=dst.nodeName )
			dst = dst.parentNode;

		var feedAndEntryId = dst.getAttribute('id');

		this.displayFeedEntry( feedAndEntryId );
	}
	/*
	 *	handleEntriesControlsClick
	 *	all things clicky in the entriesControls
	 */
	this.handleEntriesControlsClick = function( event )
	{
		var src		= event.srcElement;
		while( src && document!=src )
		{
			if( src.hasAttribute('href') )
			{
				switch( src.getAttribute('href') )
				{
					case '#subscriptionToggle':
					{
						return !this.toggleSubscription();
					}
					default:
					{
						break;
					}
				}
				return false;
			}
			src = src.parentNode;
		}
		return false;
	}
	/*
	 *	handleEntryControlsClick
	 *	all things clicky in the entryControls
	 */
	this.handleEntryControlsClick = function( event )
	{
		var src		= event.srcElement;
		while( src && document!=src )
		{
			if( src.hasAttribute('href') )
			{
				switch( type=src.getAttribute('href') )
				{
					case '#feedEntryToggleFlag':
					{
						return !this.feedEntryToggleFlag();
					}
					default:
					{
						break;
					}
				}
				return false;
			}
			src = src.parentNode;
		}
		return false;
	}
	/*
	 *	handleSettingsFormSubmit
	 */
	this.handleSettingsFormSubmit = function( event )
	{
		if( 'validateSettings'==document.activeElement.id )
		{
			var	feed			= opera.feeds.getFeedById( this.selectedFeedIds[0] )
				,meta			= feed.userDefinedTitle.split( this.metaDataSeparator )
				,oldFeedName	= meta.pop()
				,oldGroupName	= meta.pop()
				,feedLi			= $( 'feed_'+ feed.id)

				,feedName		= this.settingsFormHandle.feedName.value.trim()||oldFeedName
				,groupName		= this.settingsFormHandle.moveToNewGroup.value.trim()
									||this.settingsFormHandle.moveToGroup.options[ this.settingsFormHandle.moveToGroup.selectedIndex ].value.trim()



			//	userDefinedTitle
			feed.userDefinedTitle = [ groupName, feedName ].join( this.metaDataSeparator );

			//	feedName
			if( feedName!=oldFeedName )
			{
				var tmp	= $('feedEntries').$$('h1')[0]

				tmp.title 								=
				tmp.firstChild.nodeValue				=
				feedLi.lastChild.title					=
				feedLi.lastChild.lastChild.nodeValue	= feedName;
			}

			//	showImages
			feed.showImages	= this.settingsFormHandle.showImages.checked;

			//	maxAge
			feed.maxAge		= parseFloat( this.settingsFormHandle.maxAge.options[ this.settingsFormHandle.maxAge.selectedIndex ].value );

			//	maxEntries
			feed.maxEntries	= parseInt( this.settingsFormHandle.maxEntries.options[ this.settingsFormHandle.maxEntries.selectedIndex ].value );

			//	updateInterval
			feed.updateInterval	= parseInt( this.settingsFormHandle.updateInterval.options[ this.settingsFormHandle.updateInterval.selectedIndex ].value );


			//	groupName
			if( groupName!=oldGroupName )
			{
				if( 1==feedLi.parentNode.children.length )
					feedLi.parentNode.parentNode.parentNode.removeChild( feedLi.parentNode.parentNode );
				else
					feedLi.parentNode.removeChild( feedLi );

				this.populateFeedsList( feed );
				$('feed_'+feed.id).style = globals.selectedStyle;
				globals.forceRedraw();
			}

		}

		document.location = '#feedsList'
		event.preventDefault();
		return false;
	}



	/*
	 *	feedEntryToggleFlag
	 */
	this.feedEntryToggleFlag = function()
	{
		if( 'undefined'==typeof(this.feedAndEntryId) )
			return false;

		var tmp 		= this.feedAndEntryId.split( '_' )
			,feedEntryId= tmp.pop()
			,feedId		= tmp.pop()
			,feed		= opera.feeds.getFeedById( feedId )


		if( 'undefined'==typeof(feed) )
			return false;

		for( var i=0; feedEntry=feed.entries[i]; i++ )
		{
			if( feedEntryId!=feedEntry.id )
				continue;

			//	toggle Flag
			feedEntry.keep = !feedEntry.keep;

			//	un|flag text
//			$('feedEntryToggleFlag').firstChild.nodeValue = feedEntry.keep?'unflag':'flag';

			$(this.feedAndEntryId).firstChild.style.backgroundPosition = feedEntry.keep?'center center':'';


			//	filter list of entries
			this.filterFeedEntries( true );

			event.preventDefault();
			return true;
		}
		return false;

	}


	/*
	 *	toggleSubscription
	 */
	this.toggleSubscription = function()
	{
		var	feed				= opera.feeds.getFeedById( this.selectedFeedIds[0] )
			,msg				= $(feed.isSubscribed?'unsubscribeConfirm':'subscribeConfirm').firstChild.nodeValue;

		if( confirm( msg[0].toUpperCase()+msg.slice(1) ) )
		{
			if( !feed.isSubscribed )
			{
				//	SUBSCRIBE -> move preview to the Misc folder, etc...
				var feedLi	= $('feed_'+ this.selectedFeedIds[0] )
					,feedUl	= feedLi.parentNode;

				delete this.previewedFeedId;

				//	subscribe
				feed.subscribe();

				//	delete feedLi
				feedUl.removeChild( feedLi );

				//	move into 'misc' ( the default folder )
				var metaData	= feed.userDefinedTitle.split( this.metaDataSeparator );
				metaData[0] = $('defaultGroup').firstChild.nodeValue;
				feed.userDefinedTitle = metaData.join( this.metaDataSeparator );

				this.populateFeedsList( feed );

				if ( $('subscribeButton') )
					$('subscribeButton').value = opera.getLocaleString( 'S_MINI_FEED_UNSUBSCRIBE' );
			}
			else
			{
				//	UNSUBSCRIBE -> delete LI in feedsListUl
				var feedLi	= $('feed_'+ this.selectedFeedIds[0] )
					,feedUl	= feedLi.parentNode;

				//	unsubscribe
				feed.unSubscribe();

				//	delete selectedFeedLiId
				delete this.selectedFeedLiId;

				//	find neighbourFeedLi in the current group
				var neighbourFeedLi = null;

				if( feedLi!=feedUl.lastChild )
					neighbourFeedLi = feedLi.nextSibling;
				else if( feedLi!=feedUl.firstChild && !neighbourFeedLi )
					neighbourFeedLi = feedUl.firstChild;

				//	delete feedLi
				feedUl.removeChild( feedLi );

				//	resort to find neighbourFeedLi in the sibling groups
				if( 0==feedUl.children.length )
				{
					//	no more feed in this group
					var feedGroupLi = feedUl;
					while( 'li'!=feedGroupLi.nodeName.toLowerCase() )
						feedGroupLi = feedGroupLi.parentNode;

					//	find neighbourFeedLi
					if( feedGroupLi.nextSibling )
					{
						//	in the nextSibling ?
						var feedGroupSplitId	= feedGroupLi.nextSibling.id.split( '_' )
							,type				= feedGroupSplitId.shift()

						if( 'feed'==type || 'smartGroup'==type )
							neighbourFeedLi = feedGroupLi.nextSibling;
						else
						{
							tmp = feedGroupLi.nextSibling.$$('li');
							if( tmp.length )
								neighbourFeedLi = tmp[0];
						}
					}

					if( !neighbourFeedLi && feedGroupLi.previousSibling )
					{
						//	in the previousSibling ?
						var feedGroupSplitId	= feedGroupLi.previousSibling.id.split( '_' )
							,type				= feedGroupSplitId.shift()

						if( 'feed'==type || 'smartGroup'==type )
							neighbourFeedLi = feedGroupLi.previousSibling;
						else
						{
							tmp = feedGroupLi.previousSibling.$$('li');
							if( tmp.length )
								neighbourFeedLi = tmp[tmp.length-1];
						}
					}

					//	remove feedGroupLi
					feedGroupLi.parentNode.removeChild( feedGroupLi );
				}



				//	display the neighbour feed
				if( neighbourFeedLi )
				{
					var feedEntryId = neighbourFeedLi.id;
					this.populateFeedEntries( feedEntryId );
					this.filterFeedEntries( true );
				}

				if ( $('subscribeButton') )
					$('subscribeButton').value = opera.getLocaleString( 'S_MINI_FEED_SUBSCRIBE' );

			}
		}
		event.preventDefault();
		return true;
	}


	/*
	 *	displayFeedEntry
	 */
	this.displayFeedEntry = function( feedAndEntryId )
	{
		if( 'undefined'!=typeof(this.feedAndEntryId) && this.feedAndEntryId==feedAndEntryId )
			return false;

		if( 'undefined'!=typeof(this.feedAndEntryId) )
		{
			var	tmpNode		= $(this.feedAndEntryId)
				,tmpDisplay	= tmpNode?tmpNode.style.display:'';

			if( tmpNode )
			{
				tmpNode.style			= '';
				tmpNode.style.display	= tmpDisplay
			}
		}

		this.feedAndEntryId = feedAndEntryId;

		var tmp 		= feedAndEntryId.split( '_' )
			,feedEntryId= tmp.pop()
			,feedId		= tmp.pop()
			,feed		= opera.feeds.getFeedById( feedId )


		if( 'undefined'==typeof(feed) )
			return false;

		//	highlight entry in the feedEntriesTable
		$(this.feedAndEntryId).style = globals.selectedStyle
		globals.forceRedraw();

		if( 'undefined'==typeof(this.feedEntryContentHandle) )
			this.feedEntryContentHandle = $('feedEntryContent')

		for( var i=0; feedEntry=feed.entries[i]; i++ )
		{
			if( feedEntryId!=feedEntry.id )
				continue;

			if( 'undefined'==typeof(this.feedEntryHandle) )
				this.feedEntryHandle = $('feedEntry');
			if( this.feedEntryHandle )
			{
				//	display title
				if(  'undefined'==typeof(feedEntry.sanitizedTitle) )
				{
					var dummy = document.createElement('div');
					dummy.appendMarkup( {}, feedEntry.title.data );
					feedEntry.sanitizedTitle = dummy.textContent.trim();
				}
				this.feedEntryHandle.$$('h1')[0].title					=
				this.feedEntryHandle.$$('h1')[0].firstChild.nodeValue	= feedEntry.sanitizedTitle;

				//	set URI of the read full article
				$('feedEntryReadFullArticle').href = feedEntry.uri;

				//	un|flag text
//				$('feedEntryToggleFlag').firstChild.nodeValue = feedEntry.keep?'unflag':'flag';

				//	temporary hide content ( to save the user's eye the incremental cleaning, and update of the content DIV )
				this.feedEntryContentHandle.style.display = 'none';
				$('feedEntryContent').removeChildren();

				//	display content
				if( feedEntry.content.isMarkup )
				{
					if( feed.showImages )
						$('feedEntryContent').appendMarkup( feedEntry.content.data.trim() );
					else
						$('feedEntryContent').appendMarkup( document.body.sanitizerWhiteListWithoutImages, feedEntry.content.data.trim() );
				}
				else if( feedEntry.content.isPlainText )
					$('feedEntryContent').appendChild( document.createTextNode( feedEntry.content.data.trim() ) );
				else
					$('feedEntryContent').appendChild( document.createTextNode( 'BINARY CONTENT: '+ feedEntry.content.data ) );

				//	show the content
				this.feedEntryContentHandle.style.display = '';

				//	mark feedEntry read
				feedEntry.status = feedsManager.FEED_ENTRY_STATUS.STATUS_READ;


				return true;
			}
		}

		return false;
	}



	/*
	 *	selectFeedIdsAndFilters
	 *	select the feed ID and filters based on the feedEntryLi
	 */
	this.selectFeedIdsAndFilters = function( feedEntryLiId )
	{
		var	tmp  	= feedEntryLiId.split( '_' )
			,type	= tmp.shift()
			,id		= tmp.pop()
			,infos	= []


		if( 'feed'==type )
		{
			//	feed::
			if( 'undefined'==typeof( opera.feeds.getFeedById( id ) ) )
				return false;

			this.selectedFeedIds	= [ id ];
			this.selectedFeedFilter	= '';


			var feed = opera.feeds.getFeedById( id );
			id = feed.userDefinedTitle.split( this.metaDataSeparator).pop();
			infos.push( ''!=feed.author?feed.author:feed.uri );
		}
		else if( 'feedsGroup'==type )
		{
			//	feedsGroup::
			this.selectedFeedIds	= [];
			this.selectedFeedFilter	= '';

			for( var i=0; i<opera.feeds.subscribedFeeds.length; i++ )
				if( id==opera.feeds.subscribedFeeds.item( i ).userDefinedTitle.split( this.metaDataSeparator ).shift() )
				{
					this.selectedFeedIds.push( opera.feeds.subscribedFeeds.item( i ).id );
					infos.push( opera.feeds.subscribedFeeds.item( i ).userDefinedTitle.split( this.metaDataSeparator).pop() );
				}

			id = id +' feeds';
		}
		else if( 'smartGroup'==type )
		{
			//	smartGroup::
			this.selectedFeedIds	= [];
			this.selectedFeedFilter	= id;
			if( 'preview'==id )
			{
				//	TODO:	only include the unsubscribed feed
				id = 'feeds preview';
				feed = {author:'?!',uri:'#'}
				infos.push( ''!=feed.author?feed.author:feed.uri );
			}
			else
			{
				infos	= ['all subscribed feeds'];
				//	all feeds, the entries will be filtered later
				for( var i=0; i<opera.feeds.subscribedFeeds.length; i++ )
					this.selectedFeedIds.push( opera.feeds.subscribedFeeds.item( i ).id );

				id = id +' feeds';
			}
		}



		//	en/disable filters
		switch( this.selectedFeedFilter )
		{
			case 'flagged':
			{
				this.feedEntriesControls.unread.checked = false;
				this.feedEntriesControls.unread.setAttribute( 'disabled', 'disabled' );
				this.feedEntriesControls.flagged.setAttribute( 'disabled', 'disabled' );
				this.feedEntriesControls.flagged.checked = true;
				break;
			}
			case 'unviewed':
			{
				this.feedEntriesControls.unread.checked = true;
				this.feedEntriesControls.unread.setAttribute( 'disabled', 'disabled' );
				this.feedEntriesControls.flagged.setAttribute( 'disabled', 'disabled' );
				this.feedEntriesControls.flagged.checked = false;
				break;
			}
			default:
			{
				this.feedEntriesControls.unread.checked &= 'disabled'!=this.feedEntriesControls.unread.getAttribute('disabled');
				this.feedEntriesControls.unread.removeAttribute( 'disabled' );
				this.feedEntriesControls.flagged.checked &= 'disabled'!=this.feedEntriesControls.flagged.getAttribute('disabled');
				this.feedEntriesControls.flagged.removeAttribute( 'disabled' );
				break;
			}
		}


		$('feedEntries').getElementsByTagName('h1')[0].firstChild.nodeValue	=
		$('feedEntries').getElementsByTagName('h1')[0].title				= id;


		//	feed(s) infos + [un]subscribe button
		var feedEntriesSubscriptionHandle	= $('feedEntriesSubscription')
			,feedEntriesInfosHandle			= $('feedEntriesInfos')

		if( !feedEntriesInfosHandle )
			return false;

		feedEntriesInfosHandle.title				=
		feedEntriesInfosHandle.firstChild.nodeValue	= infos.join( ', ' );
		if( feedEntriesSubscriptionHandle )
		{
			if( 1==this.selectedFeedIds.length )
			{
				var theFeed = opera.feeds.getFeedById( this.selectedFeedIds[0] );
				feedEntriesSubscriptionHandle.style.visibility		= 'visible';
				feedEntriesSubscriptionHandle.lastChild.style.display	= theFeed.isSubscribed?'':'inline';
				feedEntriesSubscriptionHandle.firstChild.style.display	= theFeed.isSubscribed?'inline':'';
			}
			else
			{
				feedEntriesSubscriptionHandle.style.visibility		= 'hidden';
			}
		}

		return true;
	}


	/*
	 *	populateFeedEntries
	 *	...
	 */
	this.populateFeedEntries = function( feedEntryLiId )
	{
		//	get feedEntriesTable
		if( 'undefined'==typeof(this.feedEntriesTableBody) )
			this.feedEntriesTableBody = $('feedEntriesTableBody');
		if( !this.feedEntriesTableBody )
			return false;

		//	get feedEntriesControls
		if( 'undefined'==typeof(this.feedEntriesControls) )
			this.feedEntriesControls = $('feedEntriesControls');
		if( !this.feedEntriesControls )
			return false;

		//	get selectedFeedLiId and clean the feedEntriesTable if necessary
		if( 1==arguments.length )
		{
			if( this.selectedFeedLiId==feedEntryLiId )
				return true;

			if( 'undefined'!=typeof(this.selectedFeedLiId) )
			{
				$(this.selectedFeedLiId).style = '';
			}

			//	clean feedEntriesTable
			this.feedEntriesTableBody.removeChildren();
			this.selectedFeedLiId	= feedEntryLiId;
			$(this.selectedFeedLiId).style = globals.selectedStyle;
			globals.forceRedraw();
		}


		//	selectFeedIdsAndFilters
		this.selectFeedIdsAndFilters( this.selectedFeedLiId );

		var	yesterday	= new Date().valueOf()-1000*60*60*24;

		//	go through all the selectedFeeds
		for( var i=0; currentFeedId=this.selectedFeedIds[i]; i++ )
		{
		    var currentFeed = opera.feeds.getFeedById( currentFeedId );
		    if( !currentFeed )
		        continue;

			//	go through all their feedEntries
			for( var j=0,feedEntry; feedEntry=currentFeed.entries[j]; j++ )
			{
				feedEntry.publicationDateInMs = Date.parse( feedEntry.publicationDate );

				if(  'undefined'==typeof(feedEntry.sanitizedTitle) )
				{
					var dummy = document.createElement('div');
					dummy.appendMarkup( {}, feedEntry.title.data );
					feedEntry.sanitizedTitle = dummy.textContent.trim();
					delete dummy;
				}



				var	entryTr		= document.createElement('tr')
					,tmp		= new Date(feedEntry.publicationDateInMs)
					,entryTrId = 'item_'+('00000000000000000000000000000000'+feedEntry.publicationDateInMs).slice(-32)
									+'_'+this.selectedFeedIds[i]
									+'_'+feedEntry.id;

				entryTr.id = entryTrId;
				entryTr.style.display		= 'none';

				//	un|read status
				entryTr.style.fontWeight	= this.FEED_ENTRY_STATUS.STATUS_UNREAD==feedEntry.status?'bold':'normal';


				//	flag
				entryTr.appendChild( document.createElement('td') );
				entryTr.lastChild.appendChild( document.createTextNode( ' ' ) );
				entryTr.lastChild.style.backgroundPosition = feedEntry.keep?'center center':'';


				//	title
				entryTr.appendChild( document.createElement('td') );

				entryTr.lastChild.appendChild( document.createElement('a') );
				entryTr.lastChild.lastChild.appendChild( document.createTextNode( feedEntry.sanitizedTitle ) );
				entryTr.lastChild.lastChild.title = feedEntry.sanitizedTitle;
				entryTr.lastChild.lastChild.href='#feedEntry'


				//	Date
				entryTr.appendChild( document.createElement('td') );
				entryTr.lastChild.appendChild( document.createTextNode( yesterday>feedEntry.publicationDateInMs?tmp.getYear()+'/'+('0'+(1+tmp.getMonth())).slice(-2)+'/'+('0'+tmp.getDate()).slice(-2):('0'+tmp.getHours()).slice(-2)+':'+('0'+tmp.getMinutes()).slice(-2) ));


				//	insert the tr in place
				var tmp = this.feedEntriesTableBody.firstChild
				while( null!==tmp && tmp.id>entryTrId )
					tmp = tmp.nextSibling;


				if( !tmp )
					this.feedEntriesTableBody.appendChild( entryTr );
				else
					this.feedEntriesTableBody.insertBefore( entryTr, tmp );
			}
		}
/*
		<tr><td>&nbsp;</td><td><a href='#feedEntry' onclick='populateFeedEntries("1.0")'>Headline 1</a></td><td>12:34</td></tr>
*/
		return true;
	}



	/*
	 *	filterFeedEntries
	 *	...
	 */
	this.filterFeedEntries = function( forceDisplay )
	{
		if( 'undefined'==typeof(this.selectedFeedLiId) )
			return false;


		//	filter the feed entries
		var	flagged	= this.feedEntriesControls.flagged.checked
			,unread	= this.feedEntriesControls.unread.checked
			,search	= this.feedEntriesControls.search.value.trim().toLowerCase();


		//	check if the anything changed
		if( 'undefined'==typeof(this.feedEntriesFilters) )
			this.feedEntriesFilters = {'flagged':!flagged,'unread':!unread,'search':'!'+search};

		if( !forceDisplay
			&& this.feedEntriesFilters.flagged	==flagged
			&& this.feedEntriesFilters.unread	==unread
			&& this.feedEntriesFilters.search	==search )
			return false;

		this.feedEntriesFilters.flagged	=flagged;
		this.feedEntriesFilters.unread	=unread;
		this.feedEntriesFilters.search	=search;



		var tr			= feedEntriesTableBody.getElementsByTagName('tr')
			,zebra		= true
			,trMatching	= 0;


		//	filter - go through all the selectedFeeds
		for( trIndex=0; currentTr=tr[trIndex]; trIndex++ )
		{
			var	trMeta			= currentTr.id.split('_')
				,entryId		= trMeta.pop()
				,feedId			= trMeta.pop()
				,isFilteredOut	= true;


			if( -1!=('_'+this.selectedFeedIds.join('_')+'_').indexOf('_'+feedId+'_') )
			{
				var	currentFeed = opera.feeds.getFeedById( feedId );
				for( var j=0; feedEntry=currentFeed.entries[j]; j++ )
				{
					if( entryId!=feedEntry.id )
						continue;

					var feedEntryTitle	= currentTr.firstChild.nextSibling.firstChild.title
						,searchPosition	= feedEntryTitle.toLowerCase().indexOf(search)
						,isFilteredOut	= ( this.FEED_ENTRY_STATUS.STATUS_ERROR==feedEntry.status )
							||	( true==unread	&& this.FEED_ENTRY_STATUS.STATUS_UNREAD!=feedEntry.status )
							||	( true==flagged	&& true!=feedEntry.keep )
							||	( ''!=search && -1==searchPosition );


					if( !isFilteredOut )
					{

						trMatching++;
						zebra =! zebra;

						var	tmp	 			= currentTr.firstChild.nextSibling.firstChild.innerHTML
							,matchSearch	= ( ''!=search && -1!=searchPosition );
						if( matchSearch )
							var	feedEntryTitle = feedEntryTitle.slice( 0, searchPosition )+'<em>'+ search +'</em>'+ feedEntryTitle.slice( searchPosition+search.length );

						if( tmp!=feedEntryTitle )
						{
							currentTr.firstChild.nextSibling.firstChild.removeChildren();
							if( matchSearch )
								currentTr.firstChild.nextSibling.firstChild.appendMarkup( feedEntryTitle );
							else
								currentTr.firstChild.nextSibling.firstChild.appendChild( document.createTextNode( tr[ trIndex ].firstChild.nextSibling.firstChild.title ) );
						}

						if( zebra )
							currentTr.setAttribute('rel','zebra');
						else
							currentTr.removeAttribute('rel');
					}
				}
			}


            //  firstFeedEntryFound
            if( !isFilteredOut && 'undefined'==typeof(this.firstFeedEntryFound) )
                this.firstFeedEntryFound    = currentTr.id;

			//	show/hide
			var newDisplay = isFilteredOut?'none':'';
			if( currentTr.style.display!= newDisplay )
				currentTr.style.display = newDisplay;
		}
		$('noFeedEntries').style.display = trMatching?'none':'';
/*
		<tr><td>&nbsp;</td><td><a href='#feedEntry' onclick='populateFeedEntries("1.0")'>Headline 1</a></td><td>12:34</td></tr>
*/
		return true;
	}









	/*
	 *	feedListener
	 */
	this.feedListener = new function()
	{
		/*
		 *	feedListener.entryLoaded
		 *	called when a FeedEntry is loaded
		 */
		this.entryLoaded = function( feedEntry, status )
		{
			if( 'error'==feedsManager.FEED_STATUS[ status ].type )
				opera.postError( typeof(feedEntry) +' feedListener.entryLoaded event fired'+ '\n \n'+ feedsManager.FEED_STATUS[ status ].type +':'+ feedsManager.FEED_STATUS[ status ].text ) //.userDefinedTitle +' -- '+ typeof(feedEntry) );
		}
		/*
		 *	feedListener.feedLoaded
		 *	called when a Feed is loaded
		 */
		this.feedLoaded	= function( feed, status )
		{
			var message = feedsManager.FEED_STATUS[ status ].type +':'+ feedsManager.FEED_STATUS[ status ].text
			if( 'error'==feedsManager.FEED_STATUS[ status ].type )
				opera.postError( 'FEED LOADING ERROR: '+ message );
			if( null==feed )
			{
				return false;
			}



			//	check metaData ()
			metaData = feed.userDefinedTitle.split( that.metaDataSeparator );
			if( 2!=metaData.length || !feed.isSubscribed)
			{
				var dummy = document.createElement( 'div' );
				dummy.appendMarkup( {}, feed.title.data );
				feed.sanitizedTitle = dummy.textContent.trim();

				metaData = [ $('defaultGroup').firstChild.nodeValue, feed.sanitizedTitle ];
				feed.userDefinedTitle = metaData.join( that.metaDataSeparator );
			}
			else
			{
				feed.sanitizedTitle = metaData[1];
			}
			if( !feed.isSubscribed )
			{
				feed.userDefinedTitle = [ 'smartGroup_preview', feed.sanitizedTitle ].join( that.metaDataSeparator );
				that.previewedFeedId = feed.id
			}



			//	populateFeedsList
			that.populateFeedsList( feed );

		}
		/*
		 *	feedListener.updateFinished
		 *	called when all the Feeds are updated
		 */
		this.updateFinished	= function()
		{
			//	go through all the subscribedFeed
			//	update/set their metaData
			//	populate the leist of feeds
			for( var i=0,subscribedFeed; subscribedFeed=opera.feeds.subscribedFeeds[i]; i++ )
				this.feedLoaded( subscribedFeed, subscribedFeed.status );
		}
	}
}






/*
 *	initialize
 */
function initialize()
{
	//	WebFeeds module available ?
	if( 'undefined'==typeof(opera.feeds) )
	{
//		alert( 'WebFeeds module not available' );
		return false;
	}


	//	attach event listeners
	$('feedsListUl').addEventListener( 'click'
		,function(event){ return feedsManager.handleFeedsListClick.apply(feedsManager,arguments) }
		,false );

	$('feedEntryControls').addEventListener( 'click'
		,function(event){ return feedsManager.handleEntryControlsClick.apply(feedsManager,arguments) }
		,false );

	$('feedEntriesTableBody').addEventListener( 'click'
		,function(event){ return feedsManager.handleEntriesListClick.apply(feedsManager,arguments) }
		,false );

	$('feedEntriesControls').elements['search'].addEventListener( 'keyup'
		,function(event){ return feedsManager.filterFeedEntries() }
		,false );

	$('feedEntries').addEventListener( 'change'
		,function(event){ return feedsManager.filterFeedEntries() }
		,false );

	$('feedEntries').$$('div')[1].addEventListener( 'click'
		,function(event){ return feedsManager.handleEntriesControlsClick.apply(feedsManager,arguments) }
		,false );

	$('settingsForm').addEventListener( 'submit'
		,function(event){ return feedsManager.handleSettingsFormSubmit.apply(feedsManager,arguments) }
		,false );




	//	display the cached version of the feeds
	feedsManager.feedListener.updateFinished();

	//	update the feeds
	setInterval( function(){ opera.feeds.updateFeeds( feedsManager.feedListener ); }, 1000*60*5 );

	//	defaultElementDisplayed
	var defaultElementDisplayed = document;
	if( 0!=opera.feeds.subscribedFeeds.length )
		defaultElementDisplayed = $( 'feed_'+ opera.feeds.subscribedFeeds[0].id ).lastChild;

	//	populate/display previewed feed
	if( 'undefined'!=typeof(feedsManager.previewedFeedId) && Infinity!=feedsManager.previewedFeedId )
	{
		var previewedFeed = opera.feeds.getFeedById( feedsManager.previewedFeedId );
		previewedFeed.userDefinedTitle = previewedFeed.title.data;
		feedsManager.feedListener.feedLoaded( previewedFeed, previewedFeed.status );
		feedsManager.populateFeedsList( previewedFeed );
		defaultElementDisplayed = $( 'feed_'+ feedsManager.previewedFeedId ).lastChild;
	}


//	if( defaultElementDisplayed!=document )
	{
    	var evt = document.createEvent( 'UIEvents' );
    	evt.initUIEvent( 'click', true, true, window, 1 );
    	defaultElementDisplayed.dispatchEvent( evt );
    }






	return true;
}


onload = initialize