/* Copyright Opera Software */
addEvent( window, 'load', load );
var popID;
var tID;
var flash;
function load() {
inMenu = false;
flash = getNode( "flash" );
body = document.getElementsByTagName( "body" )[0];
last = null;
if( getNode( "download" ) == null ) return;
var dnld  = new MenuP( getNode( "download" ));
var buy	  = new MenuP( getNode( "buy" ));
var prds  = new MenuP( getNode( "products" ));
var com   = new MenuP( getNode( "company" ));
// var inv   = new MenuP( getNode( "investors" ));
var sprt  = new MenuP( getNode( "support" ));

dnld.add( new Menu( "Opera Desktop", "/download/" ) );
dnld.add( new Menu( "Opera Mini", "/products/mobile/operamini/phones/" ) );
dnld.add( new Menu( "Opera Mobile", "/products/mobile/download/" ) );
dnld.create();
// buy.add( new Menu( "Premium Support", "/buy/" ) );
// buy.add( new Menu( "Opera for Mobile", "/buy/?show=mobile" ) );
buy.add( new Menu( "Opera Mobile", "/b2b/solutions/mobile/" ) );
buy.add( new Menu( "Opera Mini", "/b2b/solutions/mini/" ) );
buy.add( new Menu( "Opera Devices", "/b2b/solutions/devices/" ) );
buy.add( new Menu( "Opera Widgets", "/b2b/solutions/widgets/" ) );
buy.create();
prds.add( new Menu( "Opera Desktop", "/products/desktop/" ) );
prds.add( new Menu( "Opera Mini", "http://www.operamini.com/" ) );
prds.add( new Menu( "Opera Mobile", "/products/mobile/" ) );
prds.add( new Menu( "Opera Devices", "/products/devices/nintendo/" ) );
prds.add( new Menu( "Opera Link", "/products/link/" ) );
prds.add( new Menu( "Opera Dragonfly", "/products/dragonfly/" ) );
prds.create( );
com.add( new Menu( "About Opera", "/company/about/" ) );
com.add( new Menu( "Business Development", "/company/business/" ) );
com.add( new Menu( "Investors", "/company/investors/" ) );
com.add( new Menu( "Partners", "/company/partners/" ) );
com.add( new Menu( "Press", "/press/" ) );
com.add( new Menu( "Mobile Web report", "/mobile_report/" ) );
com.add( new Menu( "Opera on Tour", "/company/events/" ) );
com.add( new Menu( "Jobs", "/company/jobs/" ) );
com.add( new Menu( "Education", "/education/" ) );
com.add( new Menu( "Contact", "/contact/" ) );
com.create( );
sprt.add( new Menu( "New to Opera", "/support/new2opera/" ) );
sprt.add( new Menu( "Register Opera Mobile", "/support/register/" ) );
sprt.add( new Menu( "Tutorials", "/support/tutorials/" ) );
sprt.add( new Menu( "Knowledge Base", "/support/service/" ) );
sprt.add( new Menu( "Community", "/support/online/" ) );
sprt.add( new Menu( "Reporting bugs", "/support/bugs/" ) );
sprt.add( new Menu( "Premium Support", "https://support.opera.com/bin/customer" ) );
sprt.add( new Menu( "Documentation", "/docs/" ) );
sprt.add( new Menu( "Contact", "/contact/" ) );
sprt.create( );

}
function MenuP ( node ) {
this.node = node;
this.menuItems = new Array();
MenuP.prototype.add = function( menuItem ) {
this.menuItems[ this.menuItems.length ] = menuItem;	
}
MenuP.prototype.create = function( ) {
var dNode = document.createElement( "div" );
dNode.className = "jsMenu";
var str = "_" + this.node.id;
dNode.setAttribute( "id", str );
var w = this.node.offsetWidth;
dNode.style.width = w > 160 ? w+"px" : "160px";
dNode.style.top = ( this.node.offsetTop + this.node.offsetHeight ) + "px";
dNode.style.left = this.node.offsetLeft + "px";
addEvent( this.node, "mouseover", function() { showMenuInTime(str, 150) } );
addEvent( this.node, "mouseout", function() { setInMenu(false) } );
dNode.setAttribute( "pItemID", this.node.id );
addEvent( dNode, "mouseover", function() { setInMenu(true) } );
addEvent( dNode, "mouseout", function() { setInMenu(false) } );
var html = "<ul>";
for( var i=0; i<this.menuItems.length; i++ ) {
	html += this.menuItems[i].getLinkHTML();
}
body.appendChild( dNode );
dNode.innerHTML = html + "</ul>";
}
}
function Menu( value, href ) {
this.value = ( value == null ) ? "" : value;
this.href = ( href == null ) ? "" : href;
if( this.href != "" && typeof useAbsPath != "undefined" )
	if( useAbsPath && this.href.indexOf( "http://" ) == -1 && this.href.indexOf( "https://" ) == -1 )
		this.href = "http://www.opera.com" + this.href;
Menu.prototype.getLinkHTML = function () {
	if( this.value != "" && this.href != "" )
		return "<li><a onclick=\"setInMenu(false); hideMenu(); return true;\" href=\"" + this.href + "\">" + this.value + "</a></li>";
	else if( this.value != "" && this.href == "" )
		return "<li class='heading'>" + this.value + "</li>";
	else
		return "<li class='separator'>&nbsp;</li>";
}
}
function showMenuInTime( node, time ) {
popID = setTimeout( "showMenu('" + node + "')", time );
}
function showMenu( node ) {
clearTimeout( popID );
if( typeof node == "string" )
	node = getNode( node );
if( last != null && last != node )
	hideMenu( last );
else if( last == node ) {
	setInMenu( true );
	return;
}
var pItem = getNode( node.getAttribute( "pItemID" ) );
var menuLeft = 0;
var menuTop = pItem.offsetHeight;
var tmp = pItem;
while(tmp!=null && tmp.tagName!="BODY") {
	if( tmp.tagName == "html:body" ) break;
	
	menuLeft += tmp.offsetLeft;
	//menuTop  += tmp.offsetTop;
	tmp = tmp.offsetParent;
}
node.style.left = menuLeft + "px";
node.style.top = 114 + "px";
var w = pItem.offsetWidth;
node.style.width = w > 160 ? w+"px" : "160px";
node.style.display = "block";	
setInMenu( true );
last = node;
if( flash != null )	flash.style.visibility = "hidden";
tID = setTimeout( "hideMenu( last )", 500 );
}
function hideMenu( node ) {
if( node == null ) {
	setInMenu( false );
	hideMenu( last );
	return;
}
if( typeof node == "string" )
	node = getNode( node );
if( !inMenu ) {
	node.style.display = "none";
	var pItem = getNode( node.getAttribute( "pItemID" ) );
	last = null;
	clearTimeout( tID );
	if( flash != null )	flash.style.visibility = "visible";
} else
	tID = setTimeout( "hideMenu( last )", 500 );	
}
function setInMenu( value ) {
inMenu = value;	
if( !value ) clearTimeout( popID );
}
function addEvent( node, evtType, func ) {
if( node.addEventListener ) {
	node.addEventListener( evtType, func, false );
	return true;
} else if( node.attachEvent )
	return node.attachEvent( "on" + evtType, func );
else
	return false;
}
function getNode( nodeId ) {
if( document.getElementById )
	return document.getElementById( nodeId );
else if( document.all && document.all( nodeId ) )
	return document.all( nodeId );
else if( document.layers && document.layers[ nodeId ] )
	return document.layers[ nodeId ];
else
	return false;
}
