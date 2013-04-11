/*********************************************
 *  compiles on gcc 4.7.2 with:
 *  g++ mcquery.cpp example_main.cpp -o mcquery.run -lboost_system -pthread -lboost_thread -std=c++11
 *  
 *  run as:
 *  ./mcquery.run [host] [port] [timeoutsecs] 
 **********************************************/

#include "mcquery.hpp"

using namespace std;

int main(int argc, char *argv[]) { 
    const char* hostname = "localhost";
    const char* port = "25565";
    int timeout = 5;
    switch(argc){
        case 4:
            timeout = atoi(argv[3]);
        case 3:
            port = argv[2];
        case 2:
            hostname = argv[1];
    }
    
    mcQuery q{hostname, port, timeout}; // constructor arguments are optional, defaults are above

    cout<< "basic query: " << endl;
    auto data = q.getBasic(); 
    if(data.success) {
        cout<< "motd:       " << data.motd << endl;
        cout<< "gametype:   " << data.gametype << endl;
        cout<< "map:        " << data.map << endl;
        cout<< "numplayers: " << data.numplayers << endl;
        cout<< "maxplayers: " << data.maxplayers << endl;
        cout<< "hostport:   " << data.hostport << endl;
        cout<< "hostip:     " << data.hostip << endl;
    }
    else cout<< "no response from server" << endl;
    cout<<endl;
    
    cout<< "Full query: " << endl;
    auto fData = q.getFull();
    if(fData.success) {
        cout<< "motd:       " << fData.motd << endl;    // also accessable as data.hostname
        cout<< "gametype:   " << fData.gametype << endl;
        cout<< "game_id:    " << fData.game_id << endl;
        cout<< "version:    " << fData.version << endl;
        cout<< "plugins:    " << fData.plugins << endl; // only used by bukkit
        cout<< "map:        " << fData.map << endl;
        cout<< "numplayers: " << fData.numplayers << endl;
        cout<< "maxplayers: " << fData.maxplayers << endl;
        cout<< "hostport:   " << fData.hostport << endl;
        cout<< "hostip:     " << fData.hostip << endl;
        cout<< "playernames:" << endl;
        for( auto str : fData.playernames )
            cout<< "    " << &str[0] << endl;
    }
    else cout<< "no response from server" << endl;
    cout<<endl;

    mcQuerySimple qs{hostname, port, timeout};
    auto sData = qs.get();
    if(sData.success) {
        cout<< "motd:       " << sData.motd << endl;
        cout<< "numplayers: " << sData.numplayers << endl;
        cout<< "maxplayers: " << sData.maxplayers << endl;
        cout<< "version:    " << sData.version << endl;
    }
    else cout<< "no response from server" << endl;
    cout<<endl;

    return 0;
}
