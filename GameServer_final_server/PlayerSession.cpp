#include "PlayerSession.h"
#include "NpcSession.h"

#include <iostream>

#include "Network.h"
#include "DbConnection.h"

using namespace std;


PlayerSession::PlayerSession(id_t id, SOCKET socket):
	Session { id },
	recv_over { IoOperation::Receive },
	socket { socket },
	view_list { },
	packet_parser { }
{

}


void PlayerSession::login() {
	//x = rand() % MAP_WIDTH;
	//y = rand() % MAP_HEIGHT;
	sendLoginInfoPacket();
	state = SessionState::InGame;

	{
		int idx_x = character.x / SECTOR_SIZE;
		int idx_y = character.y / SECTOR_SIZE;
		sectors[idx_y][idx_x].borrow()->insert(this);
	}

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
			if(p == nullptr || SessionState::InGame != p->state || p->getId() == getId()) continue;
			if(character.canSee(p->character)) {
				if(p->isPc()) {
					auto pc = reinterpret_cast<PlayerSession*>(p);
					pc->sendAddPlayerPacket(getId());
				}
				else {
					auto npc = reinterpret_cast<NpcSession*>(p);
					npc->wakeup();
					npc->event_characterMove(getId());
				}
				sendAddPlayerPacket(p->getId());
			}
		}
	}
}

void PlayerSession::processPacket(Packet packet) {
	if(state == SessionState::Free) {
		cout << "Client not found\n";
		return;
	}

	switch(packet.type) {
		case C2S_P_LOGIN: {
			cs_packet_login* p = reinterpret_cast<cs_packet_login*>(&packet);

            for(const auto& [key, session] : Session::sessions) {
                shared_ptr<Session> client = session.load();
				// npc이거나 접속을 종료했다면 같은 이름이어도 상관 없음
				if(client == nullptr || client->isNpc() || client->state == SessionState::Free) {
					continue;
				}

				// 같은 이름을 가진 유저가 이미 접속중이면
                if(strcmp(p->name, client->character.name) == 0) {
                    cout << "Login failed: " << p->name << " already logged in.\n";
                    cout << "state: " << static_cast<int>(client->state.load()) << endl;
                    sc_packet_login_fail lfp { };
                    lfp.size = sizeof(lfp);
                    lfp.type = S2C_P_LOGIN_FAIL;
                    lfp.id = key;
                    lfp.reason = 1; // 다른 클라이언트에서 사용중
                    doSend(&lfp);
                    return;
                }
            }

			strcpy_s(character.name, p->name);

			DbRequestParameters req { DbRequest::Load, getId(), { } };
			strcpy_s(req.name, p->name);
			Server::db_connection.request(req);

			break;
		}
		case C2S_P_MOVE: {
			cs_packet_move* p = reinterpret_cast<cs_packet_move*>(&packet);
			character.last_move_time = p->move_time;
			short x = character.x;
			short y = character.y;

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

            if(!Server::map.isValidPosition(x, y)) {
                // 이동 불가능한 위치
                // move_packet을 보내야 성능측정이 정상적으로 될거 같음
				break;
            }

			character.x = x;
			character.y = y;

			int curr_idx_x = x / SECTOR_SIZE;
			int curr_idx_y = y / SECTOR_SIZE;
			if(prev_idx_x != curr_idx_x || prev_idx_y != curr_idx_y) {
				sectors[prev_idx_y][prev_idx_x].borrow()->erase(this);
				sectors[curr_idx_y][curr_idx_x].borrow()->insert(this);
			}

			unordered_set<id_t> near_list;
			auto client_vl = view_list.borrow();
			unordered_set<id_t> old_vlist = *client_vl;
			client_vl.release();

			short min_x = (x - VIEW_RANGE) / SECTOR_SIZE;
			short max_x = (x + VIEW_RANGE) / SECTOR_SIZE;
			short min_y = (y - VIEW_RANGE) / SECTOR_SIZE;
			short max_y = (y + VIEW_RANGE) / SECTOR_SIZE;
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
					if(p->getId() == getId()) continue; // 자기 자신은 제외
					if(character.canSee(p->character)) {
						near_list.insert(p->getId());
					}
				}
			}

			sendMovePacket(getId());

			for(auto& pl : near_list) {
				shared_ptr<Session> cpl = Session::sessions.at(pl);
				if(cpl->isPc()) {
					auto pc = reinterpret_cast<PlayerSession*>(cpl.get());
					auto cpl_vl = pc->view_list.borrow();
					if(cpl_vl->count(getId())) {
						cpl_vl.release();
						pc->sendMovePacket(getId());
					}
					else {
						cpl_vl.release();
						pc->sendAddPlayerPacket(getId());
					}
				}
				else {
					auto npc = reinterpret_cast<NpcSession*>(cpl.get());
					npc->wakeup();
					npc->event_characterMove(getId());
				}

				if(old_vlist.count(pl) == 0) {
					sendAddPlayerPacket(pl);
				}
			}

			for(auto& pl : old_vlist) {
				if(0 == near_list.count(pl)) {
					sendRemovePlayerPacket(pl);
					shared_ptr<Session> cl = Session::sessions.at(pl);
					if(cl->isPc()) {
						auto pc = reinterpret_cast<PlayerSession*>(cl.get());
						pc->sendRemovePlayerPacket(getId());
					}
				}
			}

			break;
		}
	}
}


