#include <iostream>
#include <type_traits>
#include <format>
#include <chrono>
#include <thread>
#include <atomic>
#include <array>
#include <vector>
#include <queue>
#include <unordered_set>
#include <concurrent_unordered_map.h>
#include <concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include <locale.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#include <windows.h>

#define UNICODE
#include <sqlext.h>

#include "include/lua.hpp"

#include "game_header.h"
#include "NetUtil.h"
#include "Vault.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "lua54.lib")

using namespace std;


using id_t = int;

template<typename T>
concept ChronoDuration = std::is_base_of_v<std::chrono::duration<typename T::rep, typename T::period>, T>;


constexpr int VIEW_RANGE = 5;
constexpr int SECTOR_SIZE = 12;

constexpr static int ceil_div(int a, int b) {
	return (a + b - 1) / b;
}

constexpr int NUM_SECTOR_X = ceil_div(MAP_WIDTH, SECTOR_SIZE);
constexpr int NUM_SECTOR_Y = ceil_div(MAP_HEIGHT, SECTOR_SIZE);

constexpr int BUF_SIZE = 1024 * 8; // 8KB


enum COMP_TYPE {
	OP_ACCEPT, OP_RECV, OP_SEND, OP_LOGIN_OK, OP_LOGIN_FAIL, OP_NPC_MOVE, OP_NPC_AI
};

enum S_STATE {
	ST_ALLOC, ST_INGAME, ST_FREE,
};

enum DB_REQUEST {
	DB_LOAD, DB_STORE,
};

struct DbRequest {
	DB_REQUEST request_type;
	id_t obj_id;
	char name[MAX_ID_LENGTH];
	short x;
	short y;
};

concurrency::concurrent_queue<DbRequest> db_request_queue;

enum EVENT_TYPE {
	EV_RUN_AI, EV_MOVE, EV_HEAL, EV_ATTACK,
};

struct event_type {
	id_t obj_id;
	chrono::high_resolution_clock::time_point wakeup_time;
	EVENT_TYPE event_id;
	id_t target_id;

	constexpr bool operator<(const event_type& _Left) const {
		return (wakeup_time > _Left.wakeup_time);
	}
};

concurrency::concurrent_priority_queue<event_type> timer_queue;

HANDLE h_iocp;


class OVER_EXP {
public:
	WSAOVERLAPPED _over;
	SOCKET _accept_socket;
	WSABUF _wsabuf;
	char _send_buf[BUF_SIZE];
	COMP_TYPE _comp_type;
	id_t _ai_target_obj;

public:
	OVER_EXP(COMP_TYPE op):
		_comp_type { op },
		_accept_socket { 0 },
		_ai_target_obj { -1 },
		_send_buf { }
	{
		ZeroMemory(&_over, sizeof(_over));
		_wsabuf.buf = _send_buf;
		_wsabuf.len = BUF_SIZE;
	}
};


class SESSION {
	OVER_EXP _recv_over;

public:
	atomic<S_STATE> _state;
	id_t _id;
	SOCKET _socket;
	short	x, y;
	char	_name[MAX_ID_LENGTH];
	int		_prev_remain;
	Vault<unordered_set<id_t>> _view_list;
	size_t		_last_move_time;

	// NPC
	atomic_bool _is_active;

	Vault<lua_State*> _lua;

public:
	SESSION():
		_recv_over { OP_RECV },
		_state { ST_ALLOC },
		_id { -1 },
		_socket { 0 },
		x { 0 },
		y { 0 },
		_prev_remain { 0 },
		_last_move_time { 0 },
		_is_active { false },
		_view_list { },
		_lua { nullptr }
	{
		_name[0] = 0;
	}

	~SESSION() { }

public:
	void process_packet(char* packet);

