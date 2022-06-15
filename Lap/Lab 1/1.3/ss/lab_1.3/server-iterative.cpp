/*********************************************************** -- HEAD -{{{1- */
/* Echo Server for Network API Lab: Part I in Internet Technology 2011.
 *
 * Iterative server capable of accpeting and processing a single connection
 * at any given time. Data received from the connection is simply sent back
 * unmodified ("echoed").
 *
 * Build the server using e.g.
 * 		$ g++ -Wall -Wextra -o server-iter server-iterative.cpp
 *
 * Start using
 * 		$ ./server-iter
 * or
 * 		$ ./server-iter 31337
 * to listen on a port other than the default 5703.
 */
/******************************************************************* -}}}1- */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <vector>
#include <algorithm>

// Set VERBOSE to 1 to print additional, non-essential information.
#define VERBOSE 1

// If NONBLOCKING is set to 1, all sockets are put into non-blocking mode.
// Use this for Part II, when implementing the select() based server, where
// no blocking operations other than select() should occur. (If an blocking
// operation is attempted, a EAGAIN or EWOULDBLOCK error is raised, probably
// indicating a bug in the code!)
#define NONBLOCKING 1

// Default port of the server. May be overridden by specifying a different
// port as the first command line argument to the server program.
const int kServerPort = 5703;

// Second parameter to listen().
// Note - this parameter is (according to the POSIX standard) merely a hint.
// The implementation may choose a different value, or ignore it altogether.
const int kServerBacklog = 8;

// Size of the buffer used to transfer data. A single read from the socket
// may return at most this much data, and consequently, a single send may
// send at most this much data.
const size_t kTransferBufferSize = 64;

/* Connection states.
 * A connection may either expect to receive data, or require data to be
 * sent.
 */
enum EConnState
{
	eConnStateReceiving,
	eConnStateSending
};

/* Per-connection data
 * In the iterative server, there is a single instance of this structure,
 * holding data for the currently active connection. A concurrent server will
 * need an instance for each active connection.
 */
struct ConnectionData
{
	EConnState state; // state of the connection; see EConnState enum

	int sock; // file descriptor of the connections socket.

	// items related to buffering.
	size_t bufferOffset, bufferSize;
	char buffer[kTransferBufferSize+1];
};

/* Accepts a new connection on the given socket and adds it to the global
 * connections vector.
 */
static bool accept_connection(int listenfd, ConnectionData& conn);

/* Receive data and place it in the connection's buffer.
 *
 * Requires that ConnectionData::state is eConnStateReceiving; if not, an
 * assertation fault is generated.
 *
 * If _any_ data is received, the connection's state is transitioned to
 * eConnStateSending.
 *
 * Returns `true' if the connection remains open and further processing is
 * required. Returns `false' to indicate that the connection is closing or
 * has closed, and the connection should not be processed further.
 */
static bool process_client_recv( ConnectionData& cd );

/* Send data from the connection's buffer.
 *
 * Requires that ConnectionData::state is eConnStateSending; if not, an
 * asseration fault is generated.
 *
 * When all data is sent, the connection's state is transitioned to
 * eConnStateReceiving. If data remains in the buffer, no state-transition
 * occurs.
 *
 * Returns `true' if the connection remains open and further processing is
 * required. Returns `false' to indicate that the connection is closing or
 * has closed, and the connection should not be processed further.
 */
static bool process_client_send( ConnectionData& cd );

/* Places the socket identified by `fd' in non-blocking mode.
 *
 * Returns `true' if successful, and `false' otherwise.
 */
static bool set_socket_nonblocking( int fd );

/* Returns `true' if the connection `cd' has an invalid socket (-1), and
 * `false' otherwise.
 */
static bool is_invalid_connection( const ConnectionData& cd );


/* Sets up a listening socket on `port'.
 *
 * Returns, if successful, the new socket fd. On error, -1 is returned.
 */
static int setup_server_socket( short port );

int main( int argc, char* argv[] )
{
	int serverPort = kServerPort;

	// did the user specify a port?
	if( 2 == argc )
	{
		serverPort = atoi(argv[1]);
	}

#	if VERBOSE
	printf( "Attempting to bind to port %d\n", serverPort );
#	endif

	// set up listening socket - see setup_server_socket() for details.
	int listenfd = setup_server_socket( serverPort );

	if( -1 == listenfd )
		return 1;

	std::vector<ConnectionData> connections;

	while( 1 )
	{
		// Add sockets to readfds and writefds.
		fd_set readfds, writefds;
		FD_ZERO(&readfds);
		FD_ZERO(&writefds);
		FD_SET(listenfd, &readfds);
		int nfds = listenfd;
		for (size_t i = 0; i < connections.size(); i++) {
			ConnectionData& conn = connections[i];
			if (conn.state == eConnStateReceiving) {
				FD_SET(conn.sock, &readfds);
			} else {
				FD_SET(conn.sock, &writefds);
			}
			nfds = std::max(nfds, conn.sock);
		}

		// Check if any socket is ready for reading or writing.
		int res = select(nfds + 1, &readfds, &writefds, NULL, NULL);
		if (res == -1) {
			perror("select() failed");
			return -1;
		}

		// Accept new connection.
		if (FD_ISSET(listenfd, &readfds)) {
			ConnectionData conn;
			memset(&conn, 0, sizeof(conn));
			bool success = accept_connection(listenfd, conn);
			if (!success) {
				perror("accept_connection() failed");
			} else {
				connections.push_back(conn);
			}
		}

		// Receive from or send data to open connections.
		for (size_t i = 0; i < connections.size(); i++) {
			ConnectionData& conn = connections[i];
			bool still_open = true;
			if (FD_ISSET(conn.sock, &readfds)) {
				still_open = process_client_recv(conn);
			} else if (FD_ISSET(conn.sock, &writefds)) {
				still_open = process_client_send(conn);
			}
			if (!still_open) {
				close(conn.sock);
				conn.sock = -1;
			}
		}

		// Remove all closed connections from the connections vector.
		connections.erase(
			std::remove_if(
				connections.begin(), connections.end(), &is_invalid_connection
			),
			connections.end()
		);
	}

	// The program will never reach this part, but for demonstration purposes,
	// we'll clean up the server resources here and then exit nicely.
	close(listenfd);

	return 0;
}

