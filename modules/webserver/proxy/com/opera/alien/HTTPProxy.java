package com.opera.alien;

import java.net.*;
import java.io.*;
import java.security.*;
import java.math.BigInteger;
import java.nio.*;
import java.nio.channels.*;
import java.util.*;
import java.util.regex.*;
import java.util.logging.*;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.text.SimpleDateFormat;

/**
 * Acts as a proxy between an HTTP client and an Opera embedded server.
 * Opera embedded webservers will register with this proxy on startup
 * and maintain an open TCP connection (control channel).
 * The control channel is used to send an INVITE when a request comes
 * in for a resource on the corresponding embedded webserver.
 * The webserver responds by opening a new connection to this proxy,
 * which then forwards data between the client and server.
 *
 */
// TODO: get shared secret from MyOpera
// TODO: split HTTPProxy into ClientThread and AlienThread and ServerThread?
// TODO: don't need 3 threads per connection?
// TODO: don't need 2 thread per connection?
// TODO: denial of service attack via extremely long URI or header line?
// TODO: DOS via opening lots of connections and abandoning them?
public class HTTPProxy extends Thread
{
    /*************************************/
    /***** configure these ***************/
    /*************************************/

    // periodically write statistics to this file
    static final String statsFileName = "stats";
    // requests with this Host: value will not be proxied
    static final String myHostName = "alien.opera.com"; // name of this host
    // timeout for directConnect() (deprecated)
    static final int connectTimeout = 2; // seconds
    // how long to wait for a webserver to connect back to us
    static final int inviteTimeout = 10;
    static final String logFileName = "HTTPProxy.log";
    static final String testURI = "/keepalive";
    // bug: should be x kB, see HTTP spec for value of x
    static final int maxLinesInHeader = 1000;
    static final boolean useLoadBalancer = false;
    static final String loadBalancerHost = "localhost";
    static final int loadBalancerPort = 82;
    // port to listen on, overridden by argv[]
    static int port = 8080;
    // name for this proxy, used by load balancer
    static String name = "alien1"; 

    /*************************************/    
    /***** end configuration section *****/
    /*************************************/
    
    enum Role {HTTP, CCCP, CCCP_RSVP, TBA};
    enum Action {LIVENESS, KEEPALIVE, REGISTER, WELCOME, PROXY, RESPONSE_BUSY,
		 RESPONSE_OK, VERIFY_CRYPTO, SHUTDOWN, DROP, LIST, TEST, INVALID};
    static final double proxyVersion = 0.3;
    static final String CRLF = "\r\n";
    static final String RFC1123Format = "EEE, dd MMM yyyy HH:mm:ss zzz";
    static final TimeZone GMT = TimeZone.getTimeZone("GMT");
    static final String HTTP_11 = "HTTP/1.1";
    // Chris's Connection Control Protocol, of course
    static final String CCCP_ = "CCCP/";
    static final String CCCP_03 = "CCCP/0.3"; // like 0.2, but with keepalive instead of reregistration
    static final String CCCP_02 = "CCCP/0.2";     
    static final String CCCP_01 = "CCCP/0.1"; 
    static final Pattern HTTPVersion = Pattern.compile("HTTP/(\\d)\\.(\\d)");
    static final Pattern authResult = Pattern.compile("[\\s\\S]*login success=\"(\\d)\"[\\s\\S]*");
    static final String html = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n" 
    + "\"http://www.w3.org/TR/html4/loose.dtd\">\n"
    +	"<html>\n"
    +	"<head>\n"
    +	"<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=utf-8\">\n"
    +	"<title>Currently running webservers</title>\n"
    +	"</head>\n"
    +	"<body>\n"
    +   "<h1>Currently running webservers</h1>\n";
    static final int CRYPTO_BITS = 128;
    static final int CRYPTO_RADIX = 16;

    static ServerSocketChannel ssc;
    // at most one for each Alien    
    static HashMap<URI,Alien> controlConnections;
    // at most one for each client TCP connection
    static HashMap<Integer,HTTPProxy> clients;
    static Semaphore controlSemaphore;
    // for timing out Alien registration
    static Timer timer;
    static Logger logger;
    static int activeClients = 0;
    static long totalClients = 0;
    // activeServers determined from controlConnections hashtable
    static long totalServers = 0;

    Alien controlConnection; 
    SimpleDateFormat dateFormatter;     
    SocketChannel clientChannel; // used for talking to peer: this should just be called "channel"
    SocketChannel serverChannel; // channel to webserver, used by client connections only
    Semaphore inviteSemaphore;
    Requester requester;
    Responder responder;
    Exception stuntException;
    String host; // peer FQDN
    String peerURI; // derived from host
    URI alienURI;
    Action action;
    Role role;
    String uri;
    String protocolVersion; // which version of the protocol are we speaking?
    SocketChannel serverControlChannel; // associates clients with servers, used to prevent bogus CCCP 503
    BigInteger registerNonce; // only if I'm a control connection

    static {
        boolean assertsEnabled = false;
        assert assertsEnabled = true; // Intentional side effect!!!
        if (!assertsEnabled)
            throw new RuntimeException("Asserts must be enabled!!!");
    }

    class Requester extends Thread
    {
	static final int bufSize = 1500;
	ByteBuffer buf;
	HTTPProxy owner;
	String prefix;
	
	Requester(HTTPProxy owner)
	{
	    buf = ByteBuffer.allocate(bufSize);
	    this.owner = owner;
	}

	void
	push(ByteBuffer b)
	{
	    if (prefix == null) {
		prefix = new String();
	    }
	    int numBytes = b.limit();
	    byte bytes[] = new byte[numBytes];
	    b.get(bytes, 0, numBytes);
	    String s = new String(bytes);
	    prefix = prefix + s;
	}
	
	void
	fillBuffer() throws IOException
	{
	    buf.clear();
	    int red = -1;
	    boolean threw = false;
	    try {
		red = clientChannel.read(buf);
		assert(true);
	    } catch (IOException ioe) {
		// probably Connection reset by peer
		threw = true;
	    }
	    if (red < 0) {
		if (!threw) {
		    assert(true);
		}
		throw new IOException();
	    }
	    if (red == 0) {
		logger.finest("fillBuffer read zero bytes");
	    }
	}

