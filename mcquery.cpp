//#include <boost/asio.hpp>
//#include <array>
#include <functional>   // for bind
#include <algorithm>    // std::equal
#include "mcquery.hpp"
#include <iomanip> // remove this!

using namespace boost::asio;
using namespace std::placeholders;
using namespace std;

using boost::posix_time::seconds;
using boost::system::error_code;

using uchar = unsigned char;
using uint  = unsigned int;


struct debuglog {
    template<typename T>
    debuglog& operator<< (T rhs) { 
        //cout<< rhs;
        return *this; 
    }
} debug;

/*************************
 *  mcQuery definitions  *
 *************************/

// non-class helper function;
vector<string> extractPlugins(string raw);

mcQuery::mcQuery(const char* host /* = "localhost" */, 
                 const char* port /* = "25565" */, 
                 const int timeoutsecs /* = 5 */)
    : ioService {}, 
      t {ioService},
      Resolver {ioService},
      Query {host, port},
      Socket {ioService},
      timeout {seconds(timeoutsecs)}         
{ }

mcDataBasic mcQuery::getBasic() {
    fullreq = false;
    connect();

    return move(static_cast<mcDataBasic>(data));
}
mcDataFull mcQuery::getFull() {
    fullreq = true;
    connect();

    return move(data);
}

void mcQuery::connect() {
    data.success = false;

    t.expires_from_now(timeout);
    t.async_wait(
        [&](const error_code& e) {
            if(e) return;
            data.error = "User-defined timeout reached";
            Socket.cancel(); // causes event handlers to be called with error code 125 (asio::error::operation_aborted)
        } );

    try {
        Endpoint = *Resolver.resolve(Query);    // has a good timeout of itself, so async probably isnt necessary
        Socket.connect(Endpoint);
          // request for challenge token
          //             [-magic--]  [type] [----session id------]
        uchar req[] =  { 0xFE, 0xFD, 0x09,  0x01, 0x02, 0x03, 0x04 };
        debug<< "sending..." << '\n';
        size_t len = Socket.send_to(buffer(req), Endpoint);  // connectionless UDP: doesn't need to be async
        debug<< "sent " << len << " bytes" << '\n';
        
        debug<< "preparing recieve buffer" << '\n';
        Socket.async_receive_from(buffer(recvBuffer), Endpoint, 
            bind(&mcQuery::challengeReceiver, this, _1, _2));

        ioService.reset();
        ioService.run(); 
    } catch(exception& e) {
        data.error = e.what();
    }
}

