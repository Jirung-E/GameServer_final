#pragma once

#include <WS2tcpip.h>
#include <MSWSock.h>

#include <vector>
#include <thread>

#include "Map.h"
#include "OverlappedEx.h"
#include "DbConnection.h"

#pragma comment(lib, "WS2_32.lib")
#pragma comment(lib, "MSWSock.lib")


class WsaGuard {
public:
    WsaGuard() {
        WSADATA WSAData;
        int ret = WSAStartup(MAKEWORD(2, 2), &WSAData);
        if(ret != 0) {
            exit(-1);
        }
    }
    ~WsaGuard() {
        WSACleanup();
    }
};

class Server {
public:
    static Map map;
    static DbConnection db_connection;
    static HANDLE h_iocp;

private:
    SOCKET server_socket = INVALID_SOCKET;

public:
    bool bindSocket();
    bool startListen();
    void initializeNpc();
    bool startAccept();
    void run();

private:
    void doAccept(OverlappedEx* overlapped);
    void worker();
    void disconnect(id_t c_id);
};