	public void
	run()
	{
	    //requester
	    while (true) {
		ByteBuffer buffer = null;
		if (prefix != null) {
		    byte[] bytes = prefix.getBytes();
		    buffer = ByteBuffer.wrap(bytes);
		    prefix = null;
		}
		if (buffer == null) {
		    try {
			fillBuffer();
		    } catch (IOException e) {
			owner.interrupt();
			break;
		    }
		    if (buf.position() == 0) {
			logger.finest("local read got zero bytes");
		    }
		    buf.flip();		    
		    buffer = buf;
		}
		try {
		    serverChannel.write(buffer);
		} catch (ClosedChannelException cce) {
		    owner.interrupt();
		    break;
		} catch (IOException e) {
		    e.printStackTrace();
		    owner.interrupt();
		    break;
		}
	    }
	}
    }
    
    class Responder extends Thread
    {
	ByteBuffer buf;
	static final int bufSize = 1500;
	boolean gotResponse;
	HTTPProxy owner;
	boolean sleeping;
	
	Responder(HTTPProxy owner)
	{
	    buf = ByteBuffer.allocate(bufSize);
	    gotResponse = false;
	    this.owner = owner;
	    sleeping = false;
	}

	void
	printBuffer() throws IOException
	{
	    clientChannel.write(buf);
	}

	public void
	run()
	{
	    //responder
	    while (true) {
		int red;
		buf.clear();
		try {
		    red = serverChannel.read(buf);
		    if (red == 0) {
			//logger.finest("read zero bytes, continuing");
			continue;
		    } else if (red < 0) {
			owner.interrupt();
			break;
		    } else {
			assert (red == buf.position());
			buf.flip();
			try {
			    printBuffer();
			} catch (IOException ioe) {
			    owner.interrupt();
			    break;
			}
		    }
		} catch (ClosedChannelException cce) {
		    owner.interrupt();
		    break;
		} catch (IOException e) {
		    //e.printStackTrace();
		    owner.interrupt();
		    break;
		}
	    }
	}
    }
    
    public
    HTTPProxy(SocketChannel clientChannel)
    {
	inviteSemaphore = new Semaphore(0);
	this.clientChannel = clientChannel;
	requester = new Requester(this);
	responder = new Responder(this);
	dateFormatter = new SimpleDateFormat(RFC1123Format, Locale.US);
	dateFormatter.setTimeZone(GMT);
    }

    boolean
    verify(String crypto, String sharedSecret, BigInteger nonce)
    {
	MessageDigest md = null;
	try {
	    md = MessageDigest.getInstance("MD5");
	} catch (NoSuchAlgorithmException nsa) {
	    assert(false);
	}
	assert(md != null);
	assert(nonce != null);
	md.update(sharedSecret.getBytes());
	String nonceString = nonce.toString(CRYPTO_RADIX);
	md.update(nonceString.getBytes());
	byte[] outputBytes = md.digest();
	BigInteger check = new BigInteger(1, outputBytes);
	String checkString = check.toString(CRYPTO_RADIX);
        if (CRYPTO_RADIX == 16) {
            checkString = String.format("%032x", check);
        }
	return checkString.equalsIgnoreCase(crypto);
    }
    
    StringTokenizer
    parseControlMessage(String line) throws ServerException, BadCCCPResponse, BadCCCPRequest
    {
	assert(line != null);
	assert(!line.equals(""));
	action = Action.INVALID;
	StringTokenizer st = new StringTokenizer(line);
	String s = st.nextToken();
	assert (protocolVersion != null);
	if (s.startsWith(CCCP_)) {
	    if (!s.equals(protocolVersion)) {
		throw new BadCCCPResponse("wrong protocol version");
	    }
	    if (!st.hasMoreTokens()) throw new BadCCCPResponse("not enough parameters");
	    String code = st.nextToken();
	    if (code.equals("200")) {
		action = Action.RESPONSE_OK;
		return st;
	    } else if (code.equals("503")) {
		action = Action.RESPONSE_BUSY;
		return st;
	    } else {
		throw new BadCCCPResponse(String.format("unexpected response code %s", code));
	    }
	} else {
	    String method = s;
	    if ((!protocolVersion.equals(CCCP_01) && !protocolVersion.equals(CCCP_02)) && method.equalsIgnoreCase("KEEPALIVE")) {
		action = Action.KEEPALIVE;
		return st;
	    } else if ((protocolVersion.equals(CCCP_01) || protocolVersion.equals(CCCP_02)) && method.equalsIgnoreCase("REGISTER")) {
		action = Action.REGISTER;
		return st;
	    } else if (method.equalsIgnoreCase("RESPONSE")) {
		action = Action.VERIFY_CRYPTO;
		return st;
	    } else {
		throw new BadCCCPRequest(String.format("400 Unexpected method: %s", method));
	    }
	}
    }

    private void
    parseBusyResponse(StringTokenizer st) throws BadCCCPResponse, ServerException
    {
	if (!st.hasMoreTokens()) throw new BadCCCPResponse("not enough parameters");
	String clientIDString = st.nextToken();
	try {
	    Integer clientID = Integer.valueOf(clientIDString);
	    controlSemaphore.acquire();
	    boolean exists = clients.containsKey(clientID);
	    HTTPProxy client = clients.get(clientID);
	    controlSemaphore.release();
	    if (!exists) {
		logger.fine(String.format("got 503 for nonexistent client %d, perhaps the INVITE expired", clientID));
	    } else {
		assert(client != null);
		assert(client.serverControlChannel != null);
		assert(clientChannel != null);
		// check that the client really wants to talk to this server
		// otherwise a bad guy could send a 503 for someone else's client
		if (clientChannel != client.serverControlChannel) {
		    throw new BadCCCPResponse("bogus 503 attempt thwarted!");
		}
		assert(client.serverChannel == null);
		client.inviteSemaphore.release();
	    }
	    logger.fine("control channel got 503");
	} catch (InterruptedException ie) {
	    throw new ServerException("parseBusyResponse() interrupted");
	} catch (NumberFormatException nfe) {
	    throw new BadCCCPResponse(String.format("bad client id '%s'", clientIDString));
	}
	return;
    }

    private URI
    makeURI(String uri) throws BadCCCPRequest
    {
	URI result = null;
    	StringTokenizer ut = new StringTokenizer(uri, "@");
	if (ut.countTokens() != 2) {
	    throw new BadCCCPRequest("400 Bad URI");
	}
	try {
	    result = new URI(uri);
	} catch (URISyntaxException use) {
	    throw new BadCCCPRequest("400 Bad URI");
	}
	if (controlConnection != null) {
	    if (!controlConnection.uri.equals(result)) {
		throw new BadCCCPRequest(String.format("400 URI mismatch: expected %s, got %s",
						       controlConnection.uri,
						       result));
	    }
	}
	return result;
    }

