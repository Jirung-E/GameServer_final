#include "Network.h"

#include <iostream>
#include <random>

#include "game_header.h"
#include "Session.h"
#include "PlayerSession.h"
#include "NpcSession.h"
#include "NpcAI.h"

using namespace std;


static void printErrorMessage(int s_err) {
	WCHAR* lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, s_err,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::wcout << lpMsgBuf << std::endl;
	LocalFree(lpMsgBuf);
}


static atomic<id_t> new_id;
static PlayerSession* newClient(SOCKET socket) {
	for(auto& cl : Session::sessions) {
		shared_ptr<Session> p = cl.second;
		if(p->state == SessionState::Free && p->isPc()) {
            strcpy_s(p->character.name, ""); // Reset name
			PlayerSession* client = reinterpret_cast<PlayerSession*>(p.get());
			client->state = SessionState::Alloc;
			client->setSocket(socket);
			return reinterpret_cast<PlayerSession*>(p.get());
		}
	}

	if(new_id >= MAX_USER) {
		return nullptr;
	}

	shared_ptr<Session> p = make_shared<PlayerSession>(new_id++, socket);
	Session::sessions.insert(make_pair(p->getId(), p));
	return reinterpret_cast<PlayerSession*>(p.get());
}


Map Server::map { };
HANDLE Server::h_iocp = nullptr;
DbConnection Server::db_connection;


bool Server::bindSocket() {
	server_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if(server_socket <= 0) {
		auto err_no = WSAGetLastError();
		printErrorMessage(err_no);
		return false;
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
		printErrorMessage(err_no);
		return 0;
	}

	return true;
}


bool Server::startListen() {
	int listen_res = listen(server_socket, SOMAXCONN);
	if(listen_res == SOCKET_ERROR) {
		cout << "Listen Error\n";
		auto err_no = WSAGetLastError();
		printErrorMessage(err_no);
		return false;
	}

	return true;
}


void Server::initializeNpc() {
	cout << "NPC initialize begin.\n";
    
	vector<pair<int, int>> valid_positions = map.getValidPositions();
	uniform_int_distribution<size_t> uid { 0, valid_positions.size() - 1 };
    random_device random_device;
    default_random_engine dre { random_device() };

	for(id_t i = MAX_USER; i < MAX_USER + NUM_MONSTER; ++i) {
        auto [x, y] = valid_positions[uid(dre)];

		shared_ptr<Session> npc = make_shared<NpcSession>(i, x, y);
		Session::sessions.insert(make_pair(i, npc));
		npc->state = SessionState::InGame;

		// Sector
		int idx_x = npc->character.x / SECTOR_SIZE;
		int idx_y = npc->character.y / SECTOR_SIZE;
		Session::sectors[idx_y][idx_x].borrow()->insert(npc.get());
	}

	cout << "NPC initialize end.\n";
}


bool Server::startAccept() {
	h_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(server_socket), h_iocp, -1, 0);

	OverlappedEx accept_over { IoOperation::Accept };
	doAccept(&accept_over);

	return true;
}


void Server::run() {
	unsigned int num_threads = thread::hardware_concurrency();
	vector<thread> worker_threads;
	worker_threads.reserve(num_threads);

	for(int i=0; i<num_threads-2; ++i) {
		worker_threads.emplace_back([this]() { this->worker(); });
	}

	worker_threads.emplace_back([this]() { NpcAI::npcAiLoop(h_iocp); });
	worker_threads.emplace_back([this]() { db_connection.run(); });

	for(auto& th : worker_threads) {
		th.join();
	}
}


void Server::doAccept(OverlappedEx* overlapped) {
	SOCKET client_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	overlapped->accept_socket = client_socket;
	AcceptEx(server_socket, client_socket, overlapped->buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		NULL, &overlapped->overlapped);
}


