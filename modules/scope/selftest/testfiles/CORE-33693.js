if (location.hostname.indexOf('example.com') != -1)
{
  window.opera.addEventListener('BeforeEvent.load', function (e)
  {
    //check that it is the page loading, not just an image
    if (e.event.target instanceof Document)
    {
      e.preventDefault();
    }
  }, false);
}