    // st contains all params after the uri
    private void
    parseRegisterRequest(String uri, StringTokenizer st) throws BadCCCPRequest, ServerException
    {
	Integer alienPort = null;
	action = Action.REGISTER;
	alienURI = makeURI(uri);
	if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 Not enough parameters in REGISTER request");	
	String auth = st.nextToken();
	if (controlConnection != null) {
	    if (!controlConnection.auth.equals(auth)) {
		throw new BadCCCPRequest(String.format("400 Auth string differs from the one last sent"));
	    }
	}
	if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 Not enough parameters in REGISTER request");
	String par3 = st.nextToken();
	if (par3.equals(CCCP_03)) {
	    protocolVersion = CCCP_03;
	} else if (par3.equals(CCCP_02)) {
	    protocolVersion = CCCP_02;
	} else if (par3.equals(CCCP_01)) {
	    protocolVersion = CCCP_01;
	} else {
	    try {
		alienPort = Integer.valueOf(par3);
	    } catch (NumberFormatException nfe) {
		throw new BadCCCPRequest("400 Bad parameter in REGISTER request: " + par3);
	    }
	    if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 missing protocol version in REGISTER request");
	    String par4 = st.nextToken();
	    // CCCP/0.2 does not take a "port" argument
	    if (!par4.equals(CCCP_01)) {
		throw new BadCCCPRequest("400 Bad protocol version: " + par4);
	    } else {
		protocolVersion = par4;
	    }
	}
	assert(protocolVersion != null);
	if (controlConnection != null) {
	    if (!controlConnection.version.equals(protocolVersion)) {
		throw new BadCCCPRequest(String.format("400 Protocol version mismatch: expected %s, got %s",
						       controlConnection.version,
						       protocolVersion));
	    }
	    assert(controlConnection.channel.equals(clientChannel));
	}	    
	if (protocolVersion.equals(CCCP_02) || protocolVersion.equals(CCCP_03)) {
	    controlConnection = new Alien(protocolVersion, clientChannel, auth, alienURI);		
	    assert(alienPort == null);
	} else {
	    controlConnection = new Alien(protocolVersion, clientChannel, auth, alienURI, alienPort);		
	}
    }

    private void
    parseRSVPRequest(String uri, StringTokenizer st) throws BadCCCPRequest, CCCPUnauthorized, ServerException
    {
	action = Action.WELCOME;
	alienURI = makeURI(uri);
	String id = null;
	try {
	    controlSemaphore.acquire();
	    boolean exists = controlConnections.containsKey(alienURI);	
	    Alien alien = controlConnections.get(alienURI);
	    controlSemaphore.release();
	    if (!exists) {
		throw new BadCCCPRequest(String.format("400 URI not found: %s", uri));
	    }
	    if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 Not enough parameters in RSVP request");
	    id = st.nextToken();
	    Integer clientID = Integer.valueOf(id);
	    controlSemaphore.acquire();
	    boolean ok = clients.containsKey(clientID);
	    if (!ok) {
		controlSemaphore.release();
		throw new BadCCCPRequest(String.format("404 %d not found in clients table, perhaps the INVITE timed out", clientID));
	    }
	    HTTPProxy client = clients.get(clientID);
	    controlSemaphore.release();
	    assert (client != null);
	    if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 Missing protocol version in RSVP request");
	    String par2 = st.nextToken();
	    if (!par2.equals(CCCP_01)) {
		if (par2.equals(CCCP_02) || par2.equals(CCCP_03)) throw new BadCCCPRequest("400 Not enough parameters in RSVP request");
		String crypto = par2;
		assert(alien.nonces.containsKey(clientID));
		BigInteger connectNonce = alien.nonces.get(clientID);
		assert(connectNonce != null);
		if (!verify(crypto, alien.auth, connectNonce)) {
		    throw new CCCPUnauthorized();
		}
		if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 Missing protocol version in RSVP request");
		String par3 = st.nextToken();
		if (par3.equals(CCCP_02)) {
		    protocolVersion = CCCP_02;
		} else if (par3.equals(CCCP_03)) {
		    protocolVersion = CCCP_03;
		} else {
		    throw new BadCCCPRequest("400 Bad protocol version in RSVP request");
		}
	    } else {
		protocolVersion = CCCP_01;
	    }
	    client.serverChannel = clientChannel;
	    client.inviteSemaphore.release();
	} catch (InterruptedException ie) {
	    throw new ServerException("ParseRSVPRequest() interrupted");
	} catch (NumberFormatException nfe) {
	    assert(id != null);
	    throw new BadCCCPRequest(String.format("400 Bad client id %s", id));
	}
	return;
    }
    
    private void
    parseRequestLine(String requestLine) throws CCCPUnauthorized,
						BadCCCPRequest,
						BadHTTPRequest,
						BadHTTPVersion,
						ServerException,
						NoRequest
    {
	assert(requestLine != null);
	assert(!requestLine.equals(""));
	action = Action.INVALID;
	StringTokenizer st = new StringTokenizer(requestLine);
	if (st.countTokens() < 3) {
	    throw new NoRequest(requestLine);
	}
	String method = st.nextToken();
	String uri = st.nextToken();
	method = method.trim();
	if (method.equalsIgnoreCase("DROP")) {
	    role = Role.CCCP;
	    if (st.countTokens() == 1) {
		if (uri.equals("please")) {
		    String protocol = st.nextToken();
		    if (protocol.startsWith(CCCP_)) {
			action = action.DROP;
			return;
		    }
		}
	    }
	    throw new BadCCCPRequest("400 Badly formed DROP request");
	} else if (method.equalsIgnoreCase("SHUTDOWN")) {
	    role = Role.CCCP;
	    if (st.countTokens() == 1) {
		if (uri.equals("please")) {
		    String protocol = st.nextToken();
		    if (protocol.startsWith(CCCP_)) {
			action = action.SHUTDOWN;
			return;
		    }
		}
	    }
	    throw new BadCCCPRequest("400 Badly formed SHUTDOWN request");
	} else if (method.equalsIgnoreCase("REGISTER")) {
	    role = Role.CCCP;
	    parseRegisterRequest(uri, st);
	    return;
	} else if (method.equalsIgnoreCase("RSVP")) {
	    role = Role.CCCP_RSVP;
	    parseRSVPRequest(uri, st);
	    return;
	} else {
	    role = Role.HTTP;
	    action = Action.PROXY;
	}
	assert(role.equals(Role.HTTP));
	String httpVersion = st.nextToken();
	Matcher m = HTTPVersion.matcher(httpVersion);
	if (!m.matches()) {
	    throw new BadHTTPRequest("400 Bad HTTP version syntax");
	}
	assert(m.groupCount() == 2);
	String major = m.group(1);
	String minor = m.group(2);
	if (Integer.valueOf(major) < 1) {
	    throw new BadHTTPVersion();
	}
// 	if (Integer.valueOf(minor) < 1) {
// 	    throw new BadHTTPVersion();
// 	}
	this.uri = uri;
    }    

