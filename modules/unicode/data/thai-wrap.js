// ==UserScript==
// @name Thai wrap
// @author Art 
// @namespace http://siit.net/members/art/thaiwrap.html 
// @version 5.1
// @description  Allows the text on Thai pages to wrap to the edges of
//			the window instead of producing a horizontal
//			scrollbar.
// @ujs:category browser: enhancements
// @ujs:published 2005-09-14 19:05
// @ujs:modified 2005-09-19 09:19
// @ujs:documentation http://userjs.org/scripts/browser/enhancements/thai-wrap 
// @ujs:download http://userjs.org/scripts/download/browser/enhancements/thai-wrap.js
// ==/UserScript==


/* 
 * This script is granted to the Public Domain.
 */

(function () {

 var protectedAddresses = [

  /************************************************************************************************************/
  //Some invalid pages may not work correctly if they use the default settings, and they need special treatment.
  //If using this script causes a page to fail, add its address in here - the script will still run, but it will
  //be a little slower. You can also use the * wildcard to match multiple pages

  'http://www.example.com/foo.html',
  'http://www.jotdomain.com/*',

  /************************************************************************************************************/

 null];
 protectedAddresses = new RegExp('^('+protectedAddresses.slice(0,-1).join('|').replace(/([\\\/\?\+\(\)\{\}\[\]\.\^\$])/g,'\\$1').replace(/\*/g,'.*')+')$','');

 /* compare function for sorting */
 function cnum(a,b){return a-b;}
 
 /* unambiguous words that are common, like prepositions */
 /* for example, not include "เพื่อ" since it may be part of "เพื่อน", "ว่า"/"ว่าง", "คือ"/"เคือง" */
 /* เป็น|อยู่|จะ|ใช้|ได้|ให้|ใน|จึง|หรือ|และ|กับ|เนื่อง|ด้วย|ถ้า|แล้ว|ทั้ง|เพราะ|ซึ่ง|ซ้ำ|ไม่|ใช่|ต้อง|กัน|จาก|ถึง|นั้น|ผู้|ความ|ส่วน|ยัง|ทั่ว|อื่น|โดย|สามารถ|เท่า|ใต้|ใส่|ใด|ไว้|ใหม่|ใหญ่|เล็ก|ใกล้|ไกล|เขา|ช่วย|ฉบับ|ค้น|เร็ว|เข้า|เช้า */
 /* revised + new words by thep at linux.thai.net */ 
 var cw="(\u0e40\u0e1b\u0e47\u0e19|\u0e2d\u0e22\u0e39\u0e48|\u0e08\u0e30|\u0e43\u0e0a\u0e49|\u0e44\u0e14\u0e49|\u0e43\u0e2b\u0e49|\u0e43\u0e19|\u0e08\u0e36\u0e07|\u0e2b\u0e23\u0e37\u0e2d|\u0e41\u0e25\u0e30|\u0e01\u0e31\u0e1a|\u0e40\u0e19\u0e37\u0e48\u0e2d\u0e07|\u0e14\u0e49\u0e27\u0e22|\u0e16\u0e49\u0e32|\u0e41\u0e25\u0e49\u0e27|\u0e17\u0e31\u0e49\u0e07|\u0e40\u0e1e\u0e23\u0e32\u0e30|\u0e0b\u0e36\u0e48\u0e07|\u0e0b\u0e49\u0e33|\u0e44\u0e21\u0e48|\u0e43\u0e0a\u0e48|\u0e15\u0e49\u0e2d\u0e07|\u0e01\u0e31\u0e19|\u0e08\u0e32\u0e01|\u0e16\u0e36\u0e07|\u0e19\u0e31\u0e49\u0e19|\u0e1c\u0e39\u0e49|\u0e04\u0e27\u0e32\u0e21|\u0e2a\u0e48\u0e27\u0e19|\u0e22\u0e31\u0e07|\u0e17\u0e31\u0e48\u0e27|\u0e2d\u0e37\u0e48\u0e19|\u0e42\u0e14\u0e22|\u0e2a\u0e32\u0e21\u0e32\u0e23\u0e16|\u0e40\u0e17\u0e48\u0e32|\u0e43\u0e15\u0e49|\u0e43\u0e2a\u0e48|\u0e43\u0e14|\u0e44\u0e27\u0e49|\u0e43\u0e2b\u0e21\u0e48|\u0e43\u0e2b\u0e0d\u0e48|\u0e40\u0e25\u0e47\u0e01|\u0e43\u0e01\u0e25\u0e49|\u0e44\u0e01\u0e25|\u0e40\u0e02\u0e32|\u0e0a\u0e48\u0e27\u0e22|\u0e09\u0e1a\u0e31\u0e1a|\u0e04\u0e49\u0e19|\u0e40\u0e23\u0e47\u0e27|\u0e40\u0e02\u0e49\u0e32|\u0e40\u0e0a\u0e49\u0e32)";
 /* leading chars */
 var lc="[\u0e40-\u0e44]|\\(|\\[|\\{|\"";
 /* "following" chars */
 var fc="\u0e2f|[\u0e30-\u0e3A]|[\u0e45-\u0e4e]|\\)|\\]|\\}|\"";
 /* thai chars */
 var tc="\u0e01-\u0e3a\u0e40-\u0e4f\u0e5a\u0e5b";
 
 var r=new Array();
 var l=new Array();
 
 /* following chars followed by leading chars */
 r[0]=new RegExp("("+fc+")(?=("+lc+"))");
 l[0]=1;
 /* l >= 0 means using this value as length */
 
 /* thai followed by non-thai */
 r[1]=new RegExp("(["+tc+"])(?![\\)\\]\\}\"]|["+tc+"])");
 l[1]=1;
 
 /* non-thai followed by thai */
 r[2]=new RegExp("([^"+tc+"\\(\\(\\[\\{\"])(?=["+tc+"])");
 l[2]=1;
 
 /* non-leading char followed by known word */
 r[3]=new RegExp("([^"+lc+"])(?=("+lc+")*"+cw+"("+fc+")?)");
 l[3]=1;
 
 /* known word followed by non-following chars */
 r[4]=new RegExp("(("+lc+")*"+cw+"("+fc+")*)(?!"+fc+")");
 l[4]=-1;
 /* l < 0 means using length from match() function */
 
 /* end-of-word symbols */
 r[5]=new RegExp("([\u0e45\u0e46\u0e33])");
 l[5]=1;
 
 var hasThai = new RegExp('['+tc+']');
 
 /* do breaks */
 function F(n){
  var p,a,c,x,t,e;
  if(n.nodeType==3){
   /* find all possible break points */
   p=new Array();
   if(n.data.match(hasThai)){
    //no need to waste time running matches against empty nodes
    //this saves a significant amount of time
    for(i=0;i<r.length;i++){
     t=n.data.search(r[i]);
     if(t>=0){
      if(l[i]>=0){p.push(t+l[i]);}
      else{e=n.data.match(r[i]);p.push(t+e[1].length);}
     }
    }
   }
   /* use the left-most break point */
   if(p.length>0){
    p.sort(cnum);
    if(p[0]>=0){
     a=n.splitText(p[0]);
     n.parentNode.insertBefore(document.createTextNode("\u200b"),a);
    }
   }
  }else if(n.tagName) {
   n.tagNameLow = n.tagName.toLowerCase();
   if( n.tagNameLow != "style" && n.tagNameLow != "script" ){
    for(c=0;x=n.childNodes[c];++c){F(x);}}
  }
 }
 document.addEventListener('load',function () {
  //if required, perform the wrapping
  if( document.body && document.body.innerText.match(hasThai) ) {
   if( location.href.match(protectedAddresses) ) {
    //page has a bug that prevents cloning
    F(document.body);
   } else {
    //perform the wrap on a clone to prevent excessive reflowing
    var foo = document.body.cloneNode(true);
    for( var i = 0, j, allTextAr = [], theTxAr = document.getElementsByTagName('textarea'); j = theTxAr[i]; i++ ) {
     allTextAr[i] = j.value;
    }
    F(foo);
    document.body.parentNode.replaceChild(foo,document.body)
    for( var i = 0, j; j = theTxAr[i]; i++ ) {
     //recover textarea content (clones do not copy it)
     j.value = allTextAr[i];
    }
   }
  }
 },false);
})();