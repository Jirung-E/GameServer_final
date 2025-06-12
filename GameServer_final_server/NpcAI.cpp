#include "NpcAI.h"

#include <thread>

#include "OverlappedEx.h"
#include "Session.h"
#include "PlayerSession.h"

using namespace std;


namespace NpcAI {
	concurrency::concurrent_priority_queue<AiEventParameters> ai_event_queue;
}


int NpcAI::lua_getX(lua_State* L) {
	id_t user_id = (id_t)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = Session::sessions.at(user_id).load()->character.x;
	lua_pushnumber(L, x);
	return 1;
}

int NpcAI::lua_getY(lua_State* L) {
	id_t user_id = (id_t)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = Session::sessions.at(user_id).load()->character.y;
	lua_pushnumber(L, y);
	return 1;
}

int NpcAI::lua_move(lua_State* L) {
	// param load
	id_t npc_id = (id_t)lua_tointeger(L, -3);
	short dx = (short)lua_tointeger(L, -2);
	short dy = (short)lua_tointeger(L, -1);

	// pop params
	lua_pop(L, 4);

	// execute
	//do_npc_random_move(npc_id);
	shared_ptr<Session> npc = Session::sessions.at(npc_id);
	if(npc != nullptr && npc->state == SessionState::InGame) {
		Character& character = npc->character;
		short x = character.x + dx;
		short y = character.y + dy;

		if(x < 0) x = 0;
		if(x >= MAP_WIDTH) x = MAP_WIDTH - 1;
		if(y < 0) y = 0;
		if(y >= MAP_HEIGHT) y = MAP_HEIGHT - 1;

		character.x = x;
		character.y = y;
	}

	// push return value
	// ..

	return 0;
}

int NpcAI::lua_sendMessage(lua_State* L) {
	id_t my_id = (int)lua_tointeger(L, -3);
	id_t user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 4);

	shared_ptr<Session> session = Session::sessions.at(user_id);
	PlayerSession* client = reinterpret_cast<PlayerSession*>(session.get());
	client->sendChatPacket(my_id, mess);
	return 0;
}


void NpcAI::npcAiLoop(HANDLE h_iocp) {
	while(true) {
		while(true) {
			AiEventParameters event;
			if(ai_event_queue.try_pop(event)) {
				if(event.wakeup_time > chrono::high_resolution_clock::now()) {
					ai_event_queue.push(event);
					break;
				}

				switch(event.event_id) {
					case AiEvent::Move: {
						OverlappedEx* o = new OverlappedEx { IoOperation::NpcMove };
						PostQueuedCompletionStatus(h_iocp, NULL, event.obj_id, o->getOverlapped());
						break;
					}
					case AiEvent::RunAI: {
						OverlappedEx* o = new OverlappedEx { IoOperation::NpcAI };
						PostQueuedCompletionStatus(h_iocp, NULL, event.obj_id, o->getOverlapped());
						break;
					}
				}
			}
		}

		this_thread::sleep_for(10ms);
	}
}
