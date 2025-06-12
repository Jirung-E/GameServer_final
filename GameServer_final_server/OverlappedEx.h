#pragma once

#include <WS2tcpip.h>

#include "Defines.h"


enum class IoOperation {
	Accept,
	Receive,
	Send,
	LoginOk,
	LoginFail,
	NpcMove,
	NpcAI
};


class OverlappedEx {
public:
	WSAOVERLAPPED overlapped;
	IoOperation io_op;
	SOCKET accept_socket;
	WSABUF wsabuf;
	char buffer[BUF_SIZE];
	id_t ai_target_obj;

public:
	OverlappedEx(IoOperation op):
		io_op { op },
		accept_socket { 0 },
		buffer { },
		ai_target_obj { -1 }
	{
		ZeroMemory(&overlapped, sizeof(overlapped));
		wsabuf.buf = buffer;
		wsabuf.len = BUF_SIZE;
	}

	WSAOVERLAPPED* getOverlapped() {
		return &overlapped;
	}
};