	void do_recv() {
		DWORD recv_flag = 0;
		memset(&_recv_over._over, 0, sizeof(_recv_over._over));
		_recv_over._wsabuf.len = BUF_SIZE - _prev_remain;
		_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_remain;

		WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag,
			&_recv_over._over, 0);
	}

	void do_send(void* packet) const {
		OVER_EXP* sdata = new OVER_EXP { OP_SEND };

		CHAR* buf = reinterpret_cast<CHAR*>(packet);
		const unsigned char packet_size = static_cast<unsigned char>(buf[0]);
		memcpy(sdata->_send_buf, buf, packet_size);
		sdata->_wsabuf.len = packet_size;

		int ret = WSASend(_socket, &sdata->_wsabuf, 1, NULL, NULL, &sdata->_over, NULL);
		if(ret != 0) {
			auto err_no = WSAGetLastError();
			if(err_no != WSA_IO_PENDING) {
				// WSAECONNRESET(10054)
				//printf("%d[packet: %d]: Send Error - %d\n", _id, (int)((char*)packet)[1], err_no);
				delete sdata;
			}
		}
	}

	void send_login_info_packet() const {
		sc_packet_avatar_info p { };
		p.id = _id;
		p.size = sizeof(p);
		p.type = S2C_P_AVATAR_INFO;
		p.x = x;
		p.y = y;
		do_send(&p);
	}

	void send_move_packet(id_t c_id) const;
	void send_add_player_packet(id_t c_id);

	void send_remove_player_packet(id_t c_id) {
		{
			auto vl = _view_list.borrow();
			if(vl->count(c_id)) {
				vl->erase(c_id);
			}
			else {
				return;
			}
		}
		sc_packet_leave p { };
		p.size = sizeof(p);
		p.type = S2C_P_LEAVE;
		p.id = c_id;
		do_send(&p);
	}

	void wakeup() {
		bool not_active = false;
		if(atomic_compare_exchange_strong(&_is_active, &not_active, true)) {
			//post_move_msg(1s);
			//post_run_ai_msg(1s);

			//post_msg(EV_MOVE, 0s, waker);

			post_msg(EV_RUN_AI, 1s);
		}
	}

	template<ChronoDuration T>
	void post_msg(EVENT_TYPE event, T delay, id_t target_id = -1) {
		timer_queue.push(event_type {
			_id,
			chrono::high_resolution_clock::now() + delay,
			event,
			target_id
			});
	}

	void run_ai();

	void event_other_player_move(id_t other_id) {
		auto lua = _lua.borrow();

		auto time = duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

		lua_getglobal(*lua, "event_player_move");
		lua_pushnumber(*lua, x);
		lua_pushnumber(*lua, y);
		lua_pushnumber(*lua, other_id);
		lua_pushnumber(*lua, time);
		lua_pcall(*lua, 4, 0, 0);
		//lua_pop(lua, 1);
	}

	//void run_ai(const unordered_set<id_t>& ids) {
	//	auto lua = _lua.borrow();

	//	auto time = duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

	//	for(const auto& waker : ids) {
	//		lua_getglobal(*lua, "event_player_move");
	//		lua_pushnumber(*lua, waker);
	//		lua_pushnumber(*lua, time);
	//		lua_pcall(*lua, 2, 0, 0);
	//	}
	//}

	//template<ChronoDuration T>
	//void post_move_msg(T delay) {
	//	timer_queue.push(event_type {
	//		_id,
	//		chrono::high_resolution_clock::now() + delay,
	//		EV_MOVE,
	//		0
	//		});
	//}

	void send_chat_packet(id_t p_id, const char* mess) const {
		sc_packet_chat packet { };
		packet.id = p_id;
		packet.size = sizeof(packet);
		packet.type = S2C_P_CHAT;
		strcpy_s(packet.message, mess);
		do_send(&packet);
	}

	void login();
};


concurrency::concurrent_unordered_map<
	id_t, atomic<shared_ptr<SESSION>>
> clients;

array<array<
	Vault<unordered_set<SESSION*>>,
	NUM_SECTOR_X>, NUM_SECTOR_Y
> sectors;

atomic<id_t> g_new_id;


static bool is_pc(int object_id) {
	return object_id < MAX_USER;
}

static bool is_npc(int object_id) {
	return !is_pc(object_id);
}


