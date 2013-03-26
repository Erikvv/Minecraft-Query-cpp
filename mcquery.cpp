// g++ mcquery.cpp -o mcquery.run -lboost_system -pthread -lboost_thread
#include <boost/asio.hpp>
#include <iostream>

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

// global variables
io_service ioService;
udp::resolver resolver(ioService);

struct mcData {
   int nPlayers;
   int maxPlayers;
   string version;
};

/*class server {
public:
   // input: hostname (or ip), port number
   server(string ip, string port);
   ~server() {};

   
private:
};*/

mcData mcQuery(const string& ip, const string& port) {
   try {
      udp::resolver::query query(ip::udp::v4(), ip, port);
      udp::endpoint mcserverEndpoint = *resolver.resolve(query);
      
      cout<< "connecting to " << mcserverEndpoint << endl;
      udp::socket mcSocket(ioService);
      mcSocket.open(udp::v4());

      char req[] =  { 0xFE, 0xFD, 0x09, 0x00, 0x00, 0x00, 0x0F };
      cout<< "sending..." << endl;
      size_t len = mcSocket.send_to(buffer(req), mcserverEndpoint);
      cout<< "sent " << len << " bytes" << endl;

      cout<< "preparing recieve buffer" << endl;
      char recvBuffer[16] = { 0 };
      udp::endpoint thisEndpoint;
      cout<< "waiting on response..." << endl;
      len = mcSocket.receive_from(buffer(recvBuffer), thisEndpoint);

      cout<< "recieved " << len << " bytes" << endl;
      // byte 0 is 0x09
      // byte 1 to 4 is the session id (last 4 bytes of the request above)
      // byte 5 onwards is a null-terminated big endian ASCII number string which should be converted to an integer
      int challtoken = atoi(&recvBuffer[5]);    // undefined behavior when string is incorrect
      cout<< "challenge token: " << challtoken << endl;
   }
   catch (exception& e) {
      cout<< e.what();
   }
   

   mcData resp;
   return resp;
}
   

int main( int argc, char * argv[] )
{
   mcData d1 = mcQuery("localhost", "25565");

   return 0;   // success
}

