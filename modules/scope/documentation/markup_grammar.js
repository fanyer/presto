/**
 * Add markup.  This is made complicated by keys that are
 * prefixes, suffixes, and substrings of other keys, and the fact that
 * we must add anchors or links before we add the other.
 */
function markup(text)
{
   var kmap = {};
   var keys = [];
   text = text.replace(/</g, "&lt;");
   text = text.replace(/("[^"]*")/g, '<span class="string">$1</span>');
   text = text.replace(/(#.*)/g, '<span class="comment">$1</span>');
   text = text.replace(/TODO/g, '<span class="not-started">TODO</span>');
   text = text.replace(/[A-Z][-A-Z0-9]{2,}(?=\s+::=)/g,
                       function (key) {
                          if (!kmap.hasOwnProperty(key)) {
                             kmap[key] = true;
                             keys.push(key);
                          }
                          return "<a name=\"@" + key + "@\"></a>@" + key + "@"; } );
   keys.sort(function (a,b) { return b.length - a.length; } );
   for ( var i=0 ; i < keys.length ; i++ )
      text = text.replace(new RegExp("([^@][A-Z0-9-]*)(" + keys[i] + ")(?!(?:\"|@|-|[A-Z0-9-]))", "g"), "$1<a href=\"#@$2@\">@$2@</a>");
   text = text.replace(/@/g, "");
   text = text.replace(/(http:\/\/[^\r\n\<\&]*)/g, '<a href="$1">$1</a>');
   return text;
}

/**
 * Scroll to the link identified by the hash in the URL, e.g. with the URL
 *    http://example.com#this-one
 * this function will scroll to the element <a name='this-one'>
 * Needed since markup is added after the document has loaded.
 */
function scrollToHash()
{
   var identifier = location.hash.replace(/^#/, ''); if(identifier) {
      var as = document.getElementsByTagName('a'), a = null, i = 0;
      for( ; a = as[i]; i++)
      {
         if( a.name == identifier)
         {
            a.scrollIntoView();
            return;
         }
      }
   }
}

function markup_grammar()
{
   var grammar_elm = document.getElementById("grammar");
   if (grammar_elm)
   {
      grammar_elm.innerHTML = markup(grammar_elm.innerText);
      scrollToHash();
   }
}