static bool accept_connection(int listenfd, ConnectionData& conn) {
	sockaddr_in clientAddr;
	socklen_t addrSize = sizeof(clientAddr);

	// accept a single incoming connection
	int clientfd = accept( listenfd, (sockaddr*)&clientAddr, &addrSize );

	if(clientfd == -1) {
		perror( "accept() failed" );
		return false;
	}

#			if VERBOSE
	// print some information about the new client
	char buff[128];
	printf("Connection from %s:%d -> socket %d\n",
		inet_ntop(AF_INET, &clientAddr.sin_addr, buff, sizeof(buff)),
		ntohs(clientAddr.sin_port),
		clientfd
	);
	fflush(stdout);
#			endif

#			if NONBLOCKING
	// enable non-blocking sends and receives on this socket
	if(!set_socket_nonblocking(clientfd)) {
		return false;
	}
#			endif

	// initialize connection data
	conn.sock = clientfd;
	conn.state = eConnStateReceiving;

	return true;
}

static bool process_client_recv( ConnectionData& cd )
{
	assert( cd.state == eConnStateReceiving );

	// receive from socket
	ssize_t ret = recv( cd.sock, cd.buffer, kTransferBufferSize, 0 );

	if( 0 == ret )
	{
#		if VERBOSE
		printf( "  socket %d - orderly shutdown\n", cd.sock );
		fflush( stdout );
#		endif

		return false;
	}

	if( -1 == ret )
	{
#		if VERBOSE
		printf( "  socket %d - error on receive: '%s'\n", cd.sock,
			strerror(errno) );
		fflush( stdout );
#		endif

		return false;
	}

	// update connection buffer
	cd.bufferSize += ret;

	// zero-terminate received data
	cd.buffer[cd.bufferSize] = '\0';

	// transition to sending state
	cd.bufferOffset = 0;
	cd.state = eConnStateSending;
	return true;
}

static bool process_client_send( ConnectionData& cd )
{
	assert( cd.state == eConnStateSending );

	// send as much data as possible from buffer
	ssize_t ret = send( cd.sock,
		cd.buffer+cd.bufferOffset,
		cd.bufferSize-cd.bufferOffset,
		SO_NOSIGPIPE // MSG_NOSIGNAL // suppress SIGPIPE signals, generate EPIPE instead
	);

	if( -1 == ret )
	{
#		if VERBOSE
		printf( "  socket %d - error on send: '%s'\n", cd.sock,
			strerror(errno) );
		fflush( stdout );
#		endif

		return false;
	}

	// update buffer data
	cd.bufferOffset += ret;

	// did we finish sending all data
	if( cd.bufferOffset == cd.bufferSize )
	{
		// if so, transition to receiving state again
		cd.bufferSize = 0;
		cd.bufferOffset = 0;
		cd.state = eConnStateReceiving;
	}

	return true;
}

static int setup_server_socket( short port )
{
	// create new socket file descriptor
	int fd = socket( AF_INET, SOCK_STREAM, 0 );
	if( -1 == fd )
	{
		perror( "socket() failed" );
		return -1;
	}

	// bind socket to local address
	sockaddr_in servAddr;
	memset( &servAddr, 0, sizeof(servAddr) );

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(port);

	if( -1 == bind( fd, (const sockaddr*)&servAddr, sizeof(servAddr) ) )
	{
		perror( "bind() failed" );
		close( fd );
		return -1;
	}

	// get local address (i.e. the address we ended up being bound to)
	sockaddr_in actualAddr;
	socklen_t actualAddrLen = sizeof(actualAddr);
	memset( &actualAddr, 0, sizeof(actualAddr) );

	if( -1 == getsockname( fd, (sockaddr*)&actualAddr, &actualAddrLen ) )
	{
		perror( "getsockname() failed" );
		close( fd );
		return -1;
	}

	char actualBuff[128];
	printf( "Socket is bound to %s %d\n",
		inet_ntop( AF_INET, &actualAddr.sin_addr, actualBuff, sizeof(actualBuff) ),
		ntohs(actualAddr.sin_port)
	);

	// and start listening for incoming connections
	if( -1 == listen( fd, kServerBacklog ) )
	{
		perror( "listen() failed" );
		close( fd );
		return -1;
	}

	// allow immediate reuse of the address (ip+port)
	int one = 1;
	if( -1 == setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int) ) )
	{
		perror( "setsockopt() failed" );
		close( fd );
		return -1;
	}

#	if NONBLOCKING
	// enable non-blocking mode
	if( !set_socket_nonblocking( fd ) )
	{
		close( fd );
		return -1;
	}
#	endif

	return fd;
}

static bool set_socket_nonblocking( int fd )
{
	int oldFlags = fcntl( fd, F_GETFL, 0 );
	if( -1 == oldFlags )
	{
		perror( "fcntl(F_GETFL) failed" );
		return false;
	}

	if( -1 == fcntl( fd, F_SETFL, oldFlags | O_NONBLOCK ) )
	{
		perror( "fcntl(F_SETFL) failed" );
		return false;
	}

	return true;
}

static bool is_invalid_connection( const ConnectionData& cd )
{
	return cd.sock == -1;
}