    private void
    parseHeader(String line) throws IOException,
				      BadHTTPRequest
    {
	//logger.finest(line);
	StringTokenizer st = new StringTokenizer(line, ":");
	if (st.countTokens() > 0) {
	    String fieldName = st.nextToken();
	    if (fieldName.equalsIgnoreCase("Host")) {
		if (!st.hasMoreTokens()) {
		    throw new BadHTTPRequest("400 Bad 'Host' header");
		}
		host = st.nextToken();
	    }
	}
    }

    //FIXME: this could read less than one line
    private BufferedReader
    readBuffer(ByteBuffer buf) throws IOException
    {
	buf.clear();
	int red = clientChannel.read(buf);
	if (red < 0) {
	    throw new IOException();
	}
	if (red == 0) {
	    logger.finest("readBuffer read zero bytes");
	    throw new IOException();		
	}
	buf.flip();
	byte[] bytes = new byte[red];
	buf.get(bytes, 0, red);
	String s = new String(bytes);
	StringReader sr = new StringReader(s);	    
	BufferedReader br = new BufferedReader(sr);
	buf.flip();
	if (requester != null) {
	    requester.push(buf);
	}
	return br;
    }
    
    void
    init() throws IOException,
		  NoRequest,
		  CCCPUnauthorized,
		  BadCCCPRequest,
		  BadHTTPRequest,
		  BadHTTPVersion,
		  BadHost,
		  ServerException
    {
	role = Role.TBA;
	int headerLines = 0;
	ByteBuffer buf = ByteBuffer.allocate(Requester.bufSize);
	BufferedReader br = null;
	// note that, until we have a request line, we don't know which protocol to speak
	do {
	    try {
		br = readBuffer(buf);
	    } catch (IOException ioe) {
		throw new NoRequest("eof");
	    }
	    String requestLine = br.readLine();
	    if (requestLine == null) {
		throw new NoRequest("unterminated request line");
	    }
	    if (requestLine.equals("")) {
		// HTTP 1.1 spec allows empty lines before request line
		logger.fine("empty line before request line");
		continue;
	    } else {
		parseRequestLine(requestLine);
		break;
	    }
	} while (true);
	assert(!role.equals(Role.TBA));
	if (action != Action.PROXY) {
	    return;
	}
	assert(role.equals(Role.HTTP));
	do {
	    if (endOfStream(br)) {
		try {
		    br = readBuffer(buf);
		} catch (IOException ioe) {
		    throw new BadHTTPRequest("400 Premature end of headers");
		}
	    }
	    String headerLine = br.readLine();
	    headerLines++;
	    if (headerLines > maxLinesInHeader) {
		throw new BadHTTPRequest("400 Too many header lines in request");
	    }
	    if (headerLine == null) {
		throw new BadHTTPRequest("400 Unterminated header line");
	    }
	    if (headerLine.equals("")) {
		throw new BadHTTPRequest("400 No Host Header");
	    }
	    parseHeader(headerLine);
	    if (host != null) {
		break;
	    }
	} while (true);
	assert(host != null);
	host = host.trim();
	// FIXME: insecure!
        if (host.equalsIgnoreCase(myHostName)) {
	    if (uri.equals("/list")) {
		action = Action.LIST;
	    } else if (uri.startsWith(testURI)) {
		action = Action.TEST;
	    } else /* liveness monitoring */ {
		action = Action.LIVENESS;
	    }
        } else {
	    StringTokenizer st = new StringTokenizer(host, ".");
	    if (st.countTokens() != 5) {
		throw new BadHost(String.format("%s doesn't look like an Alien host", host));
	    }
	    peerURI = host.replaceFirst("\\.", "@");
	}
    }

    void
    testConnection() 
    {
	String foo = "?foo";
	int index = uri.indexOf(foo);
	Integer conn = null;
	if (index > 0) {
	    try {
		String arg = uri.substring(index + foo.length());
		conn = Integer.valueOf(arg);
	    } catch (NumberFormatException nfe) {
	    } catch (IndexOutOfBoundsException ioobe) {
	    }
	}
	sendHTTPKeepAliveResponse("200 OK");
	while (true) {
	    try {
		sendData(clientChannel, String.format("Hello %s<br>\n",
						      (conn != null) ? "this is connection " + conn : ""));
		sleep(1000); // ms
	    } catch (InterruptedException ie) {
		assert(false);
	    } catch (IOException ioe) {
		break;
	    }
	}
    }
	
    void
    listWebservers()
    {
	logger.info("listing webservers");
	Vector<Alien> aliens = aliensVector();
	String body = new String();
	body += html;
	if (aliens.size() > 0) {
	    body += "<ul>\n";
	}
	// FIXME: should pick up protocol version from request?
	for (Alien alien : aliens) {
	    assert(alien.uri != null);
	    String uri = alien.uri.toString();
	    String url = "http://" + uri.replaceFirst("@", ".");
	    if (useLoadBalancer) {
		if (loadBalancerPort != 80) {
		    url = url + ":" + loadBalancerPort;
		}
	    }
	    body += String.format("<li><a href=\"%s\">%s</a></li>\n", url, url);
	}
	if (aliens.size() > 0) {
	    body += "</ul>\n";
	}
	body += "</body>\n";
	body += "</html>\n";
	sendHTTPResponse("200 OK", body);
    }
    
    private boolean
    endOfStream(BufferedReader br)
    {
	boolean result = false;
	try {
	    assert(br.ready());
	    br.mark(2);
	    int i = br.read();
	    br.reset();
	    result = (i < 0);
	} catch (IOException ioe) {
	    ioe.printStackTrace();
	    result = true;
	}
	return result;
    }

