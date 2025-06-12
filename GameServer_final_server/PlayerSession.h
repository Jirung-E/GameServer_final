#pragma once

#include "Session.h"
#include "OverlappedEx.h"
#include "Vault.h"
#include "PacketParser.h"


class PlayerSession: public Session {
	OverlappedEx recv_over;
	SOCKET socket;

public:
	Vault<std::unordered_set<id_t>> view_list;
	PacketParser packet_parser;

public:
	PlayerSession(id_t id, SOCKET socket);

public:
	virtual bool isNpc() const override { return false; }

	void setSocket(SOCKET s) { socket = s; }
	SOCKET getSocket() const { return socket; }

	void login();

	void processPacket(Packet packet);

	void doRecv();
	void doSend(void* packet) const;

	void sendLoginInfoPacket() const;
	void sendMovePacket(id_t c_id) const;
	void sendAddPlayerPacket(id_t c_id);
	void sendRemovePlayerPacket(id_t c_id);
	void sendChatPacket(id_t p_id, const char* mess) const;
};