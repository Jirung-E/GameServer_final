#include "Session.h"

using namespace std;


concurrency::concurrent_unordered_map<
    id_t, std::atomic<std::shared_ptr<Session>>
> Session::sessions;

std::array<std::array<
    Vault<std::unordered_set<Session*>>,
    NUM_SECTOR_X>, NUM_SECTOR_Y
> Session::sectors;


Session::Session(id_t id):
    id { id },
    state { SessionState::Alloc },
    character { }
{

}

Session::~Session() {

}