static bool can_see(id_t from, id_t to) {
	shared_ptr<SESSION> client_from = clients.at(from);
	shared_ptr<SESSION> client_to = clients.at(to);
	if(abs(client_from->x - client_to->x) > VIEW_RANGE) return false;
	return abs(client_from->y - client_to->y) <= VIEW_RANGE;
}


void SESSION::login() {
	//x = rand() % MAP_WIDTH;
		//y = rand() % MAP_HEIGHT;
	send_login_info_packet();
	_state = ST_INGAME;

	{
		int idx_x = x / SECTOR_SIZE;
		int idx_y = y / SECTOR_SIZE;
		sectors[idx_y][idx_x].borrow()->insert(this);
	}

	short min_x = (x - VIEW_RANGE) / SECTOR_SIZE;
	short max_x = (x + VIEW_RANGE) / SECTOR_SIZE;
	short min_y = (y - VIEW_RANGE) / SECTOR_SIZE;
	short max_y = (y + VIEW_RANGE) / SECTOR_SIZE;
	vector<Vault<unordered_set<SESSION*>>::Borrowed> near_sectors;
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
			if(p == nullptr || ST_INGAME != p->_state || p->_id == _id) continue;
			if(can_see(_id, p->_id)) {
				if(is_pc(p->_id)) {
					p->send_add_player_packet(_id);
				}
				else {
					p->wakeup();
					p->event_other_player_move(_id);
				}
				send_add_player_packet(p->_id);
			}
		}
	}
}


void SESSION::send_move_packet(id_t c_id) const {
	shared_ptr<SESSION> client = clients.at(c_id);
	sc_packet_move p { };
	p.size = sizeof(p);
	p.type = S2C_P_MOVE;
	p.id = c_id;
	p.x = client->x;
	p.y = client->y;
	//p.move_time = client->_last_move_time;
	do_send(&p);
}

void SESSION::send_add_player_packet(id_t c_id) {
	shared_ptr<SESSION> client = clients.at(c_id);
	sc_packet_enter add_packet { };
	add_packet.size = sizeof(add_packet);
	add_packet.type = S2C_P_ENTER;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, client->_name);
	add_packet.x = client->x;
	add_packet.y = client->y;
	{
		_view_list.borrow()->insert(c_id);
	}
	do_send(&add_packet);
}

void SESSION::process_packet(char* packet) {
	if(_state == ST_FREE) {
		cout << "Client not found\n";
		return;
	}

	switch(packet[1]) {
		case C2S_P_LOGIN: {
			cs_packet_login* p = reinterpret_cast<cs_packet_login*>(packet);

			//strcpy_s(_name, p->name);

			DbRequest req { DB_LOAD, _id, { } };
			strcpy_s(req.name, p->name);
			db_request_queue.push(req);

			break;
		}
		case C2S_P_MOVE: {
			cs_packet_move* p = reinterpret_cast<cs_packet_move*>(packet);
			//_last_move_time = p->move_time;
			short x = this->x;
			short y = this->y;

			int prev_idx_x = x / SECTOR_SIZE;
			int prev_idx_y = y / SECTOR_SIZE;

			switch(p->direction) {
				case 0:
					if(y > 0) y--;
					break;
				case 1:
					if(y < MAP_HEIGHT - 1) y++;
					break;
				case 2:
					if(x > 0) x--;
					break;
				case 3:
					if(x < MAP_WIDTH - 1) x++;
					break;
			}
			this->x = x;
			this->y = y;

			int curr_idx_x = x / SECTOR_SIZE;
			int curr_idx_y = y / SECTOR_SIZE;
			if(prev_idx_x != curr_idx_x || prev_idx_y != curr_idx_y) {
				sectors[prev_idx_y][prev_idx_x].borrow()->erase(this);
				sectors[curr_idx_y][curr_idx_x].borrow()->insert(this);
			}

			unordered_set<id_t> near_list;
			auto client_vl = _view_list.borrow();
			unordered_set<id_t> old_vlist = *client_vl;
			client_vl.release();

			short min_x = (x - VIEW_RANGE) / SECTOR_SIZE;
			short max_x = (x + VIEW_RANGE) / SECTOR_SIZE;
			short min_y = (y - VIEW_RANGE) / SECTOR_SIZE;
			short max_y = (y + VIEW_RANGE) / SECTOR_SIZE;
			vector<Vault<unordered_set<SESSION*>>::Borrowed> near_sectors;
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
					if(p == nullptr || p->_state != ST_INGAME) continue;
					if(can_see(_id, p->_id)) {
						near_list.insert(p->_id);
					}
				}
			}

			send_move_packet(_id);

			for(auto& pl : near_list) {
				shared_ptr<SESSION> cpl = clients.at(pl);
				if(is_pc(pl)) {
					auto cpl_vl = cpl->_view_list.borrow();
					if(cpl_vl->count(_id)) {
						cpl_vl.release();
						cpl->send_move_packet(_id);
					}
					else {
						cpl_vl.release();
						cpl->send_add_player_packet(_id);
					}
				}
				else {
					cpl->wakeup();
					cpl->event_other_player_move(_id);
				}

				if(old_vlist.count(pl) == 0) {
					send_add_player_packet(pl);
				}
			}

			for(auto& pl : old_vlist) {
				if(0 == near_list.count(pl)) {
					send_remove_player_packet(pl);
					if(is_pc(pl)) {
						clients.at(pl).load()->send_remove_player_packet(_id);
					}
				}
			}

			break;
		}
	}
}