void PlayerSession::doRecv() {
	DWORD recv_flag = 0;
	memset(&recv_over.overlapped, 0, sizeof(recv_over.overlapped));
	recv_over.wsabuf.len = BUF_SIZE;
	recv_over.wsabuf.buf = recv_over.buffer;

	WSARecv(socket, &recv_over.wsabuf, 1, 0, &recv_flag,
		&recv_over.overlapped, NULL);
}

void PlayerSession::doSend(void* packet) const {
	OverlappedEx* sdata = new OverlappedEx { IoOperation::Send };

	CHAR* buf = reinterpret_cast<CHAR*>(packet);
	const unsigned char packet_size = static_cast<unsigned char>(buf[0]);
	memcpy(sdata->buffer, buf, packet_size);
	sdata->wsabuf.len = packet_size;

	int ret = WSASend(socket, &sdata->wsabuf, 1, NULL, NULL, &sdata->overlapped, NULL);
	if(ret != 0) {
		auto err_no = WSAGetLastError();
		if(err_no != WSA_IO_PENDING) {
			// WSAECONNRESET(10054)
			//printf("%d[packet: %d]: Send Error - %d\n", _id, (int)((char*)packet)[1], err_no);
			delete sdata;
		}
	}
}


void PlayerSession::sendLoginInfoPacket() const {
	sc_packet_avatar_info p { };
	p.id = getId();
	p.size = sizeof(p);
	p.type = S2C_P_AVATAR_INFO;
	p.x = character.x;
	p.y = character.y;
	doSend(&p);
}

void PlayerSession::sendMovePacket(id_t c_id) const {
	shared_ptr<Session> client = Session::sessions.at(c_id);
	sc_packet_move p { };
	p.size = sizeof(p);
	p.type = S2C_P_MOVE;
	p.id = c_id;
	p.x = client->character.x;
	p.y = client->character.y;
	p.move_time = client->character.last_move_time;
	doSend(&p);
}

void PlayerSession::sendAddPlayerPacket(id_t c_id) {
	shared_ptr<Session> client = Session::sessions.at(c_id);
	sc_packet_enter add_packet { };
	add_packet.size = sizeof(add_packet);
	add_packet.type = S2C_P_ENTER;
	add_packet.id = c_id;
	strcpy_s(add_packet.name, client->character.name);
	add_packet.x = client->character.x;
	add_packet.y = client->character.y;
	{
		view_list.borrow()->insert(c_id);
	}
	doSend(&add_packet);
}

void PlayerSession::sendRemovePlayerPacket(id_t c_id) {
	{
		auto vl = view_list.borrow();
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
	doSend(&p);
}

void PlayerSession::sendChatPacket(id_t p_id, const char* mess) const {
	sc_packet_chat packet { };
	packet.id = p_id;
	packet.size = static_cast<unsigned char>(sizeof(packet) - MAX_CHAT_LENGTH + strlen(mess) + 1);	// 그냥 sizeof만 하면 overflow
	packet.type = S2C_P_CHAT;
	strcpy_s(packet.message, mess);
	doSend(&packet);
}