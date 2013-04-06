#include <boost/asio.hpp>
#include <array>


struct mcData {
    bool succes = false;
    std::string motd;
    std::string gametype;
    std::string map;
    std::string numplayers;  // int seems better, but this is how we recieve it from minecraft
    std::string maxplayers;  // and we want to prevent the caller from having to converting it back to std::string
    short hostport = 0;
    std::string hostip;
};

struct mcQuery {
    mcQuery(const char* host = "localhost", 
            const char* port = "25565", 
            const int timeoutsecs = 5);

    mcData get();
    void challengeReceiver(const boost::system::error_code& error, size_t nBytes);
    void dataReceiver(const boost::system::error_code& error, size_t nBytes);

private:
    boost::asio::io_service ioService;
    boost::asio::deadline_timer t;
    boost::asio::ip::udp::resolver Resolver;
    boost::asio::ip::udp::resolver::query Query;
    boost::asio::ip::udp::socket Socket;
    boost::asio::ip::udp::endpoint Endpoint;

    std::array<unsigned char,100> recvBuffer;
    mcData data;
};