void SESSION::run_ai() {
	// 주변 플레이어 받아오기
	unordered_set<id_t> near_players;

	{
		short min_x = (x - VIEW_RANGE) / SECTOR_SIZE;
		short max_x = (x + VIEW_RANGE) / SECTOR_SIZE;
		short min_y = (y - VIEW_RANGE) / SECTOR_SIZE;
		short max_y = (y + VIEW_RANGE) / SECTOR_SIZE;
		vector<Vault<unordered_set<SESSION*>>::Borrowed> near_sectors;
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
				id_t p_id = p->_id;
				if(p == nullptr || p->_state != ST_INGAME) continue;
				if(is_npc(p_id)) continue;
				if(can_see(_id, p_id)) {
					near_players.insert(p_id);
				}
			}
		}
	}

	int prev_idx_x = x / SECTOR_SIZE;
	int prev_idx_y = y / SECTOR_SIZE;

	{
		// lua 스크립트 실행
		auto lua = _lua.borrow();

		auto time = duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

		lua_getglobal(*lua, "ai_update");
		lua_pushnumber(*lua, x);
		lua_pushnumber(*lua, y);
		lua_newtable(*lua);
		int i = 1;
		for(const auto& p_id : near_players) {
			lua_pushnumber(*lua, p_id);
			lua_rawseti(*lua, -2, i++);
		}
		lua_pushnumber(*lua, time);
		lua_pcall(*lua, 4, 0, 0);
	}

	// 스크립트를 통해 이동했을수 있으니 후처리
	// Sector update
	int curr_idx_x = x / SECTOR_SIZE;
	int curr_idx_y = y / SECTOR_SIZE;
	if(prev_idx_x != curr_idx_x || prev_idx_y != curr_idx_y) {
		sectors[prev_idx_y][prev_idx_x].borrow()->erase(this);
		sectors[curr_idx_y][curr_idx_x].borrow()->insert(this);
	}

	unordered_set<id_t> near_list;

	{
		short min_x = (x - VIEW_RANGE) / SECTOR_SIZE;
		short max_x = (x + VIEW_RANGE) / SECTOR_SIZE;
		short min_y = (y - VIEW_RANGE) / SECTOR_SIZE;
		short max_y = (y + VIEW_RANGE) / SECTOR_SIZE;
		vector<Vault<unordered_set<SESSION*>>::Borrowed> near_sectors;
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
				if(p == nullptr || p->_state != ST_INGAME) continue;
				if(is_npc(p->_id)) continue;
				if(can_see(_id, p->_id)) {
					near_list.insert(p->_id);
				}
			}
		}
	}

	for(auto pl : near_list) {
		shared_ptr<SESSION> cl = clients.at(pl);
		if(0 == near_players.count(pl)) {
			// 플레이어의 시야에 등장
			cl->send_add_player_packet(_id);
		}
		else {
			// 플레이어가 계속 보고 있음.
			cl->send_move_packet(_id);
		}
	}

	for(auto pl : near_players) {
		if(0 == near_list.count(pl)) {
			shared_ptr<SESSION> cl = clients.at(pl);
			auto vl = cl->_view_list.borrow();
			if(0 != vl->count(_id)) {
				vl.release();
				cl->send_remove_player_packet(_id);
			}
		}
	}

	if(near_list.empty()) {
		_is_active = false;
	}
	else {
		//post_run_ai_msg(1s);
		post_msg(EV_RUN_AI, 1s);
	}
}

