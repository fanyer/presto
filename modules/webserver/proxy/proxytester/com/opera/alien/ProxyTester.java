/**
 * This is a reverse HTTP/CCCP proxy: it connects to any HTTP server in the backend, and to an Alien proxy in the front.
 * The purpose is to simulate $n$ control channels connecting to the Alien proxy
 *
 */

package com.opera.alien;

// 63 channel limit in Selector?
// close
import java.math.*;
import java.security.*;
import java.io.*;
import java.nio.*;
import java.nio.channels.*;
import java.util.*;
import java.net.*;

public class ProxyTester
{
    /* configuration section */
    static int numControlChannels = 10000;
    static final String proxyHost = "www.linproxy.com";
//    static final String proxyHost = "www.operarebels.com";
    static final int proxyPort = 80;
    static final String serverHost = "www.operarebels.com";
    static final int serverPort = 32768;
    static final String alienDomain = "linproxy.com";
//    static final String alienDomain = "operarebels.com";
    static final String alienUser = "chrism";
    /* end configuration section */

    static final int CRYPTO_RADIX = 16;
    static Timer timer;

    public static void
    main(String[] args) throws Exception
    {
	if (args.length > 0) {
	    numControlChannels = Integer.valueOf(args[0]);
	}
	timer = new Timer();
	ControlChannelsThread cc = new ControlChannelsThread();
	RSVPThread rsvp = new RSVPThread();
        rsvp.start();
        cc.start();
	cc.join();
	rsvp.join();
    }
}

class KeepAliveEvent extends TimerTask
{
    static final long period = 300;

    ControlChannel cc;
    
    KeepAliveEvent(ControlChannel cc)
    {
	super();
	this.cc = cc;
    }
    
    public void
    run()
    {
	try {
	    cc.sendKeepAlive();
	} catch (IOException ioe) {
	    ioe.printStackTrace();
	}
    }
}

class ControlChannel
{
    enum ControlChannelState {CONNECTING, CONNECTED, SENT_REGISTER, GOT_CHALLENGE, SENT_RESPONSE, READY, ACK_INVITE};
    ControlChannelState state;
    String msg;
    String uri;
    ByteBuffer buffer;
    SocketChannel chan;
    String nonce;
    int inviteID;
    static int maxID = 0;
    static String CRLF = "\r\n\r\n";
    static String sharedSecret = "1dead1";

    ControlChannel(SocketChannel chan)
    {
	this.chan = chan;
	maxID++;
	uri = String.format("dev%d.%s", maxID, ProxyTester.alienDomain);
	state = ControlChannelState.CONNECTING;
	buffer = ByteBuffer.allocateDirect(1500);
	msg = "";
    }

    void
    sendReg() throws IOException
    {
	String reg = String.format("REGISTER %s CCCP/0.3" + CRLF,
				   uri);
	ByteBuffer buf = ByteBuffer.wrap(reg.getBytes());
	chan.write(buf);
	state = ControlChannelState.SENT_REGISTER;
    }

    // returns true if the entire challenge is read from the channel
    boolean
    readChallenge() throws IOException
    {
	int red = chan.read(buffer);
	if (red < 0) {
	    SelectionKey key = chan.keyFor(ControlChannelsThread.sel);
	    key.cancel();
	}
	if (red > 0) {
            buffer.flip();
	    byte[] bytes = new byte[red];
	    buffer.get(bytes, 0, red);
	    buffer.clear();
	    msg += new String(bytes, 0, red);
	    if (msg.contains(CRLF)) {
		if (!msg.startsWith("CHALLENGE")) {
		    throw new IOException("challenge expected");
		}
		StringTokenizer st = new StringTokenizer(msg);
		st.nextToken(); // skip verb
		nonce = st.nextToken();
		state = ControlChannelState.GOT_CHALLENGE;
		msg = "";
		return true;
	    }
	}
	return false;
    }

    String
    crypto()
    {
	MessageDigest md = null;
	try {
	    md = MessageDigest.getInstance("MD5");
	} catch (NoSuchAlgorithmException nsa) {
	    nsa.printStackTrace();
	}
	md.update(sharedSecret.getBytes());
	md.update(nonce.getBytes());
	byte[] outputBytes = md.digest();
	BigInteger check = new BigInteger(1, outputBytes);
	String checkString = check.toString(ProxyTester.CRYPTO_RADIX);
        if (ProxyTester.CRYPTO_RADIX == 16) {
            checkString = String.format("%032x", check);
        }
	return checkString;
    }
    
