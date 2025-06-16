#include <iostream>

#include "Network.h"
#include "Session.h"
#include "NpcSession.h"

using namespace std;


int main() {
    wcout.imbue(locale("korean"));
    setlocale(LC_ALL, "korean");

    WsaGuard wsaGuard;

    Server server;

    if(!server.bindSocket()) {
        return 1; // Binding failed
    }
    if(!server.startListen()) {
        return 1; // Listening failed
    }

    server.initializeNpc();

    server.startAccept();
    server.run();
}