/**
 * An adress consists of the following information
 *
 *   Peronal Name (e.g. "Erik Eriksson")
 *   Mailbox name (e.g. "ee")
 *   Host name    (e.g  "opera.com")
 *
 * This should be generic to work across different protocols
 *
 * The personal name usually has no function for others than human
 * readers, but it can be convenient to match two different adresses
 * agains each other (two addresses with the same personal name, could
 * possibly mean two different ways of contacting that person - but
 * there is no guarantee - so usually the user should be asked before
 * making such assumpions), or to search for a person's addresses.
 * Protocol doesn't need to provide information about the "Personal
 * Name"-field.
 *
 * When logging in to a protocol, a password may have to be provided
 * in addition to the Address.
 *
 *
 * Many Addresses can be merged into a Contact
 *
 * Examples:
 *
 * Mail account, ee@opera.com
 *   Erik Eriksson; ee; opera.com
 *
 * ICQ account
 *   Erik Eriksson; 609783; icq.mirabilis.com
 *
 * SMS "account"
 *   Erik Eriksson; 0046705228912; null
 * 
 * Jabber account
 *   Erik Eriksson; luakabop; jabber.com
 *
 * LysKOM account
 *   Erik Eriksson; p7; kom.intern.opera.no
 *
 * LysKOM conference
 *   software development; m42; kom.intern.opera.no
 *
 * Usenet group
 *   opera.misc; opera.misc; news.opera.com
 *
 * Web community
 *   Erik; luakabop; lunarstorm.se
 *
 * UNIX Talk
 *   Erik Eriksson; ee; daytona.intern.opera.com
 *
 * IRC
 *   Erik Eriksson; ..?
 * 
 **/

class Address 
{
  public:

	const char* User();
	
	const char* Host();
	
};

class Profile : public Address
{
	const char* Name();
};

class Contact 
{
	/**
	 * To be defined later
	 *
	 * Typically, holds Address * n
	 * and some more data such as
	 * home address, telephone number etc
	 */

	
	
};

 
