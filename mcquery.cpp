//#include <boost/asio.hpp>
//#include <array>
#include <functional>   // for bind
#include <algorithm>    // std::equal
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
      Endpoint {*Resolver.resolve(Query)},   // TODO: async
      data(nullptr)
{  }
mcQuery::~mcQuery() {
    delete data;
}

mcDataBasic mcQuery::getBasic() {
    fullreq = false;
    data = new mcDataBasic;
    connect();

    return move(*data);   // is move desirable/necessary here?
}
mcDataFull mcQuery::getFull() {
    fullreq = true;
    data = new mcDataFull;  // TODO: free previous content
    connect();

    return move(*dynamic_cast<mcDataFull*>(data));
}

void mcQuery::connect() {
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
    if(fullreq) {
        req.push_back(0x00);
        req.push_back(0x00);
        req.push_back(0x00);
        req.push_back(0x00);
    }
    
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

    extract(iss);
    data->succes = true;
}

void mcQuery::extract(istringstream& iss) {
    if( fullreq ) extractFull(iss);
    else extractBasic(iss);
}

void mcQuery::extractBasic(istringstream& iss) {
    getline(iss, data->motd, '\0');
    getline(iss, data->gametype, '\0');
    getline(iss, data->map, '\0');
    getline(iss, data->numplayers, '\0');
    getline(iss, data->maxplayers, '\0');
    iss.readsome(reinterpret_cast<char*>(&data->hostport), sizeof(data->hostport));
    getline(iss, data->hostip, '\0');
}
void mcQuery::extractFull(istringstream& iss) {
    mcDataFull* fdata = dynamic_cast<mcDataFull*>(data);
    string temp;

    getline(iss, temp, '\0');
    if( temp.compare("splitnum") )
        throw runtime_error("Incorrect response from server, expected 'splitnum'");

    iss.ignore(2);
    getline(iss, temp, '\0');
    if( temp.compare("hostname") )
        throw runtime_error("Incorrect response from server, expected 'hostname'");

    getline(iss, fdata->motd, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("gametype") )
        throw runtime_error("Incorrect response from server, expected 'gametype'");
    getline(iss, fdata->gametype, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("game_id") )
        throw runtime_error("Incorrect response from server, expected 'game_id'");
    getline(iss, fdata->game_id, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("version") )
        throw runtime_error("Incorrect response from server, expected 'version'");
    getline(iss, fdata->version, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("plugins") )
        throw runtime_error("Incorrect response from server, expected 'plugins'");
    getline(iss, fdata->plugins, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("map") )
        throw runtime_error("Incorrect response from server, expected 'map'");
    getline(iss, fdata->map, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("numplayers") )
        throw runtime_error("Incorrect response from server, expected 'numplayers'");
    getline(iss, fdata->numplayers, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("maxplayers") )
        throw runtime_error("Incorrect response from server, expected 'maxplayers'");
    getline(iss, fdata->maxplayers, '\0');

    getline(iss, temp, '\0');
    if( temp.compare("hostport") )
        throw runtime_error("Incorrect response from server, expected 'hostport'");
    iss>> fdata->hostport;

    iss.ignore(1);
    getline(iss, temp, '\0');
    if( temp.compare("hostip") )
        throw runtime_error("Incorrect response from server, expected 'hostip'");
    getline(iss, fdata->hostip, '\0');

    iss.ignore(2);
    getline(iss, temp, '\0');
    if( temp.compare("player_") )
        throw runtime_error("Incorrect response from server, expected 'hostip'");

    iss.ignore(1);
    array<char,17> buf;
    while(1) {
        iss.getline(&buf[0], 17, '\0');
        if( strlen(&buf[0]) )
            fdata->players.push_back(buf);
        else break;
    } 
}


