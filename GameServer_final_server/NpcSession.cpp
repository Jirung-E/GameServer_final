#include "NpcSession.h"
#include "PlayerSession.h"

#include <unordered_set>

using namespace std;
using namespace NpcAI;


NpcSession::NpcSession(id_t id, int x, int y):
	Session { id },
	lua { nullptr },
	is_active { false }
{
	// Initialize
	//character.x = rand() % MAP_WIDTH;
	//character.y = rand() % MAP_HEIGHT;
    character.x = x;
    character.y = y;

	sprintf_s(character.name, "NPC%d", id);

	// Script 
	auto lua = luaL_newstate();
	luaL_openlibs(lua);
	luaL_loadfile(lua, "npc.lua");
	lua_pcall(lua, 0, 0, 0);

	lua_getglobal(lua, "set_uid");
	lua_pushnumber(lua, id);
	lua_pcall(lua, 1, 0, 0);

	lua_register(lua, "API_sendMessage", lua_sendMessage);
	lua_register(lua, "API_move", lua_move);
	lua_register(lua, "API_get_x", lua_getX);
	lua_register(lua, "API_get_y", lua_getY);

	*this->lua.borrow() = lua;
}

NpcSession::~NpcSession() {
	auto lua = this->lua.borrow();
	if(*lua) {
		lua_close(*lua);
	}
}


void NpcSession::wakeup() {
	bool not_active = false;
	if(atomic_compare_exchange_strong(&is_active, &not_active, true)) {
		postMsg(AiEvent::RunAI, 1s);
	}
}

void NpcSession::runAI() {
	// 주변 플레이어 받아오기
	unordered_set<id_t> near_players;

	{
		short min_x = (character.x - VIEW_RANGE) / SECTOR_SIZE;
		short max_x = (character.x + VIEW_RANGE) / SECTOR_SIZE;
		short min_y = (character.y - VIEW_RANGE) / SECTOR_SIZE;
		short max_y = (character.y + VIEW_RANGE) / SECTOR_SIZE;
		vector<Vault<unordered_set<Session*>>::Borrowed> near_sectors;
		near_sectors.reserve(4);
		for(short y = min_y; y <= max_y; ++y) {
			for(short x = min_x; x <= max_x; ++x) {
				if(x < 0 || y < 0 || x >= NUM_SECTOR_X || y >= NUM_SECTOR_Y) continue;
				// 미리 다 lock해놔야함
				near_sectors.push_back(sectors[y][x].borrow());
			}
		}
		for(auto& sector : near_sectors) {
			for(const auto& p : *sector) {
				id_t p_id = p->getId();
				if(p == nullptr || p->state != SessionState::InGame) continue;
				if(p->isNpc()) continue;
				if(character.canSee(p->character)) {
					near_players.insert(p_id);
				}
			}
		}
	}

	int prev_idx_x = character.x / SECTOR_SIZE;
	int prev_idx_y = character.y / SECTOR_SIZE;

	{
		// lua 스크립트 실행
		auto lua = this->lua.borrow();

		auto time = duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

		lua_getglobal(*lua, "ai_update");
		lua_pushnumber(*lua, character.x);
		lua_pushnumber(*lua, character.y);
		lua_newtable(*lua);
		int i = 1;
		for(const auto& p_id : near_players) {
			lua_pushnumber(*lua, p_id);
			lua_rawseti(*lua, -2, i++);
		}
		lua_pushnumber(*lua, static_cast<lua_Number>(time));
		lua_pcall(*lua, 4, 0, 0);
	}

	// 스크립트를 통해 이동했을수 있으니 후처리
	// Sector update
	int curr_idx_x = character.x / SECTOR_SIZE;
	int curr_idx_y = character.y / SECTOR_SIZE;
	if(prev_idx_x != curr_idx_x || prev_idx_y != curr_idx_y) {
		sectors[prev_idx_y][prev_idx_x].borrow()->erase(this);
		sectors[curr_idx_y][curr_idx_x].borrow()->insert(this);
	}

	unordered_set<id_t> near_list;

	{
		short min_x = (character.x - VIEW_RANGE) / SECTOR_SIZE;
		short max_x = (character.x + VIEW_RANGE) / SECTOR_SIZE;
		short min_y = (character.y - VIEW_RANGE) / SECTOR_SIZE;
		short max_y = (character.y + VIEW_RANGE) / SECTOR_SIZE;
		vector<Vault<unordered_set<Session*>>::Borrowed> near_sectors;
		near_sectors.reserve(4);
		for(short y = min_y; y <= max_y; ++y) {
			for(short x = min_x; x <= max_x; ++x) {
				if(x < 0 || y < 0 || x >= NUM_SECTOR_X || y >= NUM_SECTOR_Y) continue;
				// 미리 다 lock해놔야함
				near_sectors.push_back(sectors[y][x].borrow());
			}
		}
		for(auto& sector : near_sectors) {
			for(const auto& p : *sector) {
				if(p == nullptr || p->state != SessionState::InGame) continue;
				if(p->isNpc()) continue;
				if(character.canSee(p->character)) {
					near_list.insert(p->getId());
				}
			}
		}
	}

	for(auto pl : near_list) {
		shared_ptr<Session> cl = sessions.at(pl);
		auto pc = reinterpret_cast<PlayerSession*>(cl.get());
		if(0 == near_players.count(pl)) {
			// 플레이어의 시야에 등장
			pc->sendAddPlayerPacket(getId());
		}
		else {
			// 플레이어가 계속 보고 있음.
			pc->sendMovePacket(getId());
		}
	}

	for(auto pl : near_players) {
		if(0 == near_list.count(pl)) {
			shared_ptr<Session> cl = sessions.at(pl);
			auto pc = reinterpret_cast<PlayerSession*>(cl.get());
			auto vl = pc->view_list.borrow();
			if(0 != vl->count(getId())) {
				vl.release();
				pc->sendRemovePlayerPacket(getId());
			}
		}
	}

	if(near_list.empty()) {
		is_active = false;
	}
	else {
		postMsg(AiEvent::RunAI, 1s);
	}
}

void NpcSession::event_characterMove(id_t character_id) {
	auto lua = this->lua.borrow();

	auto time = duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

	lua_getglobal(*lua, "event_player_move");
	lua_pushnumber(*lua, character.x);
	lua_pushnumber(*lua, character.y);
	lua_pushnumber(*lua, character_id);
	lua_pushnumber(*lua, static_cast<lua_Number>(time));
	lua_pcall(*lua, 4, 0, 0);
}
