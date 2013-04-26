#include <boost/asio.hpp>
#include <array>
#include <sstream>

// we don't wanna pollute the global namespace
namespace mcq {

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;
using boost::system::error_code;

/*******************************
 *  DECLARATIONS               *
 *******************************/

    /*******************************
     *  mcData                     *
     *******************************/

// base class: not instantiated
struct data {
    data(const char* host, unsigned short port);
    
    bool success = false;

    // inputs
    string inputName;
    unsigned short inputPort;

    // all other data types are output, also in derived
    string error;  // contains the .what() string if an exception was thrown
    string motd;
    int numplayers;
    int maxplayers;
};

struct dataListping : data {
    string version;
};

struct dataBasic : data {
    string gametype;
    string map;
    unsigned short hostport = 0;
    string hostip;
};

struct dataFull : dataBasic {
    string& hostname = motd;   // same thing different name
    string game_id;
    string version;
    vector<string> plugins;     // only used by bukkit
    vector<string> playernames; // a name is max 16 chars, so a fixed-length string type would be more appropriate
};

struct mcDataSnoop {    // snooper not implemented: waiting on upstream fixes
    int difficulty;     // data types tbd
    int modlist;
    int uptime;
    int worlborder;
    int tps;
};

struct mcDataPlayer {
    char name[17];
    int gamemode;
    int capabilities;
    int armor;
    int xp;
    int group;
    int health;
    int food;
    int ping;
    int money;
    int pos;
};

struct mcDataInv {
    char name[17];
    int head;
    int chest;
    int legs;
    int feet;
    int inv;
};


    /*******************************
     *  traits object               *
     *******************************/

template<typename DATA> 
struct queryTraits;

    /*******************************
     *  helper                     *
     *******************************/

template<typename DATA>
struct temporaries;

    /*******************************
     *  query object               *
     *******************************/

template<typename DATA>
struct genericQuery {
    using protocol = typename queryTraits<DATA>::protocol;
    using socket   = typename protocol::socket;
    using endpoint = typename protocol::endpoint;
    using resolver = typename protocol::resolver;

    // constructor
    explicit genericQuery(const int timeoutsecs = 5);

    // public interfaces
    // could use future instead of shared_ptr for thread safety
    shared_ptr<DATA> add(const char* host, unsigned short port = 25565);
    void run();


private:    // functions
    void stepResolve();

private:    // data
    boost::asio::io_service ioService;
    boost::asio::deadline_timer t;
    istringstream iss; // dunno if this should be a member

    boost::posix_time::time_duration timeout;
    array<unsigned char,5000> recvBuffer;   // should look into making the buffer size variable, have to look at boost documentation
    vector<shared_ptr<DATA>> vData;
    vector<temporaries<DATA>> vTemp;
};

/*******************************
 *  DEFINITIONS                *
 *******************************/

    /*******************************
     *  helper container: cleared after every run *
     *******************************/

template<typename DATA>
struct temporaries {
    using protocol = typename queryTraits<DATA>::protocol;
    using socket   = typename protocol::socket;
    using endpoint = typename protocol::endpoint;
    using resolver = typename protocol::resolver;
    using resQuery = typename resolver::query;
    using resIterator = typename resolver::iterator;

    temporaries(shared_ptr<DATA>& d, io_service io) 
        : Socket{io},
          Resolver{io},
          data{d}
    { }

    void startEvent() {
        resQuery q{data->inputName, to_string(data->inputPort)};
        Resolver.async_resolve(q, [](error_code ec, resIterator it)
                { cout<< "ai"; });

    }


    weak_ptr<DATA> data;
    socket Socket;
    endpoint Endpoint;
    resolver Resolver;
};
    

    /*******************************
     *  traits object               *
     *******************************/

// no default implementation of queryTraits
template<> 
struct queryTraits<dataListping> 
{
    using protocol = tcp;
};
template<>
struct queryTraits<dataBasic>
{
    using protocol = udp;
};
template<>
struct queryTraits<dataFull>
{
    using protocol = udp;
    
};

    /*******************************
     *  data object               *
     *******************************/

data::data(const char* host, unsigned short port) 
      : inputName {host},
        inputPort {port}
{ }

    /*******************************
     *  query object               *
     *******************************/

template<typename DATA>
genericQuery<DATA>::genericQuery(const int timeoutsecs /* =5 */ ) 
    : ioService {}, 
      t {ioService},
      timeout {boost::posix_time::seconds(timeoutsecs)}
{ }

template<typename DATA>
shared_ptr<DATA> genericQuery<DATA>::add(const char* host, ushort port /* =25565 */) 
{
    auto temp = std::make_shared<DATA>(host, port);
    vData.push_back(temp);
    return temp;
}

template<typename DATA>
void genericQuery<DATA>::run() {
    stepResolve();

    ioService.run();
}

template<typename DATA>
void genericQuery<DATA>::stepResolve() {
    auto curData = vData[0];
    //resolver::query(cur
}








}   // end of internal namespace

// pull everything the user needs into the global namespace
