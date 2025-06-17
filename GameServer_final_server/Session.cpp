#include "Session.h"

#include "Network.h"

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


void Session::tpTo(short x, short y) {
    if(x < 0) x = 0;
    if(x >= MAP_WIDTH) x = MAP_WIDTH - 1;
    if(y < 0) y = 0;
    if(y >= MAP_HEIGHT) y = MAP_HEIGHT - 1;

    character.x = x;
    character.y = y;

    // 기존 섹터에서 제거
    int prev_idx_x = character.x / SECTOR_SIZE;
    int prev_idx_y = character.y / SECTOR_SIZE;
    sectors[prev_idx_y][prev_idx_x].borrow()->erase(this);

    // 새로운 섹터에 추가
    int idx_x = character.x / SECTOR_SIZE;
    int idx_y = character.y / SECTOR_SIZE;
    sectors[idx_y][idx_x].borrow()->insert(this);
}

void Session::damage(short amount) {
    character.hp -= amount;
    if(character.hp < 0) {
        character.hp = 0;
        revive();
    }
}