    void
    unregisterAlien(URI uri)
    {
	try {
	    logger.info(String.format("unregistering %s", uri.toString()));
	    controlSemaphore.acquire();
	    boolean exists = controlConnections.containsKey(uri);
	    Alien alien = controlConnections.get(uri);
	    if (exists) {
		controlConnections.remove(uri);
	    }
	    controlSemaphore.release();
	    if (!exists) {
		logger.warning(String.format("%s not registered", uri.toString()));
	    } else {
		SocketChannel csc = alien.channel;
		if ((csc != null) && csc.isOpen()) {
		    closeChannel(csc);
		}
	    }
	} catch (InterruptedException ie) {
	    logger.severe("unregisterAlien() interrupted");
	}
    }

    // firstTime is false if this is a re-registration of an existing connection
    void
    registerAlien(boolean firstTime) 
    {
	assert(alienURI != null);
	try {
	    controlSemaphore.acquire();
	    boolean exists = controlConnections.containsKey(alienURI);
	    Alien alien = controlConnections.get(alienURI);	    
	    controlSemaphore.release();	    
	    if (!exists) {
		logger.info(String.format("registering %s", alienURI.toString()));
	    } else if (firstTime) {
		sendCCCPResponse("403 Already Registered");
	    } else {
		assert(alien != null);
		assert(protocolVersion.equals(CCCP_01) ||
		       protocolVersion.equals(CCCP_02) ||
		       protocolVersion.equals(CCCP_03));
		logger.fine(String.format("registering %s", alienURI.toString()));    
		if ((alien.channel != null) && (!alien.channel.equals(clientChannel))) {
		    logger.warning(String.format("%s already has control channel %s",
						 alienURI.toString(),
						 alien.channel.toString()));
		}
	    }
	    assert(clientChannel != null);
	    assert(protocolVersion != null);
	    if (!protocolVersion.equals(CCCP_01)) {
		registerNonce = new BigInteger(CRYPTO_BITS, new Random());
		String msg = "CHALLENGE " + registerNonce.toString(CRYPTO_RADIX);
		sendCCCPRequest(clientChannel, msg, new Semaphore(1), protocolVersion);
	    } else {
		assert(controlConnection != null);
		controlSemaphore.acquire();
		controlConnections.put(alienURI, controlConnection);
		totalServers++;
		controlSemaphore.release();
		sendCCCPResponse("201 Holepunching Failed");
	    }
	    if (firstTime) {
		control();
	    } else {
		return;
	    }
	} catch (InterruptedException ie) {
	    ie.printStackTrace();
	    sendCCCPResponse("500 Internal Server Error");
	}
	closeClient();
    }

    void
    control() 
    {
	assert (clientChannel != null);
	assert (clientChannel.isOpen());
	boolean gotMessage = false;
	ByteBuffer buf = ByteBuffer.allocate(Requester.bufSize);
	try {
	    BufferedReader br = readBuffer(buf);
	    do {
		if (endOfStream(br)) {
		    br = readBuffer(buf);
		}
		String line = br.readLine();
		if (line == null) {
		    sendCCCPResponse("400 Unterminated request line");
		    continue;
		}
		if (line.equals("")) {
		    if (!gotMessage) {
			sendCCCPResponse("400 Empty request line");
		    }
		    gotMessage = false;
		    continue;
		} else {
		    try {
			gotMessage = true;
			StringTokenizer st = parseControlMessage(line);
			switch (action) {
			case KEEPALIVE:
			    assert(alienURI != null);
			    assert(controlConnections.containsKey(alienURI));
			    assert(controlConnection != null);
			    assert(controlConnection.channel != null);
			    assert(clientChannel != null);
			    assert(controlConnection.channel.equals(clientChannel));
			    controlConnection = new Alien(protocolVersion, clientChannel, controlConnection.auth, alienURI);
			    try {
				controlSemaphore.acquire();
			    } catch (InterruptedException ie) {
				throw new ServerException("control() interrupted");
			    }
			    controlConnections.put(alienURI, controlConnection);
			    controlSemaphore.release();
			    sendCCCPResponse("200 Connection renewed");
			    break;
			case REGISTER:
			    // assert is OK here because we checked this in parseControlMessage()
			    assert(protocolVersion.equals(CCCP_01) || protocolVersion.equals(CCCP_02));
			    if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 Not enough parameters");
			    String uri = st.nextToken();
			    parseRegisterRequest(uri, st);
			    registerAlien(false);
			    break;
			case VERIFY_CRYPTO:
			    assert(protocolVersion.equals(CCCP_02) || protocolVersion.equals(CCCP_03));
			    if (!st.hasMoreTokens()) throw new BadCCCPRequest("400 Not enough parameters");
			    String crypto = st.nextToken();
			    assert(alienURI != null);
			    assert(registerNonce != null);
			    assert(controlConnection != null);
			    try {
				if (verify(crypto, controlConnection.auth, registerNonce)) {
				    controlSemaphore.acquire();
				    controlConnections.put(alienURI, controlConnection);
				    totalServers++;
				    controlSemaphore.release();
				    sendCCCPResponse("200 OK");
				} else {
				    controlSemaphore.acquire();
				    boolean exists = controlConnections.containsKey(alienURI);
				    Alien alien = controlConnections.get(alienURI);
				    controlSemaphore.release();
				    sendCCCPResponse("401 Unauthorized");
				    if (exists) {
					assert(alien != null);
					assert(alien.channel != null);
					if (alien.channel.equals(clientChannel)) {
					    // don't close the connection if the webserver is currently registered
					    // if he keeps failing to reregister, eventually the timeout thread will disconnect him
					    break;
					}
				    }
				    return;
				}
			    } catch (InterruptedException ie) {
				throw new ServerException("VERIFY_CRYPTO interrupted");
			    }
			    break;
			case RESPONSE_OK:
			    break;
			case RESPONSE_BUSY:
			    parseBusyResponse(st);
			    break;
			default:
			    throw new AssertionError(action);
			}
		    } catch (ServerException se) {
			sendCCCPResponse("500 Internal Server Error");
			continue;
		    } catch (BadCCCPRequest bar) {
			String msg = bar.getMessage();
			assert (msg != null);
			// 			if (msg == null) {
			// 			    msg = String.format("400 Bad request: %s", line);
			// 			}
			sendCCCPResponse(msg);
			continue;
		    } catch (BadCCCPResponse bad) {
			String msg = bad.getMessage();
			assert (msg != null);
			logger.info(msg);
			continue;
		    }
		}
	    } while (true);
	} catch (IOException ioe) {
	    String msg = ioe.getMessage();
	    logger.finest("control caught IOException" + ((msg != null)?": " + msg:""));
	    if (controlConnections.containsKey(alienURI)) {
		unregisterAlien(alienURI);
	    }
	}
    }

