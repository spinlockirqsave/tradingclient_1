#include "PosixClient.h"

#include "EPosixClientSocket.h"
/* In this example we just include the platform header to have select(). In real
   life you should include the needed headers from your system. */
#include "EPosixClientSocketPlatform.h"

#include "Contract.h"
#include "Order.h"

#include <time.h>
#include <sys/time.h>

#if defined __INTEL_COMPILER
# pragma warning (disable:869)
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wswitch"
# pragma GCC diagnostic ignored "-Wunused-parameter"
#endif  /* __INTEL_COMPILER */

namespace IB {

const int PING_DEADLINE = 2; // seconds
const int SLEEP_BETWEEN_PINGS = 30; // seconds

///////////////////////////////////////////////////////////
// member funcs
PosixClient::PosixClient()
	: m_pClient(new EPosixClientSocket(this))
	, m_state(ST_CONNECT)
	, m_sleepDeadline(0)
	, m_orderId(0)
{
}

PosixClient::~PosixClient()
{
}

bool PosixClient::connect(const char *host, unsigned int port, int clientId)
{
	// trying to connect
	printf( "Connecting to %s:%u clientId:%d\n", !( host && *host) ? "127.0.0.1" : host, port, clientId);

	bool bRes = m_pClient->eConnect2( host, port, clientId);

	if (bRes) {
		printf( "Connected to %s:%u clientId:%d\n", !( host && *host) ? "127.0.0.1" : host, port, clientId);
	}
	else
		printf( "Cannot connect to %s:%u clientId:%d\n", !( host && *host) ? "127.0.0.1" : host, port, clientId);

	return bRes;
}

void PosixClient::disconnect() const
{
	m_pClient->eDisconnect();

	printf ( "Disconnected\n");
}

bool PosixClient::isConnected() const
{
	return m_pClient->isConnected();
}

void PosixClient::processMessages()
{
	fd_set readSet, writeSet;

	struct timeval tval;
	tval.tv_usec = 0;
	tval.tv_sec = 0;

	time_t now = time(NULL);

	switch (m_state) {
		case ST_PLACEORDER:
			placeOrder();
			break;
		case ST_PLACEORDER_ACK:
			break;
		case ST_CANCELORDER:
			cancelOrder();
			break;
		case ST_CANCELORDER_ACK:
			break;
		case ST_PING:
			reqCurrentTime();
			break;
		case ST_PING_ACK:
			if( m_sleepDeadline < now) {
				disconnect();
				return;
			}
			break;
		case ST_IDLE:
			if( m_sleepDeadline < now) {
				m_state = ST_PING;
				return;
			}
			break;
	}

	if( m_sleepDeadline > 0) {
		// initialize timeout with m_sleepDeadline - now
		tval.tv_sec = m_sleepDeadline - now;
	}

	if( m_pClient->fd() >= 0 ) {

		FD_ZERO( &readSet);
		writeSet = readSet;

		FD_SET( m_pClient->fd(), &readSet);

		if( !m_pClient->isOutBufferEmpty())
			FD_SET( m_pClient->fd(), &writeSet);

		int ret = select( m_pClient->fd() + 1, &readSet, &writeSet, NULL, &tval);

		if( ret == 0) { // timeout
			return;
		}

		if( ret < 0) {	// error
			disconnect();
			return;
		}

		if( FD_ISSET( m_pClient->fd(), &writeSet)) {
			// socket is ready for writing
			m_pClient->onSend();
		}

		if( m_pClient->fd() < 0)
			return;

		if( FD_ISSET( m_pClient->fd(), &readSet)) {
			// socket is ready for reading
			m_pClient->onReceive();
		}
	}
}

//////////////////////////////////////////////////////////////////
// methods
void PosixClient::reqCurrentTime()
{
	printf( "--> Requesting Current Time2\n");

	// set ping deadline to "now + n seconds"
	m_sleepDeadline = time( NULL) + PING_DEADLINE;

	m_state = ST_PING_ACK;

	m_pClient->reqCurrentTime();
}

void PosixClient::placeOrder()
{
	Contract contract;
	Order order;

	contract.symbol = "MSFT";
	contract.secType = "STK";
	contract.exchange = "SMART";
	contract.currency = "USD";

	order.action = "BUY";
	order.totalQuantity = 1000;
	order.orderType = "LMT";
	order.lmtPrice = 26.7;

	printf( "Placing Order %ld: %s %ld %s at %f\n", m_orderId, order.action.c_str(), order.totalQuantity, contract.symbol.c_str(), order.lmtPrice);

	m_state = ST_PLACEORDER_ACK;

	m_pClient->placeOrder( m_orderId, contract, order);
}

void PosixClient::cancelOrder()
{
	printf( "Cancelling Order %ld\n", m_orderId);

	m_state = ST_CANCELORDER_ACK;

	m_pClient->cancelOrder( m_orderId);
}

///////////////////////////////////////////////////////////////////
// events
void PosixClient::orderStatus( OrderId orderId, const IBString &status, int filled,
	   int remaining, double avgFillPrice, int permId, int parentId,
	   double lastFillPrice, int clientId, const IBString& whyHeld)

{
	if( orderId == m_orderId) {
		if( m_state == ST_PLACEORDER_ACK && (status == "PreSubmitted" || status == "Submitted"))
			m_state = ST_CANCELORDER;

		if( m_state == ST_CANCELORDER_ACK && status == "Cancelled")
			m_state = ST_PING;

		printf( "Order: id=%ld, status=%s\n", orderId, status.c_str());
	}
}

void PosixClient::nextValidId( OrderId orderId)
{
	m_orderId = orderId;

	m_state = ST_PLACEORDER;
}

void PosixClient::currentTime( long time)
{
	if ( m_state == ST_PING_ACK) {
		time_t t = ( time_t)time;
		struct tm * timeinfo = localtime ( &t);
		printf( "The current date/time is: %s", asctime( timeinfo));

		time_t now = ::time(NULL);
		m_sleepDeadline = now + SLEEP_BETWEEN_PINGS;

		m_state = ST_IDLE;
	}
}

void PosixClient::error(const int id, const int errorCode, const IBString errorString)
{
	printf( "Error id=%d, errorCode=%d, msg=%s\n", id, errorCode, errorString.c_str());

	if( id == -1 && errorCode == 1100) // if "Connectivity between IB and TWS has been lost"
		disconnect();
}

void PosixClient::tickPrice( TickerId tickerId, TickType field, double price, int canAutoExecute) {}
void PosixClient::tickSize( TickerId tickerId, TickType field, int size) {}
void PosixClient::tickOptionComputation( TickerId tickerId, TickType tickType, double impliedVol, double delta,
											 double optPrice, double pvDividend,
											 double gamma, double vega, double theta, double undPrice) {}
void PosixClient::tickGeneric(TickerId tickerId, TickType tickType, double value) {}
void PosixClient::tickString(TickerId tickerId, TickType tickType, const IBString& value) {}
void PosixClient::tickEFP(TickerId tickerId, TickType tickType, double basisPoints, const IBString& formattedBasisPoints,
							   double totalDividends, int holdDays, const IBString& futureExpiry, double dividendImpact, double dividendsToExpiry) {}
void PosixClient::openOrder( OrderId orderId, const Contract&, const Order&, const OrderState& ostate) {}
void PosixClient::openOrderEnd() {}
void PosixClient::winError( const IBString &str, int lastError) {}
void PosixClient::connectionClosed() {}
void PosixClient::updateAccountValue(const IBString& key, const IBString& val,
										  const IBString& currency, const IBString& accountName) {}
void PosixClient::updatePortfolio(const Contract& contract, int position,
		double marketPrice, double marketValue, double averageCost,
		double unrealizedPNL, double realizedPNL, const IBString& accountName){}
void PosixClient::updateAccountTime(const IBString& timeStamp) {}
void PosixClient::accountDownloadEnd(const IBString& accountName) {}
void PosixClient::contractDetails( int reqId, const ContractDetails& contractDetails) {}
void PosixClient::bondContractDetails( int reqId, const ContractDetails& contractDetails) {}
void PosixClient::contractDetailsEnd( int reqId) {}
void PosixClient::execDetails( int reqId, const Contract& contract, const Execution& execution) {}
void PosixClient::execDetailsEnd( int reqId) {}

void PosixClient::updateMktDepth(TickerId id, int position, int operation, int side,
									  double price, int size) {}
void PosixClient::updateMktDepthL2(TickerId id, int position, IBString marketMaker, int operation,
										int side, double price, int size) {}
void PosixClient::updateNewsBulletin(int msgId, int msgType, const IBString& newsMessage, const IBString& originExch) {}
void PosixClient::managedAccounts( const IBString& accountsList) {}
void PosixClient::receiveFA(faDataType pFaDataType, const IBString& cxml) {}
void PosixClient::historicalData(TickerId reqId, const IBString& date, double open, double high,
									  double low, double close, int volume, int barCount, double WAP, int hasGaps) {}
void PosixClient::scannerParameters(const IBString &xml) {}
void PosixClient::scannerData(int reqId, int rank, const ContractDetails &contractDetails,
	   const IBString &distance, const IBString &benchmark, const IBString &projection,
	   const IBString &legsStr) {}
void PosixClient::scannerDataEnd(int reqId) {}
void PosixClient::realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
								   long volume, double wap, int count) {}
void PosixClient::fundamentalData(TickerId reqId, const IBString& data) {}
void PosixClient::deltaNeutralValidation(int reqId, const UnderComp& underComp) {}
void PosixClient::tickSnapshotEnd(int reqId) {}
void PosixClient::marketDataType(TickerId reqId, int marketDataType) {}

}
