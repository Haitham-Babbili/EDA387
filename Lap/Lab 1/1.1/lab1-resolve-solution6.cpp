/********************************************************* -- SOURCE -{{{1- */
/** Translate host name into IPv4 and IPv6 address(es)
 *
 * Resolve IPv4 and IPv6 addresses for a given host name. The host name is 
 * specified as the first command line argument to the program. 
 *
 * Build program:
 * 	$ g++ -Wall -g -o resolve <file>.cpp
 */
/******************************************************************* -}}}1- */

#include <stdio.h>
#include <stddef.h>

#include <assert.h>
#include <limits.h>
#include <unistd.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <sys/types.h>
#include <sys/socket.h>

//--//////////////////////////////////////////////////////////////////////////
//--    local declarations          ///{{{1///////////////////////////////////

void print_usage( const char* aProgramName );

//--    local config                ///{{{1///////////////////////////////////

/* HOST_NAME_MAX may be missing, e.g. if you're running this on an MacOS X
 * machine. In that case, use MAXHOSTNAMELEN from <sys/param.h>. Otherwise 
 * generate an compiler error.
 */
#if !defined(HOST_NAME_MAX)
#	if defined(__APPLE__)
#		include <sys/param.h>
#		define HOST_NAME_MAX MAXHOSTNAMELEN
#	else  // !__APPLE__
#		error "HOST_NAME_MAX undefined!"
#	endif // ~ __APPLE__
#endif // ~ HOST_NAME_MAX


//--    main()                      ///{{{1///////////////////////////////////
int main( int aArgc, char* aArgv[] )
{
	// Check if the user supplied a command line argument.
	if( aArgc != 2 )
	{
		print_usage( aArgv[0] );
		return 1;
	}

	// The (only) argument is the remote host that we should resolve.
	const char* remoteHostName = aArgv[1];

	// Get the local host's name (i.e. the machine that the program is 
	// currently running on).
	const size_t kHostNameMaxLength = HOST_NAME_MAX+1;
	char localHostName[kHostNameMaxLength];

	if( -1 == gethostname( localHostName, kHostNameMaxLength ) )
	{
		perror( "gethostname(): " );
		return 1;
	}

	// Print the initial message
	printf( "Resolving `%s' from `%s':\n", remoteHostName, localHostName );

	// Set up the `hints' sent to getaddrinfo().
	addrinfo addressHints = {0}, *addresses;
	addressHints.ai_family = AF_UNSPEC; // AF_UNSPEC => any address family
	addressHints.ai_socktype = SOCK_STREAM; // see below
	addressHints.ai_protocol = IPPROTO_TCP; // see below

	/* We don't really care about the socket type (ai_socktype) or the 
	 * protocol (ai_protocol) at this point. However if we don't specify them
	 * here, getaddrinfo() will return a result for every valid combination
	 * of socket type and protocol. (See IPv4 only version for example.)
	 */

	int ret = getaddrinfo( remoteHostName, 0, &addressHints, &addresses );
	if( 0 != ret )
	{
		fprintf( stderr, "Error in getaddrinfo(): %s\n", gai_strerror(ret) );
		return 1;
	}
	
	// getaddrinfo() may return several results. print all of them.
	for( addrinfo const *ptr = addresses; ptr; ptr = ptr->ai_next )
	{
		sockaddr const *sockAddr = ptr->ai_addr;

		// handle different address families
		if( AF_INET == sockAddr->sa_family )
		{
			// IPv4 addresses
			const size_t kNameBufferSize = 32;
			char nameBuffer[kNameBufferSize];

			sockaddr_in const *inAddr = (sockaddr_in const *)sockAddr;
			printf( "IPv4: %s\n", inet_ntop( AF_INET, &inAddr->sin_addr, nameBuffer, kNameBufferSize ) );
		}
		else if( AF_INET6 == sockAddr->sa_family )
		{
			// IPv6 addresses
			const size_t kNameBufferSize = 64;
			char nameBuffer[kNameBufferSize];

			sockaddr_in6 const *inAddr = (sockaddr_in6 const *)sockAddr;
			printf( "IPv6: %s\n", inet_ntop( AF_INET6, &inAddr->sin6_addr, nameBuffer, kNameBufferSize ) );
		}
	}
	
	// free resources allocated by getaddrinfo()!
	freeaddrinfo( addresses );

	// Ok, we're done. Return success.
	return 0;
}


//--    print_usage()               ///{{{1///////////////////////////////////
void print_usage( const char* aProgramName )
{
	fprintf( stderr, "Usage: %s <hostname>\n", aProgramName );
}

//--///}}}1/////////////// vim:syntax=cpp:foldmethod=marker:ts=4:noexpandtab: 