static void disconnect(id_t c_id) {
	shared_ptr<SESSION> client = clients.at(c_id);
	auto vl_borrowed = client->_view_list.borrow();
	unordered_set<id_t> vl = *vl_borrowed;
	vl_borrowed.release();

	for(auto& p_id : vl) {
		if(is_npc(p_id)) continue;
		shared_ptr<SESSION> p = clients.at(p_id);
		if(p == nullptr || ST_INGAME != p->_state || p_id == c_id) continue;
		p->send_remove_player_packet(c_id);
	}

	{
		int idx_x = client->x / SECTOR_SIZE;
		int idx_y = client->y / SECTOR_SIZE;
		sectors[idx_y][idx_x].borrow()->erase(client.get());
	}

	DbRequest db_req { DB_STORE, c_id, { }, client->x, client->y };
	strcpy_s(db_req.name, client->_name);
	db_request_queue.push(db_req);

	closesocket(client->_socket);

	client->_state = ST_FREE;
}


static void do_accept(SOCKET server_socket, OVER_EXP* accept_over) {
	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	accept_over->_accept_socket = client_socket;
	AcceptEx(server_socket, client_socket, accept_over->_send_buf, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &accept_over->_over);
}


static shared_ptr<SESSION> new_client() {
	for(auto& cl : clients) {
		shared_ptr<SESSION> p = cl.second;
		if(p->_state == ST_FREE) {
			p->_state = ST_ALLOC;
			return cl.second;
		}
	}

	if(g_new_id >= MAX_USER) {
		return nullptr;
	}

	shared_ptr<SESSION> nc = std::make_shared<SESSION>();
	nc->_state = ST_ALLOC;
	nc->_id = g_new_id++;
	clients.insert(make_pair(nc->_id, nc));
	return nc;
}