    static void
    sendHeader(SocketChannel channel, String msg)
    {
	msg = msg + CRLF;
	try {
	    sendData(channel, msg);
	} catch (IOException ioe) {
	    assert(true);
	}
    }
    
    static void
    sendData(SocketChannel channel, String msg) throws IOException
    {
	byte bytes[] = msg.getBytes();
	ByteBuffer buf = ByteBuffer.wrap(bytes);
	try {
	    channel.write(buf);
	} catch (ClosedChannelException cce) {
	    assert(true);
 	}
    }
	
    void
    invite(URI uri, Alien alien) throws ServerException, InviteException
    {
	assert(alien != null);
	SocketChannel control = alien.channel;
	Semaphore channelLock = alien.semaphore;
	String alienVersion = alien.version;
	assert(clientChannel != null);
	assert(alienVersion != null);
	Integer key = clientChannel.hashCode();
	try {
	    controlSemaphore.acquire();
	    Collection<Integer> keys = clients.keySet();
	    TreeSet<Integer> used = new TreeSet<Integer>(keys);
            // avoid collisions
            int n = Integer.MAX_VALUE; // because we can't express MAX_VALUE + 1 !
	    if (used.size() == n) {
		controlSemaphore.release();
		throw new ServerException("clients table ran out of key space");
	    }
	    for (int i=1; true; i++) {
		if (used.isEmpty()) {
		    break;
		}
		if (!used.contains(key)) {
		    break;
		}
		assert (i < n);
		int last = used.last();
                int next = last % n;
		// if last == Integer.MAX_VALUE then we skip 0, but we want to skip 0 anyway		
                next += 1; 
		assert(next <= n);
		key = next;
	    }
	    assert(key > 0);
	    assert(!clients.containsKey(key));
	    serverControlChannel = control;
	    clients.put(key, this);
	    controlSemaphore.release();
	    String msg = String.format("INVITE %d", key);
	    if (alienVersion.equals(CCCP_02) || alienVersion.equals(CCCP_03)) {
		assert(!alien.nonces.containsKey(key));
		BigInteger inviteNonce = new BigInteger(CRYPTO_BITS, new Random());
		alien.semaphore.acquire();
		alien.nonces.put(key, inviteNonce);
		alien.semaphore.release();
		msg = msg + " " + inviteNonce.toString(CRYPTO_RADIX);
	    }
	    sendCCCPRequest(control, msg, channelLock, alienVersion);
	    assert (serverChannel == null);
	    if (!inviteSemaphore.tryAcquire(inviteTimeout, TimeUnit.SECONDS)) {
		logger.info(String.format("INVITE %d timed out after %d seconds", key, inviteTimeout));
	    }
	    alien.semaphore.acquire();
	    alien.nonces.remove(key);
	    alien.semaphore.release();
            controlSemaphore.acquire();
            clients.remove(key);
            controlSemaphore.release();
	    if (serverChannel == null) {
		throw new InviteException();
	    }
	} catch (InterruptedException ie) {
	    throw new ServerException("invite() interrupted");
        }
    }

    boolean
    directConnect(SocketChannel controlChannel, int port) throws InterruptedException 
    {
	boolean result = true;
	assert(serverChannel == null);
	Socket s = controlChannel.socket();
	InetAddress addr = s.getInetAddress();
	InetSocketAddress isa = new InetSocketAddress(addr, port);
	try {
	    serverChannel = SocketChannel.open();
	    serverChannel.configureBlocking(false);
	    if (!serverChannel.connect(isa)) {
		int msWaited = 0;
		final int period = 20; // ms
		while (true) {
		    sleep(period);
		    msWaited += period;
		    if (serverChannel.finishConnect()) {
			break;
		    }
		    if (msWaited >= (connectTimeout * 1000)) {
			logger.info("connection to " + isa + " timed out");			
			break;
		    }
		}
		    
	    }
	} catch (IOException ioe) {
	    logger.info(String.format("could not connect to %s: %s", isa, ioe.getMessage()));
	} finally {
	    if (!serverChannel.isConnected()) {
		serverChannel = null;
		result = false;
	    }
	    return result;
	}
    }
    
    void
    proxy()
    {
	try {
	    URI uri = new URI(peerURI);
	    if (!controlConnections.containsKey(uri)) {
		assert (peerURI.equals(host.replaceFirst("\\.", "@")));
		logger.info(String.format("no entry found for uri %s", peerURI));
		sendHTTPError("404 Unknown Host " + host);
		return;
	    }
	    Alien alien = controlConnections.get(uri);
	    assert (alien.channel != null);
//		if (alien.port != null) {
//		    if (!directConnect(alien.channel, alien.port)) {
//			alien.port = null;
//		    }
//		}
	    if (serverChannel == null) {
		invite(uri, alien);
	    }
	    try {
		controlSemaphore.acquire();
	    } catch (InterruptedException ie) {
		throw new ServerException("proxy() interrupted");
	    }
	    activeClients++;
	    totalClients++;
	    controlSemaphore.release();
	} catch (InviteException ie) {
	    sendHTTPError("503 Server Busy");
	    return;
	} catch (ServerException srv) {
	    sendHTTPError("500 Internal Server Error");
	    return;
	} catch (URISyntaxException use) {
	    sendHTTPError("400 Bad Host: header");
	    return;
	}
	assert(serverChannel != null);
	requester.start();
	responder.start();
	try {
	    requester.join();
	    responder.join();
	} catch (InterruptedException ie) {
	    if (requester.isAlive()) {
		requester.interrupt();
	    }
	    if (responder.isAlive()) {
		responder.interrupt();
	    }
	}
	try {
	    controlSemaphore.acquire();
	} catch (InterruptedException ie) {
	    logger.severe("proxy() interrupted");
	}
	activeClients--;
	controlSemaphore.release();
	closeServer();
        closeClient();
    }

    Vector<Alien>
    aliensVector()
    {
	Collection<Alien> values = controlConnections.values();
	Vector<Alien> aliens = new Vector<Alien>();
	for (Alien alien : values) {
	    aliens.add(alien);
	}
	return aliens;
    }

