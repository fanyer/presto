var strServer;
if (window.location.toString().search(/webdevp/) >= 0)
	strServer = "webdevp3.mingpao.com/Webnews";
else
	strServer = "webnews1.mingpao.com";

document.writeln('<table width=100% border=0 cellspacing=0 cellpadding=0 bgcolor="#009999">');
document.writeln('<tr>');
document.writeln('<td>'); 
document.writeln('<table width=100% border=0 cellspacing=1 cellpadding=5>');
document.writeln('<tr bgcolor=#EBFCF9>');
document.writeln('<td bgcolor="#F1F4F4" align="center"><img src=http://www.mingpaonews.com/image/news_gspot.gif width=6 height=6></td>');
document.writeln('<td bgcolor="#F1F4F4"><font size=2><a href="http://' + strServer + '/cfm/Corr_Index.cfm">更正欄</a></font></td>');
document.writeln('</tr>');
document.writeln('<tr bgcolor=#EBFCF9>');
document.writeln('<td bgcolor=#DCEBEB align="center"><img src=http://www.mingpaonews.com/image/news_wspot.gif width=6 height=6></td>');
document.writeln('<td nowrap bgcolor=#DCEBEB><font size=2><a href="http://' + strServer + '/cfm/CEditor_Index.cfm">總編輯<br>張健波信箱</a></font></td>');
document.writeln('</tr>');
document.writeln('<tr bgcolor=#EBFCF9>');
document.writeln('<td bgcolor="#F1F4F4" align="center"><img src=http://www.mingpaonews.com/image/news_gspot.gif width=6 height=6></td>');
document.writeln('<td bgcolor="#F1F4F4"><font size=2><a href="http://' + strServer + '/cfm/Contact_Index.cfm">明報聯絡資料</a></font></td>');
document.writeln('</tr>');
document.writeln('<tr bgcolor=#EBFCF9>');
document.writeln('<td bgcolor="#DCEBEB" align="center"><img src=http://www.mingpaonews.com/image/news_wspot.gif width=6 height=6></td>');
document.writeln('<td bgcolor="#DCEBEB"><font size=2><a href=http://www.mingpaonews.com/noversea/index.htm target=_blank>海外聯網</a></font></td>');
document.writeln('</tr>');
document.writeln('<tr bgcolor=#B2EBE6>');
document.writeln('<td bgcolor="#F1F4F4" align="center"><img src=http://www.mingpaonews.com/image/news_gspot.gif width=6 height=6></td>');
document.writeln('<td bgcolor="#F1F4F4"><font size=2><a href=http://www.mingpaonews.com/sales_kit/index.htm target=_blank>廣告服務</a></font></td>');
document.writeln('</tr>');
document.writeln('<tr bgcolor=#EBFCF9>');
document.writeln('<td bgcolor="#DCEBEB" align="center"><img src=http://www.mingpaonews.com/image/news_wspot.gif width=6 height=6></td>');
document.writeln('<td bgcolor="#DCEBEB"><font size=2><a href=http://www.mingpaonews.com/adbanner/overseas/index.htm target=_blank>訂閱費用表</a></font></td>');
document.writeln('</tr>');
document.writeln('<tr bgcolor=#B2EBE6>');
document.writeln('<td bgcolor="#F1F4F4" align="center"><img src=http://www.mingpaonews.com/image/news_gspot.gif width=6 height=6></td>');
document.writeln('<td bgcolor="#F1F4F4"><font size=2><a href=http://www.mingpaonews.com/sales_kit/award.htm target=_blank>奪獎特輯</a></font></td>');
document.writeln('</tr>');
document.writeln('<tr bgcolor=#EBFCF9>');
document.writeln('<td bgcolor="#DCEBEB" align="center"><img src=http://www.mingpaonews.com/image/news_wspot.gif width=6 height=6></td>');
document.writeln('<td bgcolor="#DCEBEB"><font size=2><a href=mailto:inews@mingpao.com?Subject=讀者報料 target=_blank>讀者報料</a></font></td>');
document.writeln('</tr>');
document.writeln('</table>');
document.writeln('</td>');
document.writeln('</tr>');
document.writeln('</table>');

function flayer() {
	var ypos = 0;
	var ns = (navigator.appName.indexOf("Netscape") != -1);
	function clayer(id) {
		var objex=document.getElementById?document.getElementById(id):document.all?document.all[id]:document.layers[id];
		if(document.layers)objex.style=objex;
		objex.sP=function(x,y){this.style.left=x;this.style.top=y;};
		objex.x = 765;
		objex.y = ypos;
		return objex;
	}
	window.xl=function() {
		var slayer = ns ? pageYOffset : document.body.scrollTop;
		tlayer.y += (slayer + ypos - tlayer.y)/8;
		tlayer.sP(tlayer.x, tlayer.y);
		setTimeout("xl()", 8);
	}
	tlayer = clayer("rbox");
	xl();
}