    void
    sendResponse() throws IOException
    {
	String resp = String.format("RESPONSE %s CCCP/0.3" + CRLF, crypto());
	ByteBuffer buf = ByteBuffer.wrap(resp.getBytes());
	chan.write(buf);
	state = ControlChannelState.SENT_RESPONSE;
    }

    void
    readOK() throws IOException
    {
	int red = chan.read(buffer);
	if (red < 0) {
	    SelectionKey key = chan.keyFor(ControlChannelsThread.sel);
	    key.cancel();
	}
	if (red > 0) {
            buffer.flip();
	    byte[] bytes = new byte[red];
	    buffer.get(bytes, 0, red);
	    buffer.clear();
	    msg += new String(bytes, 0, red);
	    if (msg.contains(CRLF)) {
		if (!msg.startsWith("CCCP/0.3")) {
		    throw new IOException("Expected CCCP/0.3");
		}
		StringTokenizer st = new StringTokenizer(msg);
		st.nextToken(); // skip protocol
		int code = Integer.valueOf(st.nextToken());
		if (code == 200) {
		    state = ControlChannelState.READY;
		    System.out.println(uri + " registered with proxy");
		    KeepAliveEvent kae = new KeepAliveEvent(this);
		    ProxyTester.timer.schedule(kae, 1000*kae.period, 1000*kae.period);
		} else {
		    throw new IOException(uri + " Expected 200 OK, got " + msg);
		}
		msg = "";
	    }
	}
    }

    boolean
    readInvite() throws IOException
    {
	int red = chan.read(buffer);
	if (red < 0) {
	    SelectionKey key = chan.keyFor(ControlChannelsThread.sel);
	    key.cancel();
	}
	if (red > 0) {
            buffer.flip();
	    byte[] bytes = new byte[red];
	    buffer.get(bytes, 0, red);
	    buffer.clear();
	    msg += new String(bytes, 0, red);
	}
	if (msg.contains(CRLF)) {
	    if (!msg.startsWith("INVITE")) {
		if (msg.startsWith("CCCP/0.3 200 Connection renewed")) {
		    // KEEPALIVE response from java proxy
		} else {
		    throw new IOException(uri + " expected an INVITE, got " + msg);
		}
	    }
	    StringTokenizer st = new StringTokenizer(msg);
	    st.nextToken(); // skip verb
	    inviteID = Integer.valueOf(st.nextToken());
	    nonce = st.nextToken();
	    msg = msg.substring(msg.indexOf(CRLF) + CRLF.length(), msg.length()); 
	    RSVPThread.add(new Session(uri, inviteID, crypto()));
	    state = ControlChannelState.ACK_INVITE;
	    System.out.println(uri + " got INVITE");
	    return true;
	}
	return false;
    }

    synchronized void
    ackInvite() throws IOException
    {
	String ack = String.format("CCCP/0.3 200 %d" + CRLF, inviteID);
	ByteBuffer buf = ByteBuffer.wrap(ack.getBytes());
	chan.write(buf);
	state = ControlChannelState.READY;
    }

    synchronized void
    sendKeepAlive() throws IOException
    {
	String msg = "KEEPALIVE " + uri + " CCCP/0.3" + CRLF;
	ByteBuffer buf = ByteBuffer.wrap(msg.getBytes());
	chan.write(buf);
	System.out.println(uri + " sent KEEPALIVE");
    }
    
}

class ControlChannelsThread extends Thread
{
    static Selector sel;

    ControlChannelsThread() throws IOException
    {
	sel = Selector.open();
	for(int i=0; i < ProxyTester.numControlChannels; i++) {
	    SocketChannel chan = SocketChannel.open();
	    chan.configureBlocking(false);
	    ControlChannel cc = new ControlChannel(chan);
	    chan.register(sel, SelectionKey.OP_CONNECT, cc);
	    InetSocketAddress ip = new InetSocketAddress(ProxyTester.proxyHost, ProxyTester.proxyPort);
	    chan.connect(ip);
	}
    }
    
