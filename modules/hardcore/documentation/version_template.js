// <define current_version>
// <define next_version>
var versions = [current_version, next_version];

function getVersionXmlLink(file_base, directory)
{
    var links = new Array();
    for (var v in versions) {
        var filename = file_base+"."+versions[v]+".xml";
        links.push("<a href=\""+filename+"\">"+directory+filename+"</a>");
    }
    return links;
}

function writeVersionLinkList(file_base, directory)
{
    var links = getVersionXmlLink(file_base, directory);
    document.write("<ul>");
    for (var l in links)
        document.write("<li>"+ links[l] + "</li>");
    document.write("</ul>");
}