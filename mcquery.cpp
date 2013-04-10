//#include <boost/asio.hpp>
//#include <array>
#include <functional>   // for bind
#include <algorithm>    // std::equal
#include <ostream>
#include <streambuf>
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



struct logbuf : std::streambuf {

} mylogbuf;

ostream debuglog(&mylogbuf);

/******************
 *  definitions   *
 ******************/
mcQuery::mcQuery(const char* host /* = "localhost" */, 
                 const char* port /* = "25565" */, 
                 const int timeoutsecs /* = 5 */)
    : ioService {}, 
      t {ioService},
      Resolver {ioService},
      Socket {ioService},
      Query {host, port},
      Endpoint {*Resolver.resolve(Query)},   // TODO: async
      timeout{timeoutsecs}
{ }

mcDataBasic mcQuery::getBasic() {
    fullreq = false;
    connect();

    return static_cast<mcDataBasic>(data);   // is move desirable/necessary here?
}
mcDataFull mcQuery::getFull() {
    fullreq = true;
    connect();

    return data;
}

void mcQuery::connect() {
    Socket.connect(Endpoint);
      // request for challenge token
      //             [-magic--]  [type] [----session id------]
    uchar req[] =  { 0xFE, 0xFD, 0x09,  0x01, 0x02, 0x03, 0x04 };
    debuglog<< "sending..." << endl;
    size_t len = Socket.send_to(buffer(req), Endpoint);  // connectionless UDP: doesn't need to be async
    debuglog<< "sent " << len << " bytes";

    t.expires_from_now(seconds(timeout));
    t.async_wait(
        [&](const boost::system::error_code& e) {
            if(e) return;
            debuglog<< "timed out: closing socket"<< endl;
            Socket.cancel(); // causes event handlers to be called with error code 125 (asio::error::operation_aborted)
        } );
    
    debuglog<< "preparing recieve buffer" << endl;
    Socket.async_receive_from(buffer(recvBuffer), Endpoint, 
        bind(&mcQuery::challengeReceiver, this, _1, _2));

    ioService.reset();
    try { ioService.run(); } catch(exception& e) {
        debuglog<< "Exception caught from ioService: " << e.what() << endl;
    }
}

void mcQuery::challengeReceiver(const error_code& error, size_t nBytes) {
    if(error) return;   // recieve failed, probably cancelled by timer
    debuglog<< "received " << nBytes << " bytes" << endl;
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
    if(fullreq) {
        req.push_back(0x00);
        req.push_back(0x00);
        req.push_back(0x00);
        req.push_back(0x00);
    }
    
    debuglog<< "sending actual request" << endl;
    Socket.send_to(buffer(req), Endpoint);    
    Socket.async_receive_from(buffer(recvBuffer), Endpoint, bind(&mcQuery::dataReceiver, this, _1, _2));

}

void mcQuery::dataReceiver(const boost::system::error_code& error, size_t nBytes) {
    t.cancel(); // causes event handler to be called with boost::asio::error::operation_aborted
    if(error) return;   // recieve failed
    debuglog<< "received " << nBytes << " bytes" << endl;
    
    const array<uchar,5> expected = { 0x00, 0x01, 0x02, 0x03, 0x04 };
    if( !equal(expected.begin(), expected.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving data");

    // tokenize answer into mcData struct
    istringstream iss;
    iss.rdbuf()->pubsetbuf(reinterpret_cast<char*>(&recvBuffer[5]), recvBuffer.size());

    extract(iss);
    data.succes = true;
}

void mcQuery::extract(istringstream& iss) {
    if( fullreq ) extractFull(iss);
    else extractBasic(iss);
}

void mcQuery::extractBasic(istringstream& iss) {
    getline(iss, data.motd, '\0');
    getline(iss, data.gametype, '\0');
    getline(iss, data.map, '\0');
    getline(iss, data.numplayers, '\0');
    getline(iss, data.maxplayers, '\0');
    iss.readsome(reinterpret_cast<char*>(&data.hostport), sizeof(data.hostport));
    getline(iss, data.hostip, '\0');
}
void mcQuery::extractFull(istringstream& iss) {
    string temp;

    getline(iss, temp, '\0');
    if( temp.compare("splitnum") )
        throw runtime_error("Incorrect response from server, expected 'splitnum'");

    iss.ignore(2);
    getline(iss, temp, '\0');
    if( temp.compare("hostname") )
        throw runtime_error("Incorrect response from server, expected 'hostname'");

    getline(iss, data.motd, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("gametype") )
        throw runtime_error("Incorrect response from server, expected 'gametype'");
    getline(iss, data.gametype, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("game_id") )
        throw runtime_error("Incorrect response from server, expected 'game_id'");
    getline(iss, data.game_id, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("version") )
        throw runtime_error("Incorrect response from server, expected 'version'");
    getline(iss, data.version, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("plugins") )
        throw runtime_error("Incorrect response from server, expected 'plugins'");
    getline(iss, data.plugins, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("map") )
        throw runtime_error("Incorrect response from server, expected 'map'");
    getline(iss, data.map, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("numplayers") )
        throw runtime_error("Incorrect response from server, expected 'numplayers'");
    getline(iss, data.numplayers, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("maxplayers") )
        throw runtime_error("Incorrect response from server, expected 'maxplayers'");
    getline(iss, data.maxplayers, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("hostport") )
        throw runtime_error("Incorrect response from server, expected 'hostport'");
    iss>> data.hostport;

    iss.ignore(1);
    getline(iss, temp, '\0');
    if( temp.compare("hostip") )
        throw runtime_error("Incorrect response from server, expected 'hostip'");
    getline(iss, data.hostip, '\0');

    iss.ignore(2);
    getline(iss, temp, '\0');
    if( temp.compare("player_") )
        throw runtime_error("Incorrect response from server, expected 'player_'");

    iss.ignore(1);
    array<char,17> buf;
    while(1) {
        iss.getline(&buf[0], 17, '\0');
        if( strlen(&buf[0]) )
            data.playernames.push_back(buf);
        else break;
    } 
}