static void worker_thread(SOCKET server_socket) {
	while(true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);
		if(FALSE == ret) {
			if(ex_over->_comp_type == OP_ACCEPT) {
				cout << "Accept Error";
			}
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<id_t>(key));
				if(ex_over->_comp_type == OP_SEND) delete ex_over;
				continue;
			}
		}

		if((0 == num_bytes) && ((ex_over->_comp_type == OP_RECV) || (ex_over->_comp_type == OP_SEND))) {
			disconnect(static_cast<id_t>(key));
			if(ex_over->_comp_type == OP_SEND) {
				delete ex_over;
			}
			continue;
		}

		switch(ex_over->_comp_type) {
			case OP_ACCEPT: {
				shared_ptr<SESSION> client = new_client();
				if(client != nullptr) {
					client->x = 0;
					client->y = 0;
					client->_name[0] = 0;
					client->_prev_remain = 0;
					client->_socket = ex_over->_accept_socket;
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(ex_over->_accept_socket),
						h_iocp, client->_id, 0);
					client->do_recv();
					do_accept(server_socket, ex_over);
				}
				else {
					cout << "Max user exceeded.\n";
				}
				break;
			}
			case OP_RECV: {
				shared_ptr<SESSION> client = clients.at(static_cast<id_t>(key));
				if(client == nullptr) {
					cout << "Client not found\n";
					break;
				}
				int remain_data = num_bytes + client->_prev_remain;
				char* p = ex_over->_send_buf;
				while(remain_data > 0) {
					int packet_size = p[0];
					if(packet_size <= remain_data) {
						client->process_packet(p);
						p = p + packet_size;
						remain_data = remain_data - packet_size;
					}
					else break;
				}
				client->_prev_remain = remain_data;
				if(remain_data > 0) {
					memcpy(ex_over->_send_buf, p, remain_data);
				}
				client->do_recv();
				break;
			}
			case OP_SEND: {
				delete ex_over;
				break;
			}
			case OP_LOGIN_OK: {
				shared_ptr<SESSION> client = clients.at(static_cast<id_t>(key));

				//SC_LOGIN_OK_PACKET lfp { };
				//lfp.size = sizeof(SC_LOGIN_OK_PACKET);
				//lfp.type = SC_LOGIN_OK;
				//client->do_send(&lfp);

				client->login();

				delete ex_over;

				break;
			}
			case OP_LOGIN_FAIL: {
				shared_ptr<SESSION> client = clients.at(static_cast<id_t>(key));

				client->_state = ST_FREE;
				sc_packet_login_fail lfp { };
				lfp.size = sizeof(lfp);
				lfp.type = S2C_P_LOGIN_FAIL;
				client->do_send(&lfp);

				delete ex_over;

				break;
			}
			case OP_NPC_MOVE: {
				//do_npc_random_move(static_cast<id_t>(key));
				shared_ptr<SESSION> npc = clients.at(static_cast<id_t>(key));
				npc->event_other_player_move(ex_over->_ai_target_obj);
				delete ex_over;
				break;
			}
			case OP_NPC_AI: {
				shared_ptr<SESSION> npc = clients.at(static_cast<id_t>(key));
				npc->run_ai();
				delete ex_over;
				break;
			}
		}
	}
}


static int lua_get_x(lua_State* L) {
	id_t user_id = (id_t)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x = clients.at(user_id).load()->x;
	lua_pushnumber(L, x);
	return 1;
}

static int lua_get_y(lua_State* L) {
	id_t user_id = (id_t)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y = clients.at(user_id).load()->y;
	lua_pushnumber(L, y);
	return 1;
}

static int lua_move(lua_State* L) {
	// param load
	id_t npc_id = (id_t)lua_tointeger(L, -3);
	short dx = (short)lua_tointeger(L, -2);
	short dy = (short)lua_tointeger(L, -1);

	// pop params
	lua_pop(L, 4);

	// execute
	//do_npc_random_move(npc_id);
	shared_ptr<SESSION> npc = clients.at(npc_id);
	if(npc != nullptr && npc->_state == ST_INGAME) {
		short x = npc->x + dx;
		short y = npc->y + dy;

		if(x < 0) x = 0;
		if(x >= MAP_WIDTH) x = MAP_WIDTH - 1;
		if(y < 0) y = 0;
		if(y >= MAP_HEIGHT) y = MAP_HEIGHT - 1;

		npc->x = x;
		npc->y = y;
	}

	// push return value
	// ..

	return 0;
}

static int lua_sendMessage(lua_State* L) {
	id_t my_id = (int)lua_tointeger(L, -3);
	id_t user_id = (int)lua_tointeger(L, -2);
	char* mess = (char*)lua_tostring(L, -1);

	lua_pop(L, 4);

	clients.at(user_id).load()->send_chat_packet(my_id, mess);
	return 0;
}


static void InitializeNPC() {
	cout << "NPC initialize begin.\n";
	for(id_t i = MAX_USER; i < MAX_USER + NUM_MONSTER; ++i) {
		shared_ptr<SESSION> npc = make_shared<SESSION>();
		clients.insert(make_pair(i, npc));

		// Initialize
		npc->x = rand() % MAP_WIDTH;
		npc->y = rand() % MAP_HEIGHT;
		npc->_id = i;
		sprintf_s(npc->_name, "NPC%d", i);
		npc->_state = ST_INGAME;

		// Sector
		int idx_x = npc->x / SECTOR_SIZE;
		int idx_y = npc->y / SECTOR_SIZE;
		sectors[idx_y][idx_x].borrow()->insert(npc.get());

		// Script 
		auto lua = luaL_newstate();
		luaL_openlibs(lua);
		luaL_loadfile(lua, "npc.lua");
		lua_pcall(lua, 0, 0, 0);

		lua_getglobal(lua, "set_uid");
		lua_pushnumber(lua, i);
		lua_pcall(lua, 1, 0, 0);
		// lua_pop(lua, 1);// eliminate set_uid from stack after call

		lua_register(lua, "API_sendMessage", lua_sendMessage);
		lua_register(lua, "API_move", lua_move);
		lua_register(lua, "API_get_x", lua_get_x);
		lua_register(lua, "API_get_y", lua_get_y);

		*npc->_lua.borrow() = lua;
	}
	cout << "NPC initialize end.\n";
}


