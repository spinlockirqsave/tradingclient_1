#ifdef _WIN32
# include <windows.h>
# define sleep( seconds) Sleep( seconds * 1000);
#else
# include <unistd.h>
#endif

#include "PosixClient.h"


const unsigned MAX_ATTEMPTS = 1;
const unsigned SLEEP_TIME = 10;

int main(int argc, char** argv)
{
	const char* host = argc > 1 ? argv[1] : "";
	unsigned int port = argc > 2 ? atoi(argv[2]) : 7496;
	int clientId = 16;

	unsigned attempt = 0;
	printf( "tradingclient_1: start %u\n", attempt);

	for (;;) {
		++attempt;
		printf( "Attempt %u of %u\n", attempt, MAX_ATTEMPTS);

		IB::PosixClient client;
                

		client.connect( host, port, clientId);

		while( client.isConnected()) {
			client.processMessages();
		}

		if( attempt >= MAX_ATTEMPTS) {
			break;
		}

		printf( "tradingclient_1: sleeping %u seconds before next attempt\n", SLEEP_TIME);
		sleep( SLEEP_TIME);
	}

	printf ( "tradingclient_1: end\n");
}

