// Compiles on my debian system with:
// g++ mcquery.cpp -o mcquery.run -lboost_system -pthread -lboost_thread -std=c++11
#include <boost/asio.hpp>
#include <iostream>
#include <functional>   // for bind
#include <array>
#include <algorithm>    // std::equal

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::placeholders;

using uchar = unsigned char;
using uint  = unsigned int;

// global variables
io_service ioService;
udp::resolver resolver(ioService);

// data struct to be used by all protocols (using inheritance?)
struct mcData {
   int nPlayers;
   int maxPlayers;
   string version;
};

//forward declarations (will be in header at some point)
mcData mcQuery(const string& ip, const string& port = "25565", int timeoutsecs = 5);
void challengeReceiver(const boost::system::error_code& error, size_t nBytes, array<uchar,100>& recvBuffer, udp::socket& mcSocket, udp::endpoint& mcServerEndpoint);
void dataReceiver(const boost::system::error_code& error, size_t nBytes, array<uchar,100>& recvBuffer);

/***** 
 * main 
 * 
 * not part of the final product, just for testing
 ******/
int main( int argc, char * argv[] )
{
    mcData d1 = mcQuery("localhost");
    return 0;   // success
}

/***** 
 * udp query 
 * todo: 
 * - retry on timeout
 * - return data
 ******/
mcData mcQuery(const string& ip, const string& port /* = "25565" */, int timeoutsecs /* = 5 */) {
   try {
      udp::resolver::query query { ip::udp::v4(), ip, port } ;
      udp::endpoint mcServerEndpoint = *resolver.resolve(query);
      
      cout<< "connecting to " << mcServerEndpoint << endl;
      udp::socket mcSocket(ioService);
      mcSocket.open(udp::v4());
      // request for challenge token
      //               [-magic--]  [type] [----session id------]
      uchar req[] =  { 0xFE, 0xFD, 0x09,  0x01, 0x02, 0x03, 0x04 };
      cout<< "sending..." << endl;
      size_t len = mcSocket.send_to(buffer(req), mcServerEndpoint);  // UDP: doesn't need to be async
      cout<< "sent " << len << " bytes" << endl;

      deadline_timer t(ioService, boost::posix_time::seconds(timeoutsecs));
      t.async_wait(
         [&](const boost::system::error_code& e) {
         cout<< "timed out: closing socket";
         mcSocket.cancel(); // causes event handlers to be called with error code 125 (asio::error::operation_aborted)
      } );

      cout<< "preparing recieve buffer" << endl;
      array<uchar,100> recvBuffer;     // is used for both of the server responses. 100 bytes should be plenty
      mcSocket.async_receive_from(buffer(recvBuffer), mcServerEndpoint, 
            bind(challengeReceiver, _1, _2, ref(recvBuffer), ref(mcSocket), ref(mcServerEndpoint)));

      ioService.run();
      
   } 
   catch (exception& e) {
      cout<< e.what();
   }
   
   // how to return the data???
   mcData data;
   return data;

}

void challengeReceiver(const boost::system::error_code& error, size_t nBytes, array<uchar,100>& recvBuffer, udp::socket& mcSocket, udp::endpoint& mcServerEndpoint) {
    if(error) return;   // recieve failed
    cout<< "received " << nBytes << " bytes" << endl;
    // byte 0 is 0x09
    // byte 1 to 4 is the session id (last 4 bytes of the request we sent xor'ed with 0F0F0F0F).
    // These bytes don't hold usefull info, but we check if they are correct anyways
    const array<uchar,5> expectedbytes = { 0x09, 0x01, 0x02, 0x03, 0x04 };
    if( !equal(expectedbytes.begin(), expectedbytes.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving data");

    // byte 5 onwards is the challange token: a null-terminated ASCII number string which should be sent back as a 32-bit integer
    uint challtoken = atoi((char*)&recvBuffer[5]);  // unsafe?
    
    // the actual request
    //                      [-magic--]  [type] [----session id------]
    array<uchar,11> req = { 0xFE, 0xFD, 0x00,  0x01, 0x02, 0x03, 0x04 };
    req[10] = challtoken>>0  & 0xFF;
    req[9]  = challtoken>>8  & 0xFF;
    req[8]  = challtoken>>16 & 0xFF;
    req[7]  = challtoken>>24 & 0xFF;
    
    cout<< "sending actual request" << endl;
    mcSocket.send_to(buffer(req), mcServerEndpoint);    
    mcSocket.async_receive_from(buffer(recvBuffer), mcServerEndpoint, bind(dataReceiver, _1, _2, ref(recvBuffer)));

}

void dataReceiver(const boost::system::error_code& error, size_t nBytes, array<uchar,100>& recvBuffer) {
    if(error) return;   // recieve failed
    cout<< "received " << nBytes << " bytes" << endl;
    
    for(int i=0; i<recvBuffer.size(); i++ )
        cout<< recvBuffer[i] << ' ';
    //for(char e : recvBuffer)
    //    cout<< e;
    
    const array<uchar,5> expectedbytes = { 0x00, 0x01, 0x02, 0x03, 0x04 };
    if( !equal(expectedbytes.begin(), expectedbytes.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving data");

}