void mcQuery::challengeReceiver(const error_code& error, size_t nBytes) {
    if(error) return;   // recieve failed, probably cancelled by timer
    debug<< "received " << nBytes << " bytes" << '\n';
    // byte 0 is 0x09
    // byte 1 to 4 is the session id (last 4 bytes of the request we sent xor'ed with 0F0F0F0F).
    // These bytes don't hold usefull info, but we check if they are correct anyways
    const array<uchar,5> expected = { 0x09, 0x01, 0x02, 0x03, 0x04 };
    if( !equal(expected.begin(), expected.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving challenge token");

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
    
    debug<< "sending actual request" << '\n';
    Socket.send_to(buffer(req), Endpoint);    
    Socket.async_receive_from(buffer(recvBuffer), Endpoint, bind(&mcQuery::dataReceiver, this, _1, _2));
}

void mcQuery::dataReceiver(const boost::system::error_code& error, size_t nBytes) {
    t.cancel(); // causes event handler to be called with boost::asio::error::operation_aborted
    if(error) return;   // recieve failed
    debug<< "received " << nBytes << " bytes" << '\n';
    
    const array<uchar,5> expected = { 0x00, 0x01, 0x02, 0x03, 0x04 };
    if( !equal(expected.begin(), expected.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving data");

    // tokenize answer into mcData struct
    iss.rdbuf()->pubsetbuf(reinterpret_cast<char*>(&recvBuffer[5]), recvBuffer.size()-5);

    extract();
    data.success = true;    // the only place where this flag can be set to true
}

void mcQuery::extract() {
    if( fullreq ) extractFull();
    else extractBasic();
}

void mcQuery::extractBasic() {
    getline(iss, data.motd, '\0');
    getline(iss, data.gametype, '\0');
    getline(iss, data.map, '\0');
    iss >> data.numplayers;
    iss.ignore(1);
    iss >> data.maxplayers;
    iss.ignore(1);
    iss.readsome(reinterpret_cast<char*>(&data.hostport), sizeof(data.hostport));
    getline(iss, data.hostip, '\0');
}

void mcQuery::extractFull() {
    extractKey("splitnum");

    iss.ignore(2);
    extractKey("hostname");
    getline(iss, data.motd, '\0');

    extractKey("gametype");
    getline(iss, data.gametype, '\0');

    extractKey("game_id");
    getline(iss, data.game_id, '\0');

    extractKey("version");
    getline(iss, data.version, '\0');

    extractKey("plugins");
    string rawPlugins;
    getline(iss, rawPlugins, '\0');
    data.plugins = extractPlugins(rawPlugins);

    extractKey("map");
    getline(iss, data.map, '\0');

    extractKey("numplayers");
    iss >> data.numplayers;
    iss.ignore(1);

    extractKey("maxplayers");
    iss >> data.maxplayers;
    iss.ignore(1);

    extractKey("hostport");
    iss >> data.hostport;
    iss.ignore(1);

    extractKey("hostip");
    getline(iss, data.hostip, '\0');
    iss.ignore(2);

    extractKey("player_");
    iss.ignore(1);

    string name;
    while( iss.good() && iss.peek()!='\0' ) {
        getline(iss, name, '\0');
        data.playernames.push_back(name);
    } 
}

void mcQuery::extractKey(const char* expected) {
    string temp;
    getline(iss, temp, '\0');
    if( temp.compare(expected) )
        throw runtime_error("Unexpected key found in server data");
}

vector<string> extractPlugins(string raw) {
    vector<string> vs;
    if( !raw.size() )
        return vs;
    istringstream pss(raw);
    string temp;
    getline(pss, temp, ':'); // craftbukkit description
    vs.push_back(temp);

    while( pss.good() ) {
        pss.ignore(1); // whitespace
        getline(pss, temp, ';');    // plugin name
        vs.push_back(temp);
    }

    return vs;
}

/*******************************
 *  mcQuerySimple definitions  *
 *******************************/

mcQuerySimple::mcQuerySimple(
        const char* host /* = "localhost" */, 
        const char* port /* = "25565" */, 
        const int timeoutsecs /* = 5 */)
    : ioService {},
      t {ioService},
      Resolver {ioService},
      Query {host, port},
      Socket {ioService},
      timeout {seconds(timeoutsecs)}
{ }

mcDataSimple mcQuerySimple::get() {
    data.success = false;

    t.expires_from_now(timeout);
    t.async_wait(
        [&](const error_code& e) {
            if(e) return;
            data.error = "User-defined timeout reached";
            Socket.cancel(); // causes event handlers to be called with error code 125 (asio::error::operation_aborted)
        } );
    
    try {
        Endpoint = *Resolver.resolve(Query);
   
        Socket.async_connect(Endpoint, bind(&mcQuerySimple::connector, this, _1));

        ioService.reset();
        ioService.run(); 
    } catch(exception& e) {
        data.error = e.what();
    }

    return data;
}

void mcQuerySimple::connector(const error_code& e) {    // TODO: try totally different ordering
    if(e) return;

    uchar req[] = { 0xFE, 0x01 };

    Socket.async_send(buffer(req), bind(&mcQuerySimple::sender, this, _1, _2));
}

void mcQuerySimple::sender(const error_code& e, size_t numBytes) {
    if(e) return;

    Socket.async_receive(buffer(recvBuffer), bind(&mcQuerySimple::receiver, this, _1, _2));    
}

void mcQuerySimple::receiver(const error_code& e, size_t numBytes) {    // does not work for mc 1.3 and earlier
    t.cancel();
    if(e) return;
    debug<< "received " << numBytes << " bytes\n";

    array<uchar,2> expected = { 0xFF, 0x00 };
    if( !equal(expected.begin(), expected.end(), recvBuffer.begin()) )
        throw runtime_error("Incorrect response from server when recieving data");

    // remove all uneven indexed bytes (they're all zero)
    for( int i=0; i<recvBuffer.size()/2; i++) {
        recvBuffer[i] = recvBuffer[i*2];
        recvBuffer[i*2] = '\0';
    }

    istringstream iss;
    iss.rdbuf()->pubsetbuf(reinterpret_cast<char*>(&recvBuffer[8]), recvBuffer.size()-8);

    getline(iss, data.version, '\0');
    getline(iss, data.motd, '\0');
    iss >> data.numplayers;
    iss.ignore(1);
    iss >> data.maxplayers;
    
    data.success = true;
}