void Server::worker() {
	while(true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OverlappedEx* ex_over = reinterpret_cast<OverlappedEx*>(over);
		if(FALSE == ret) {
			if(ex_over->io_op == IoOperation::Accept) {
				cout << "Accept Error";
			}
			else {
				cout << "GQCS Error on client[" << key << "]\n";
				disconnect(static_cast<id_t>(key));
				if(ex_over->io_op == IoOperation::Send) delete ex_over;
				continue;
			}
		}

		if((0 == num_bytes) && ((ex_over->io_op == IoOperation::Receive) || (ex_over->io_op == IoOperation::Send))) {
			disconnect(static_cast<id_t>(key));
			if(ex_over->io_op == IoOperation::Send) {
				delete ex_over;
			}
			continue;
		}

		switch(ex_over->io_op) {
			case IoOperation::Accept: {
				PlayerSession* client = newClient(ex_over->accept_socket);
				if(client != nullptr) {
					CreateIoCompletionPort(reinterpret_cast<HANDLE>(ex_over->accept_socket),
						h_iocp, client->getId(), 0);
					client->doRecv();
					doAccept(ex_over);
				}
				else {
					cout << "Max user exceeded.\n";
				}
				break;
			}
			case IoOperation::Receive: {
				shared_ptr<Session> session = Session::sessions.at(static_cast<id_t>(key));

				if(session == nullptr) {
					cout << "Client not found\n";
					break;
				}

				PlayerSession* client = reinterpret_cast<PlayerSession*>(session.get());
				char* p = ex_over->buffer;
				client->packet_parser.push(p, num_bytes);
				while(client->packet_parser.canPop()) {
					Packet packet = client->packet_parser.pop();
					client->processPacket(packet);
				}

				client->doRecv();

				break;
			}
			case IoOperation::Send: {
				delete ex_over;
				break;
			}
			case IoOperation::LoginOk: {
				shared_ptr<Session> session = Session::sessions.at(static_cast<id_t>(key));
				PlayerSession* client = reinterpret_cast<PlayerSession*>(session.get());

				client->login();

				delete ex_over;

				break;
			}
			case IoOperation::LoginFail: {
                // DB에서 로드 실패
				shared_ptr<Session> session = Session::sessions.at(static_cast<id_t>(key));
				PlayerSession* client = reinterpret_cast<PlayerSession*>(session.get());

				client->state = SessionState::Free;
				sc_packet_login_fail lfp { };
				lfp.size = sizeof(lfp);
				lfp.type = S2C_P_LOGIN_FAIL;
				client->doSend(&lfp);

				delete ex_over;

				break;
			}
			case IoOperation::NpcMove: {
				shared_ptr<Session> session = Session::sessions.at(static_cast<id_t>(key));
				NpcSession* npc = reinterpret_cast<NpcSession*>(session.get());
				npc->event_characterMove(ex_over->ai_target_obj);
				delete ex_over;
				break;
			}
			case IoOperation::NpcAI: {
				shared_ptr<Session> session = Session::sessions.at(static_cast<id_t>(key));
				NpcSession* npc = reinterpret_cast<NpcSession*>(session.get());
				npc->runAI();
				delete ex_over;
				break;
			}
		}
	}
}

void Server::disconnect(id_t c_id) {
	shared_ptr<Session> client = Session::sessions.at(c_id);
	auto pc = reinterpret_cast<PlayerSession*>(client.get());

	auto vl_borrowed = pc->view_list.borrow();
	unordered_set<id_t> vl = *vl_borrowed;
	vl_borrowed.release();

	for(auto& p_id : vl) {
		shared_ptr<Session> p = Session::sessions.at(p_id);
		if(p == nullptr || p->isNpc() || p->state != SessionState::InGame || p_id == c_id) continue;
		auto c = reinterpret_cast<PlayerSession*>(p.get());
		c->sendRemovePlayerPacket(c_id);
	}

	{
		int idx_x = client->character.x / SECTOR_SIZE;
		int idx_y = client->character.y / SECTOR_SIZE;
		Session::sectors[idx_y][idx_x].borrow()->erase(client.get());
	}

	DbRequestParameters db_req { DbRequest::Store, c_id, { }, client->character.x, client->character.y };
	strcpy_s(db_req.name, client->character.name);
	db_connection.request(db_req);

	closesocket(pc->getSocket());

	client->state = SessionState::Free;
}