    void
    unregisterAliens()
    {
	Vector<Alien> aliens = aliensVector();
	for (Alien alien : aliens) {
	    unregisterAlien(alien.uri);
	    logger.info(String.format("forcibly expired %s", alien.uri.toString()));
	}
    }

    void
    sendHTTPError(String msg)
    {
	String html = "<html>" + msg + "</html>";
	sendHTTPResponse(msg, html);
    }

    public void
    run()
    {
	try {
	    init();
	} catch (ServerException se) {
	    switch (role) {
	    case HTTP:
		sendHTTPError("500 Internal Server Error");
		closeClient();
		break;
	    case CCCP:
		sendCCCPResponse("500 Internal Server Error");
		closeClient();		
	    default:
		closeClient();		
	    }
	    return;
	} catch (NoRequest nr) {
	    String msg = nr.getMessage();
	    assert (msg != null);
	    logger.fine(String.format("incomprehensible request (%s)", msg));
	    closeClient();
	    return;
	} catch (BadCCCPRequest bcr) {
	    String msg = bcr.getMessage();
	    assert (msg != null);	    
	    switch (role) {
	    case CCCP:
		assert(!action.equals(Action.WELCOME));
		sendCCCPResponse(msg);
		break;
	    case CCCP_RSVP:
		assert(action.equals(Action.WELCOME));
		if (msg.startsWith("404")) {
		    logger.fine(msg);
		} else {
		    logger.info(msg);
		}
	    default:
		assert(false);
	    }
	    closeClient();
	    return;
	} catch (CCCPUnauthorized no) {
	    switch (role) {
	    case CCCP:
		assert(!action.equals(Action.WELCOME));
		sendCCCPResponse("401 Unauthorized");
		break;
	    case CCCP_RSVP:
		assert(action.equals(Action.WELCOME));
		break;
	    default:
		assert(false);
	    }
	    closeClient();
	    return;
	} catch (BadHTTPRequest br) {
	    assert(role.equals(Role.HTTP));
	    String msg = br.getMessage();
	    assert (msg != null);
	    sendHTTPError(msg);
	    return;
	} catch (BadHTTPVersion bhv) {
	    assert(role.equals(Role.HTTP));
	    sendHTTPError("505 HTTP Version Not Supported");
	    return;
	} catch (BadHost bh) {
	    assert(role.equals(Role.HTTP));
	    sendHTTPError("404 Not Found");
	    return;
	} catch (IOException ioe) {
	    // should be redundant
	    closeClient();
	    return;
	}
	switch (action) {
	case LIVENESS:
	    sendHTTPResponse("200 OK");
	    return;
	case TEST:
	    testConnection();
	    return;
	case LIST:
	    listWebservers();
	    return;
	case DROP:
	    logger.info("dropping connections");
	    unregisterAliens();
	    closeClient();
	    return;
	case SHUTDOWN:
	    //FIXME: ensure no new registrations can happen
	    //FIXME: add proper authentication
	    logger.info("shutting down");
	    unregisterAliens();
	    System.exit(0);
	    return;
	case REGISTER:
	    requester = null;
	    responder = null;
	    registerAlien(true);
	    return;
	case WELCOME:
	    return;
	case PROXY:
	    proxy();
	    return;
	default:
	    assert(false);
	}
    }

    static void
    closeChannel(SocketChannel channel)
    {
	Socket s = channel.socket();
	try {
	    s.shutdownOutput();
	    s.close();
	    channel.close();
	} catch (ClosedChannelException cce) {
	} catch (SocketException se) {
	} catch (IOException e) {
	    e.printStackTrace();
	} finally {
            channel = null;
        }
    }
    
    void
    closeClient()
    {
	closeChannel(clientChannel);
    }
    
    void
    closeServer()
    {
	closeChannel(serverChannel);
    }

    static void
    sendCCCPRequest(SocketChannel channel, String s, Semaphore lock, String version)
    {
	assert(version != null);
	assert(version.equals(CCCP_01) || version.equals(CCCP_02) || version.equals(CCCP_03));
	String msg = String.format("%s %s", s, version);
	try {
	    lock.acquire();
	} catch (InterruptedException ie) {
	    logger.severe("sendCCCPRequest() interrupted");
	}
	sendHeader(channel, msg);
	sendHeader(channel, "");
	lock.release();
    }
    
    void
    sendCCCPResponse(String status)
    {
	String protocol = protocolVersion;
	if (protocol == null) protocol = CCCP_03;
	sendStatus(protocol, status);
	sendHeader(clientChannel, "");
    }

    void
    sendHTTPResponse(String status)
    {
	sendHTTPResponse(status, null);
    }    

    void
    sendHTTPKeepAliveResponse(String status)
    {
	sendHTTPResponse(status, null, true);
    }

    void
    sendHTTPResponse(String status, String body)
    {
	sendHTTPResponse(status, body, false);
    }
			      
    void
    sendHTTPResponse(String status, String body, boolean keepAlive)
    {
	Date date = new Date();
	String dateHeader = String.format("Date: %s",
					  dateFormatter.format(date));
	sendStatus(HTTP_11, status);
	sendHeader(clientChannel, dateHeader);
	sendHeader(clientChannel, "Server: com.opera.alien.HTTPProxy/" + proxyVersion);
	if (!keepAlive) {
	    sendHeader(clientChannel, "Connection: close");
	}
	sendHeader(clientChannel, "Content-Type: text/html");
	if (body != null) {
	    assert(!keepAlive);
	    String contentLength = String.format("Content-Length: %d", body.length());
	    sendHeader(clientChannel, contentLength);
	}
	sendHeader(clientChannel, "");
	if (body != null) {
	    try {
		sendData(clientChannel, body);
	    } catch (IOException ioe) {
		assert(true);
	    }
	}
	if (!keepAlive) {
	    closeClient();
	}
    }
    
    void
    sendStatus(String protocol, String status) 
    {
	String msg = String.format("%s %s", protocol, status);
	if (status.startsWith("500")) {
	    logger.severe(msg);
	} else if (status.startsWith("2")) {
	    //	    logger.fine(msg);
	} else if (protocol.startsWith(CCCP_)) /* webserver screwed up */ {
	    logger.info(msg);
	} else /* client screwed up */ {
	    logger.fine(msg);
	}
	sendHeader(clientChannel, msg);
    }

