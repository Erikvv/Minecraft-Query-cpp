//#include <boost/asio.hpp>
//#include <array>
#include <functional>   // for bind
#include <algorithm>    // std::equal
#include <sstream>
#include "mcquery.hpp"

using namespace boost::asio::ip;
using namespace boost::asio;
using namespace std::placeholders;
using namespace std;

using boost::posix_time::seconds;
using boost::system::error_code;

using endpoint   = udp::endpoint;
using resolver   = udp::resolver;
using udp_socket = udp::socket;

using uchar = unsigned char;
using uint  = unsigned int;

/******************
 *  definitions   *
 ******************/
void log(string msg) {
    cout<< msg << endl;
}

mcQuery::mcQuery(const char* host /* = "localhost" */, 
                 const char* port /* = "25565" */, 
                 const int timeoutsecs /* = 5 */)
    : ioService {}, 
      t {ioService, seconds{timeoutsecs}},
      Resolver {ioService},
      Socket {ioService},
      Query {host, port},
      Endpoint {*Resolver.resolve(Query)}   // TODO: async
{  }
mcQuery::~mcQuery() {
    delete data;
}

mcQueryBasic::mcQueryBasic(const char* host, 
                 const char* port, 
                 const int timeoutsecs)
    : mcQuery(host, port, timeoutsecs)
{ 
    data = new mcDataBasic;
}

mcQueryFull::mcQueryFull(const char* host, 
                 const char* port, 
                 const int timeoutsecs)
    : mcQuery(host, port, timeoutsecs)
{ 
    data = new mcDataFull;
}      

mcDataBasic mcQuery::get() {
    Socket.connect(Endpoint);
      // request for challenge token
      //             [-magic--]  [type] [----session id------]
    uchar req[] =  { 0xFE, 0xFD, 0x09,  0x01, 0x02, 0x03, 0x04 };
    cout<< "sending..." << endl;
    size_t len = Socket.send_to(buffer(req), Endpoint);  // connectionless UDP: doesn't need to be async
    log("sent " + to_string(len) + " bytes");

    t.async_wait(
        [&](const boost::system::error_code& e) {
            if(e) return;
            cout<< "timed out: closing socket";
            Socket.cancel(); // causes event handlers to be called with error code 125 (asio::error::operation_aborted)
        } );
    
    cout<< "preparing recieve buffer" << endl;
    Socket.async_receive_from(buffer(recvBuffer), Endpoint, 
        bind(&mcQuery::challengeReceiver, this, _1, _2));

    try { ioService.run(); } catch(exception& e) {
        cout<< e.what();
    }

    return *data;
}

void mcQuery::challengeReceiver(const error_code& error, size_t nBytes) {
    if(error) return;   // recieve failed, probably cancelled by timer
    cout<< "received " << nBytes << " bytes" << endl;
    // byte 0 is 0x09
    // byte 1 to 4 is the session id (last 4 bytes of the request we sent xor'ed with 0F0F0F0F).
    // These bytes don't hold usefull info, but we check if they are correct anyways
    const array<uchar,5> expected = { 0x09, 0x01, 0x02, 0x03, 0x04 };
    if( !equal(expected.begin(), expected.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving data");

    // byte 5 onwards is the challange token: a null-terminated ASCII number string which should be sent back as a 32-bit integer
    uint challtoken = atoi((char*)&recvBuffer[5]); 
    
    // the actual request
    //                      [-magic--]  [type] [----session id------]
    vector<uchar> req { 0xFE, 0xFD, 0x00,  0x01, 0x02, 0x03, 0x04,
        static_cast<uchar>(challtoken>>24 & 0xFF),
        static_cast<uchar>(challtoken>>16 & 0xFF),
        static_cast<uchar>(challtoken>>8  & 0xFF),
        static_cast<uchar>(challtoken>>0  & 0xFF)
    };
    appendzeroes(req);     // virtual function call: appends four null bytes for the full stat
    
    cout<< "sending actual request" << endl;
    Socket.send_to(buffer(req), Endpoint);    
    Socket.async_receive_from(buffer(recvBuffer), Endpoint, bind(&mcQuery::dataReceiver, this, _1, _2));

}

void mcQuery::dataReceiver(const boost::system::error_code& error, size_t nBytes) {
    t.cancel(); // causes event handler to be called with boost::asio::error::operation_aborted
    if(error) return;   // recieve failed
    cout<< "received " << nBytes << " bytes" << endl;
    
    const array<uchar,5> expected = { 0x00, 0x01, 0x02, 0x03, 0x04 };
    if( !equal(expected.begin(), expected.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving data");

    // tokenize answer into mcData struct
    istringstream iss;
    iss.rdbuf()->pubsetbuf(reinterpret_cast<char*>(&recvBuffer[5]), recvBuffer.size());
    for( auto p : recvBuffer )
        cout<<p;

    getline(iss, data->motd, '\0');
    getline(iss, data->gametype, '\0');
    getline(iss, data->map, '\0');
    getline(iss, data->numplayers, '\0');
    getline(iss, data->maxplayers, '\0');
    iss.readsome(reinterpret_cast<char*>(&data->hostport), sizeof(data->hostport));
    getline(iss, data->hostip, '\0');

    data->succes = true;
}

void mcQuery::appendzeroes(std::vector<unsigned char>& req) {
    // do nothing
}

void mcQueryFull::appendzeroes(std::vector<unsigned char>& req) {
    req.push_back( 0x00 );
    req.push_back( 0x00 );
    req.push_back( 0x00 );
    req.push_back( 0x00 );
}