    public void
    run()
    {
	while (true) {
	    int keysReady = 0;
	    try {
		keysReady = sel.select();
	    } catch (IOException ioe) {
		ioe.printStackTrace();
	    }
	    if (keysReady > 0) {
		Set readyKeys = sel.selectedKeys();
		for (Iterator i = readyKeys.iterator(); i.hasNext();) {
		    SelectionKey key = (SelectionKey)i.next();
		    i.remove();
		    ControlChannel cc = (ControlChannel)key.attachment();
		    try {
			switch (cc.state) {
			case CONNECTING:
			    if (key.isConnectable()) {
				SocketChannel chan = (SocketChannel)key.channel();
				if (chan.finishConnect()) {
                                    cc.state = ControlChannel.ControlChannelState.CONNECTED;
				    key.interestOps(SelectionKey.OP_WRITE);
				} else {
				    throw new IOException("Control channel could not connect to proxy");
				}
			    }
			    break;
                        case CONNECTED: 
                            if (key.isWritable()) {
                                cc.sendReg();
				key.interestOps(SelectionKey.OP_READ);
                            }
                            break;
			case SENT_REGISTER:
			    if (key.isReadable()) {
				if (cc.readChallenge()) {
				    key.interestOps(SelectionKey.OP_WRITE);
				}
			    }
			    break;
			case GOT_CHALLENGE:
			    if (key.isWritable()) {
				cc.sendResponse();
				key.interestOps(SelectionKey.OP_READ);
			    }
			    break;
			case SENT_RESPONSE:
			    if (key.isReadable()) {
				cc.readOK();
			    }
			    break;
			case READY:
			    if (key.isReadable()) {
				if (cc.readInvite()) {
				    key.interestOps(SelectionKey.OP_WRITE);
				}
			    }
			    break;
			case ACK_INVITE:
			    if (key.isWritable()) {
				cc.ackInvite();
				key.interestOps(SelectionKey.OP_READ);
			    }
			    break;
			default:
			}
		    } catch (IOException ioe) {
			ioe.printStackTrace();
		    }
		}
	    }
	}
    }
}
    
class Session
{
    enum SessionState {IDLE, CONNECTING_TO_PROXY, CONNECTED_TO_PROXY, CONNECTING_TO_SERVER, CONNECTED_TO_SERVER, RUNNING}
    SessionState state;
    int inviteID;
    String uri;
    String crypto;
    SocketChannel cli;
    SocketChannel srv;
    ByteBuffer reqBuf;
    ByteBuffer respBuf;
    SelectionKey cliKey;
    SelectionKey srvKey;

    Session(String uri, int inviteID, String crypto) throws IOException
    {
	this.uri = uri;
        this.inviteID = inviteID;
	this.crypto = crypto;
       	cli = SocketChannel.open();
	cli.configureBlocking(false);
	srv = SocketChannel.open();
	srv.configureBlocking(false);
	reqBuf = ByteBuffer.allocateDirect(1500);
	respBuf = ByteBuffer.allocateDirect(1500);
	state = SessionState.IDLE;
    }

    void
    sendRSVP() throws IOException
    {
	String rsvp = String.format("RSVP %s %d %s CCCP/0.3" + ControlChannel.CRLF, uri, inviteID, crypto);
	ByteBuffer buf = ByteBuffer.wrap(rsvp.getBytes());
	cli.write(buf);
        InetSocketAddress serverIP = new InetSocketAddress(ProxyTester.serverHost, ProxyTester.serverPort);
        srv.connect(serverIP);
	state = SessionState.CONNECTING_TO_SERVER;
	System.out.println(uri + " sent RSVP to proxy");
    }

    void
    readFromClient() throws IOException
    {
	int red = cli.read(reqBuf);
	if (red < 0) {
	    cliKey.cancel();
	}
	if (red > 0) {
	    if (srvKey.isValid()) {
		srvKey.interestOps(SelectionKey.OP_WRITE);
	    }
	}
    }

