#include "DbConnection.h"

#include <format>

#include "Network.h"
#include "OverlappedEx.h"
#include "Session.h"

using namespace std;


DbConnection::DbConnection() {

}


void DbConnection::run() {
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

					// 반환받을 데이터 지정(일단 전부 받음)
					retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &user_id, 100, &cb_user_id);
					retcode = SQLBindCol(hstmt, 2, SQL_C_WCHAR, user_name, MAX_ID_LENGTH, &cb_user_name);
					retcode = SQLBindCol(hstmt, 3, SQL_C_LONG, &user_level, 100, &cb_user_level);
					retcode = SQLBindCol(hstmt, 4, SQL_C_LONG, &user_x, 100, &cb_x);
					retcode = SQLBindCol(hstmt, 5, SQL_C_LONG, &user_y, 100, &cb_y);

					loop();

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


void DbConnection::request(DbRequestParameters request) {
	request_queue.push(request);
}


bool DbConnection::load(char name[MAX_ID_LENGTH], Character* data) {
	bool success = false;

	size_t name_len = strlen(name);
	std::wstring name_wstr { name, name + name_len };

	wstring query = std::format(L"EXEC load_data \'{:{}}\'",
		name_wstr, static_cast<int>(MAX_ID_LENGTH));

	retcode = SQLExecDirect(hstmt, query.data(), SQL_NTS);
	if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLFetch(hstmt);
		if(retcode == SQL_ERROR/* || retcode == SQL_SUCCESS_WITH_INFO*/) {
			//show_error();
		}
		if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			data->x = user_x;
			data->y = user_y;
			strcpy_s(data->name, MAX_ID_LENGTH, name);
			success = true;
		}
	}
	else {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
	}

	SQLCancel(hstmt);

	return success;
}

bool DbConnection::store(char name[MAX_ID_LENGTH], short x, short y) {
	size_t name_len = strlen(name);
	std::wstring name_wstr { name, name + name_len };

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

void DbConnection::loop() {
	DbRequestParameters request;
	while(true) {
		if(request_queue.try_pop(request)) {
			switch(request.request_type) {
				case DbRequest::Load: {
					shared_ptr<Session> client = Session::sessions.at(request.obj_id);
					bool success = load(request.name, &client->character);

					IoOperation op = IoOperation::LoginFail;
					if(success) {
						op = IoOperation::LoginOk;
					}
					OverlappedEx* overlapped = new OverlappedEx { op };
					PostQueuedCompletionStatus(Server::h_iocp, NULL, request.obj_id, &overlapped->overlapped);

					break;
				}
				case DbRequest::Store: {
					bool success = store(request.name, request.x, request.y);
					break;
				}
			}
		}
	}
}


void DbConnection::HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode) {
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
