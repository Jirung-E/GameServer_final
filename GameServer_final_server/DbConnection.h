#pragma once

#include <WS2tcpip.h>
#include <windows.h>

#define UNICODE
#include <sqlext.h>

#include <concurrent_queue.h>

#include "Character.h"
#include "Defines.h"


enum DbRequest {
	Load,
	Store,
};


struct DbRequestParameters {
	DbRequest request_type;
	id_t obj_id;
	char name[MAX_ID_LENGTH];
	short x;
	short y;
};


class DbConnection {
private:
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt = 0;
	SQLRETURN retcode;
	SQLWCHAR user_name[50];
	SQLINTEGER user_id, user_level, user_x, user_y;
	SQLLEN cb_user_name = 0, cb_user_id = 0, cb_user_level = 0, cb_x = 0, cb_y = 0;

	concurrency::concurrent_queue<DbRequestParameters> request_queue;

public:
	DbConnection();

public:
	void run();

	void request(DbRequestParameters request);

private:
	bool load(char name[MAX_ID_LENGTH], Character* data);
	bool store(char name[MAX_ID_LENGTH], short x, short y);
	void loop();

	void showError() {
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
	void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode);
};
