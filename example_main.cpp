// compiles on gcc 4.7.2 with:
// g++ mcquery.cpp example_main.cpp -o mcquery.run -lboost_system -pthread -lboost_thread -std=c++11
#include "mcquery.hpp"

using namespace std;

int main() { 
    mcQuery q;
    auto data = q.get("localhost");
    
    if(data.succes) {
        cout<< "data.motd:       " << data.motd << endl;
        cout<< "data.gametype:   " << data.gametype << endl;
        cout<< "data.map:        " << data.map << endl;
        cout<< "data.numplayers: " << data.numplayers << endl;
        cout<< "data.maxplayers: " << data.maxplayers << endl;
        cout<< "data.hostport:   " << data.hostport << endl;
        cout<< "data.hostip:     " << data.hostip << endl;
    }
    else cout<< "no response from server" << endl;
}
