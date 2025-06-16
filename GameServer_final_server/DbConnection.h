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

	// obj_id�� ���� �������� �̹� �α׾ƿ��ؼ� �ٸ� ������ ��ü�Ǿ����� ���ɼ��� �����Ƿ� �����ؼ� ����
	// name�� *�� �ƴϹǷ� ���� ����ɰ���
    Character data;
};


class DbConnection {
private:
	SQLHENV henv;
	SQLHDBC hdbc;
	SQLHSTMT hstmt;
	SQLRETURN retcode;
	SQLWCHAR user_name[50];
	SQLINTEGER level, exp, hp, x, y;
	SQLLEN cb_user_name, cb_level, cb_exp, cb_hp, cb_x, cb_y;

	concurrency::concurrent_queue<DbRequestParameters> request_queue;

public:
	DbConnection();

public:
	void run();

	void request(DbRequestParameters request);

private:
	bool load(Character* data);
	bool add(Character* data);
	bool store(const Character* data);
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