static void do_timer() {
	while(true) {
		while(true) {
			//if(timer_queue.empty()) {
			//	break;
			//}

			event_type event;
			if(timer_queue.try_pop(event)) {
				if(event.wakeup_time > chrono::high_resolution_clock::now()) {
					timer_queue.push(event);
					break;
				}

				switch(event.event_id) {
					case EV_MOVE: {
						OVER_EXP* o = new OVER_EXP { OP_NPC_MOVE };
						PostQueuedCompletionStatus(h_iocp, NULL, event.obj_id, &o->_over);
						break;
					}
					case EV_RUN_AI: {
						OVER_EXP* o = new OVER_EXP { OP_NPC_AI };
						PostQueuedCompletionStatus(h_iocp, NULL, event.obj_id, &o->_over);
						break;
					}
				}
			}
		}
		this_thread::sleep_for(10ms);
	}
}


SQLHENV henv;
SQLHDBC hdbc;
SQLHSTMT hstmt = 0;
SQLRETURN retcode;
SQLWCHAR user_name[50];
SQLINTEGER user_id, user_level, user_x, user_y;
SQLLEN cb_user_name = 0, cb_user_id = 0, cb_user_level = 0, cb_x = 0, cb_y = 0;

void show_error() {
	printf("error\n");
}

/************************************************************************
/* HandleDiagnosticRecord : display error/warning information
/*
/* Parameters:
/* hHandle ODBC handle
/* hType Type of handle (SQL_HANDLE_STMT, SQL_HANDLE_ENV, SQL_HANDLE_DBC)
/* RetCode Return code of failing command
/************************************************************************/
void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode) {
	SQLSMALLINT iRec = 0;
	SQLINTEGER iError;
	WCHAR wszMessage[1000];
	WCHAR wszState[SQL_SQLSTATE_SIZE+1];
	if(RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while(SQLGetDiagRec(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT*)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if(wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}

static bool db_load(char name[MAX_ID_LENGTH], SESSION* client) {
	bool success = false;

	size_t name_len = strlen(name);
	std::wstring name_wstr { name, name + name_len };

	//  wstring query = format(L"SELECT user_id, user_name, user_level, x, y FROM user_data WHERE user_name = \'{:{}}\'", 
		  //name_wstr, MAX_ID_LENGTH);
	wstring query = std::format(L"EXEC load_data \'{:{}}\'",
		name_wstr, static_cast<int>(MAX_ID_LENGTH));

	retcode = SQLExecDirect(hstmt, query.data(), SQL_NTS);
	if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLFetch(hstmt);
		if(retcode == SQL_ERROR/* || retcode == SQL_SUCCESS_WITH_INFO*/)
			show_error();
		if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			//printf("%d: %d %ls %d\n", i + 1, user_id, user_name, user_level);
			client->x = user_x;
			client->y = user_y;
			strcpy_s(client->_name, MAX_ID_LENGTH, name);
			success = true;
			//break;
		}
	}
	else {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
	}

	SQLCancel(hstmt);

	return success;
}


static bool db_store(char name[MAX_ID_LENGTH], short x, short y) {
	size_t name_len = strlen(name);
	std::wstring name_wstr { name, name + name_len };

	//wstring query = format(L"UPDATE user_data SET x = {}, y = {} WHERE user_name = \'{:{}}\'", 
	//	x, y, name_wstr, MAX_ID_LENGTH);
	wstring query = format(L"EXEC store_data \'{:{}}\', {}, {}",
		name_wstr, static_cast<int>(MAX_ID_LENGTH), x, y);

	retcode = SQLExecDirect(hstmt, query.data(), SQL_NTS);
	if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		return true;
	}
	else {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
		return false;
	}
}