    void
    readFromServer() throws IOException
    {
	int red = srv.read(respBuf);
	if (red < 0) {
	    srvKey.cancel();
	}
	if (red > 0) {
	    if (cliKey.isValid()) {
		cliKey.interestOps(SelectionKey.OP_WRITE);
	    }
	}
    }

    void
    writeToClient() throws IOException
    {
	respBuf.flip();
	cli.write(respBuf);
	respBuf.compact();
	if (respBuf.position() == 0) { // everything written
	    if (cliKey.isValid()) {
		cliKey.interestOps(SelectionKey.OP_READ);
	    }
	}
    }

    void
    writeToServer() throws IOException
    {
	reqBuf.flip();
	srv.write(reqBuf);
	reqBuf.compact();
	if (reqBuf.position() == 0) { // everything written
	    if (srvKey.isValid()) {
		srvKey.interestOps(SelectionKey.OP_READ);
	    }
	}
    }
}
    
class RSVPThread extends Thread
{
    static LinkedList<Session> sessionsToAdd;
    static Selector sel;

    RSVPThread() throws IOException
    {
	sel = Selector.open();
	sessionsToAdd = new LinkedList<Session>();
    }

    public static void
    add(Session sess) throws IOException
    {
	sessionsToAdd.add(sess);
	sel.wakeup();
    }
	    
    public void
    run()
    {
	while (true) {
	    int keysReady = 0;
	    try {
		keysReady = sel.select();
		if (sessionsToAdd.size() > 0) {
		    Session sess = sessionsToAdd.poll();
		    sess.cli.register(sel, SelectionKey.OP_CONNECT, sess);
		    InetSocketAddress proxyIP = new InetSocketAddress(ProxyTester.proxyHost, ProxyTester.proxyPort);
		    sess.cli.connect(proxyIP);
                    sess.state = Session.SessionState.CONNECTING_TO_PROXY;
		}
	    } catch (IOException ioe) {
		ioe.printStackTrace();
	    }
	    if (keysReady > 0) {
		Set readyKeys = sel.selectedKeys();
		for (Iterator i = readyKeys.iterator(); i.hasNext();) {
		    SelectionKey key = (SelectionKey)i.next();
		    i.remove();
		    Session sess = (Session)key.attachment();
		    try {
			switch (sess.state) {
			case CONNECTING_TO_PROXY:
			    if (key.isConnectable()) {
				SocketChannel chan = (SocketChannel)key.channel();
                                if (chan.equals(sess.cli)) {
				    sess.cliKey = key;
                                    if (chan.finishConnect()) {
					System.out.println(sess + " connected to proxy");
                                        sess.state = Session.SessionState.CONNECTED_TO_PROXY;
                                        key.interestOps(SelectionKey.OP_WRITE);
                                    } else {
                                        throw new IOException("RSVP channel could not connect to proxy");
                                    }
                                }
                            }
			    break;
                        case CONNECTED_TO_PROXY:
                            if (key.isWritable()) {
				SocketChannel chan = (SocketChannel)key.channel();
				if (chan.equals(sess.cli)) {
				    sess.srv.register(sel, SelectionKey.OP_CONNECT, sess);
				    sess.sendRSVP();
				    key.interestOps(SelectionKey.OP_READ);
				}
                            }
                            break;
			case CONNECTING_TO_SERVER:
			    if (key.isConnectable()) {
				SocketChannel chan = (SocketChannel)key.channel();
				if (chan.equals(sess.srv)) {
				    sess.srvKey = key;
				    if (chan.finishConnect()) {
					System.out.println(sess + " connected to server");					
					sess.state = Session.SessionState.RUNNING;
					key.interestOps(SelectionKey.OP_READ);
				    } else {
					throw new IOException("RSVP channel could not connect to server");
				    }
				}
			    }
			    break;
			case RUNNING:
			    if (key.isReadable()) {
				if (key.equals(sess.cliKey)) {
				    sess.readFromClient();
				} else {
				    sess.readFromServer();
				}
			    }
			    if (key.isValid() && key.isWritable()) {
				if (key.equals(sess.srvKey)) {
				    sess.writeToServer();
				} else {
				    sess.writeToClient();
				}
			    }
			    break;
			}
		    } catch (IOException ioe) {
			ioe.printStackTrace();
		    }
		}
	    }
	}
    }
}