    static void
    initLogging() throws IOException
    {
	logger = Logger.getLogger("com.opera.alien.HTTPProxy");
	java.util.logging.Handler[] handlers = logger.getParent().getHandlers();
	for (java.util.logging.Handler handler : handlers) {
	    if (handler instanceof ConsoleHandler) {
		handler.setLevel(Level.WARNING);
	    }
	}
	FileHandler fh = new FileHandler(logFileName);
	SimpleFormatter sf = new SimpleFormatter();
	fh.setFormatter(sf);
	fh.setLevel(Level.FINE);
	logger.addHandler(fh);
	logger.setLevel(Level.FINEST);
    }
    
    public static void
    main(String[] args) throws Exception
    {
	if (args.length > 0) {
	    try {
		port = Integer.valueOf(args[0]);
		if (args.length > 1) {
		    name = args[1];
		}
	    } catch (NumberFormatException nfe) {
		System.err.println(String.format("bad port '%s', defaulting to %d", args[0], port));
	    }
	}
	initLogging();
	controlConnections = new HashMap<URI,Alien>();
	clients = new HashMap<Integer,HTTPProxy>();
	//	endPoints = new LinkedList<STUNTEndpoint>();
	controlSemaphore = new Semaphore(1);
	timer = new Timer();
	//Control.logToFile(stuntLogFileName);
	ssc = ServerSocketChannel.open();
	ServerSocket ss = ssc.socket();
	InetSocketAddress isa = new InetSocketAddress(port);
	ss.bind(isa);
	logger.info(String.format("Listening to local port %d", port));
	if (useLoadBalancer) {
	    LoadBalancerRegistrar lbReg = new LoadBalancerRegistrar();
	    timer.schedule(lbReg, 0, 1000*lbReg.period);
	}
	StatsWriter statsWriter = new StatsWriter();
	timer.schedule(statsWriter, 0, 1000*statsWriter.period);
	while (true) {
	    try {
		SocketChannel clientChannel = ssc.accept();
		Socket s = clientChannel.socket();
		InetAddress ia = s.getInetAddress();
		logger.finest(String.format("accepted connection from %s", ia.getHostAddress()));
		HTTPProxy proxy = null;
		proxy = new HTTPProxy(clientChannel);
		proxy.start();
	    } catch (IOException ioe) {
		logger.log(Level.SEVERE, "accept() caught exception", ioe);
		break;
	    }
	}
    }

    class NoRequest extends Exception
    {
	public
	NoRequest(String s)
	{
	    super(s);
	}
    }
    
    class BadHTTPRequest extends Exception
    {
	public
	BadHTTPRequest()
	{
	    super();
	}
	
	public
	BadHTTPRequest(String s)
	{
	    super(s);
	}
    }

    class CCCPUnauthorized extends Exception
    {
	public
	CCCPUnauthorized()
	{
	    super();
	}
    }
    
    class BadCCCPResponse extends Exception
    {
	public
	BadCCCPResponse(String s)
	{
	    super(s);
	}
    }
    
    class BadCCCPRequest extends Exception
    {
	public
	BadCCCPRequest(String s)
	{
	    super(s);
	}
    }

    class BadHTTPVersion extends Exception
    {
	public
	BadHTTPVersion()
	{
	    super();
	}
    
	public
	BadHTTPVersion(String s)
	{
	    super(s);
	}
    }

    class BadHost extends Exception
    {
	public
	BadHost()
	{
	    super();
	}
    
	public
	BadHost(String s)
	{
	    super(s);
	}
    }

    class ServerException extends Exception
    {
	public
	ServerException(String reason)
	{
	    super();
	    logger.severe(reason);
	}
    }
    
    class InviteException extends Exception
    {
	public
	InviteException()
	{
	    super();
	}
    }

    static class StatsWriter extends TimerTask
    {
	static final long period = 300;

	public void
	run()
	{
	    int activeServers = controlConnections.size();
	    try {
		FileWriter fw = new FileWriter(statsFileName, false);
		String s = String.format("%d,%d,%d,%d\n",
					 activeClients,
					 totalClients,
					 activeServers,
					 totalServers);
		fw.write(s, 0, s.length());
		fw.close();
	    } catch (IOException ioe) {
		logger.severe("couldn't write stats file: " + ioe.getMessage());
	    }
	}
    }
    
    static class LoadBalancerRegistrar extends TimerTask
    {
	static final long period = 60;

	public void
	run()
	{
	    logger.fine(String.format("registering with load balancer %s:%d", loadBalancerHost, loadBalancerPort));
	    InetSocketAddress isa = new InetSocketAddress(loadBalancerHost, loadBalancerPort);
	    String s = String.format("LBREGISTER %s?port=%d HTTP/1.1" + CRLF + CRLF, name, port);
	    ByteBuffer buf = ByteBuffer.wrap(s.getBytes());
	    try {
		SocketChannel channel = SocketChannel.open(isa);
		channel.write(buf);
		closeChannel(channel);
	    } catch (IOException ioe) {
		String reason = ioe.getMessage();
		String why = (reason != null) ? ": " + reason : "";
		logger.info("failed to register with load balancer" + why);
	    }
	}
    }


    class Alien
    {
	static final long period  = 660; // registration expiry, seconds
	//static final long period  = 30; // registration expiry, seconds
	SocketChannel channel;
	String auth; // in CCCP/0.2, this is the shared secret
	Date timestamp;
	URI uri;
	Integer port;
	Semaphore semaphore;
	String version;
	// maps INVITEs to nonces
	HashMap<Integer,BigInteger> nonces;

	public
	Alien(String version, SocketChannel channel, String auth, URI uri, int port)
	{
	    this(version, channel, auth, uri);
	    this.port = port;
	}
	
	public
	Alien(String version, SocketChannel channel, String auth, URI uri)
	{
	    nonces = new HashMap<Integer,BigInteger>();
	    semaphore = new Semaphore(1);
	    this.version = version;
	    this.channel = channel;
	    this.auth = auth;
	    timestamp = new Date(System.currentTimeMillis());
	    this.uri = uri;
	    port = null;
	    AlienTimerTask task = new AlienTimerTask(this);
	    HTTPProxy.timer.schedule(task, 1000*period);
	}
    }

    class AlienTimerTask extends TimerTask
    {
	Alien owner;
	    
	public
	AlienTimerTask(Alien owner)
	{
	    super();
	    this.owner = owner;
	}
	    
	public void
	run()
	{
	    Alien alien = controlConnections.get(owner.uri);
	    if (alien != null) {
		if (alien.equals(owner)) {
		    logger.info(String.format("%s expired", owner.uri.toString()));
		    unregisterAlien(owner.uri);
		}
	    }
	}
    }
}

