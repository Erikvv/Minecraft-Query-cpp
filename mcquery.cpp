// Compiles on my debian system with:
// g++ mcquery.cpp -o mcquery.run -lboost_system -pthread -lboost_thread -std=c++11
#include <boost/asio.hpp>
#include <iostream>
#include <functional>   // for bind
#include <array>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std::placeholders;

// global variables
io_service ioService;
udp::resolver resolver(ioService);

struct mcData {
   int nPlayers;
   int maxPlayers;
   string version;
};

//forward declarations (will be in header at some point)
mcData mcQuery(const string& ip, const string& port, int timeoutsecs = 5);
void challengeReciever(const boost::system::error_code& error, size_t nBytes, const array<char,16>& recvBuffer);

/***** main *****/
int main( int argc, char * argv[] )
{
   mcData d1 = mcQuery("localhost", "25565");

   return 0;   // success
}


mcData mcQuery(const string& ip, const string& port, int timeoutsecs /* = 5 */) {
   try {
      udp::resolver::query query(ip::udp::v4(), ip, port);
      udp::endpoint mcserverEndpoint = *resolver.resolve(query);
      
      cout<< "connecting to " << mcserverEndpoint << endl;
      udp::socket mcSocket(ioService);
      mcSocket.open(udp::v4());
                            /* [-magic--]  [type]  [----session id------] */
      unsigned char req[] =  { 0xFE, 0xFD, 0x09  , 0x0F, 0x0F, 0x0F, 0x0F };
      cout<< "sending..." << endl;
      size_t len = mcSocket.send_to(buffer(req), mcserverEndpoint);  // UDP: doesn't need to be async
      cout<< "sent " << len << " bytes" << endl;

      deadline_timer t(ioService, boost::posix_time::seconds(timeoutsecs));
      t.async_wait(
         [&](const boost::system::error_code& e) {
         cout<< "timed out: closing socket";
         mcSocket.close();
      } );

      cout<< "preparing recieve buffer" << endl;
      array<char,16> recvBuffer = { 0 };
      mcSocket.async_receive_from(buffer(recvBuffer), mcserverEndpoint, 
            bind(challengeReciever, _1, _2, std::cref(recvBuffer)));

      ioService.run();
      
   } 
   catch (exception& e) {
      cout<< e.what();
   }
   

   mcData resp;
   return resp;
}

void challengeReciever(const boost::system::error_code& error, size_t nBytes, const array<char,16>& recvBuffer) {
   cout<< "received " << nBytes << " bytes" << endl;
   // byte 0 is 0x09
   // byte 1 to 4 is the session id (last 4 bytes of the request we sent). We don't use this.
   // byte 5 onwards is the challange token: a null-terminated big endian ASCII number string which should be sent back as a 32-bit integer
   int challtoken = atoi(&recvBuffer[5]);    // I don't think this is safe (or defined)
   cout<< "challenge token: " << challtoken << endl;
}
   


