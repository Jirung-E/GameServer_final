#include <iostream>

#include "Network.h"
#include "Session.h"
#include "NpcSession.h"

using namespace std;


static void initializeNpc() {
    cout << "NPC initialize begin.\n";
    for(id_t i = MAX_USER; i < MAX_USER + NUM_MONSTER; ++i) {
        shared_ptr<Session> npc = make_shared<NpcSession>(i);
        Session::sessions.insert(make_pair(i, npc));
        npc->state = SessionState::InGame;

        // Sector
        int idx_x = npc->character.x / SECTOR_SIZE;
        int idx_y = npc->character.y / SECTOR_SIZE;
        Session::sectors[idx_y][idx_x].borrow()->insert(npc.get());
    }
    cout << "NPC initialize end.\n";
}


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

    initializeNpc();

    server.startAccept();
    server.run();
}