static void db_loop() {
	while(true) {
		while(true) {
			DbRequest request;
			if(db_request_queue.try_pop(request)) {
				switch(request.request_type) {
					case DB_LOAD: {
						shared_ptr<SESSION> client = clients.at(request.obj_id);
						bool success = db_load(request.name, client.get());

						COMP_TYPE op = OP_LOGIN_FAIL;
						if(success) {
							op = OP_LOGIN_OK;
						}
						OVER_EXP* o = new OVER_EXP { op };
						PostQueuedCompletionStatus(h_iocp, NULL, request.obj_id, &o->_over);

						break;
					}
					case DB_STORE: {
						bool success = db_store(request.name, request.x, request.y);
						break;
					}
				}
			}
		}
		//this_thread::sleep_for(10ms);
	}
}


static void db_run() {
	setlocale(LC_ALL, "korean");

	// Allocate environment handle  
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);

	// Set the ODBC version environment attribute  
	if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER*)SQL_OV_ODBC3, 0);

		// Allocate connection handle  
		if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			retcode = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);

			// Set login timeout to 5 seconds  
			if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				SQLSetConnectAttr(hdbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

				// Connect to data source  
				retcode = SQLConnect(hdbc, (SQLWCHAR*)L"game_server_hw_2020180028", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

				// Allocate statement handle  
				if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					retcode = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);

					// Bind columns 1, 2, and 3  
					retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &user_id, 100, &cb_user_id);
					retcode = SQLBindCol(hstmt, 2, SQL_C_WCHAR, user_name, MAX_ID_LENGTH, &cb_user_name);
					retcode = SQLBindCol(hstmt, 3, SQL_C_LONG, &user_level, 100, &cb_user_level);
					retcode = SQLBindCol(hstmt, 4, SQL_C_LONG, &user_x, 100, &cb_x);
					retcode = SQLBindCol(hstmt, 5, SQL_C_LONG, &user_y, 100, &cb_y);

					db_loop();

					// Process data  
					if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
						SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
					}

					SQLDisconnect(hdbc);
				}

				SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
			}
		}
		SQLFreeHandle(SQL_HANDLE_ENV, henv);
	}
}


int main() {
	wcout.imbue(locale("korean"));

	WSADATA WSAData;
	WSAStartup(MAKEWORD(2, 2), &WSAData);

	SOCKET server_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(server_socket <= 0) {
		auto err_no = WSAGetLastError();
		print_error_message(err_no);
		return -1;
	}

	SOCKADDR_IN server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(GAME_PORT);

	int bind_res = bind(server_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
	if(bind_res == SOCKET_ERROR) {
		cout << "Bind Error\n";
		auto err_no = WSAGetLastError();
		print_error_message(err_no);
		return 0;
	}

	int listen_res = listen(server_socket, SOMAXCONN);
	if(listen_res == SOCKET_ERROR) {
		cout << "Listen Error\n";
		auto err_no = WSAGetLastError();
		print_error_message(err_no);
		return 0;
	}

	int addr_size = sizeof(SOCKADDR_IN);

	InitializeNPC();

	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(server_socket), h_iocp, -1, 0);

	OVER_EXP accept_over { OP_ACCEPT };
	do_accept(server_socket, &accept_over);

	vector<thread> worker_threads;
	int num_threads = std::thread::hardware_concurrency() - 2;
	for(int i = 0; i < num_threads; ++i) {
		worker_threads.emplace_back(worker_thread, server_socket);
	}
	thread ai_thread { do_timer };
	thread db_thread { db_run };

	db_thread.join();
	ai_thread.join();
	for(auto& th : worker_threads) {
		th.join();
	}

	closesocket(server_socket);

	WSACleanup();
}
