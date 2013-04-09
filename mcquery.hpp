#include <boost/asio.hpp>
#include <array>



struct mcDataBasic {
    bool succes = false;
    std::string motd;
    std::string gametype;
    std::string map;
    std::string numplayers;  // int seems better, but this is how we recieve it from minecraft
    std::string maxplayers;
    short hostport = 0;
    std::string hostip;
};

struct mcDataFull : mcDataBasic {
    std::string& hostname = motd;   // same thing different name
    std::string game_id;
    std::string plugins;     // I think only used by bukkit: may need to become a vector of strings
    std::vector<char[17]> players;
};

struct mcQuery {
    mcQuery(const char* host = "localhost",     // TODO: move arguments away from constructor
            const char* port = "25565", 
            const int timeoutsecs = 5);
    virtual ~mcQuery();

    mcDataBasic get();
    void challengeReceiver(const boost::system::error_code& error, size_t nBytes);
    void dataReceiver(const boost::system::error_code& error, size_t nBytes);

protected:
    virtual void appendzeroes(std::vector<unsigned char>& req);
    mcDataBasic* data;
    std::array<unsigned char,100> recvBuffer;   // should look into making the buffer size variable, have to look in boost documentation

private:
    boost::asio::io_service ioService;
    boost::asio::deadline_timer t;
    boost::asio::ip::udp::resolver Resolver;
    boost::asio::ip::udp::resolver::query Query;
    boost::asio::ip::udp::socket Socket;
    boost::asio::ip::udp::endpoint Endpoint;
};

struct mcQueryBasic : mcQuery {
    mcQueryBasic(const char* host = "localhost", 
            const char* port = "25565", 
            const int timeoutsecs = 5);
};

struct mcQueryFull : mcQuery {
    mcQueryFull(const char* host = "localhost", 
            const char* port = "25565", 
            const int timeoutsecs = 5);

private:
    virtual void appendzeroes(std::vector<unsigned char>& req);
};
