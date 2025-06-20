#include "DbConnection.h"

#include <format>

#include "Network.h"
#include "OverlappedEx.h"
#include "Session.h"

using namespace std;


DbConnection::DbConnection():
    henv { NULL },
    hdbc { NULL },
    hstmt { NULL },
    retcode { SQL_SUCCESS },
    user_name { L"" },
    level { 0 }, exp { 0 }, hp { 0 }, x { 0 }, y { 0 },
    cb_user_name { 0 }, cb_level { 0 }, cb_exp { 0 }, cb_hp { 0 }, cb_x { 0 }, cb_y { 0 }
{

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
					//retcode = SQLBindCol(hstmt, 1, SQL_C_WCHAR, user_name, MAX_ID_LENGTH, &cb_user_name);	// 이름을 불러올 필요는 없음
					retcode = SQLBindCol(hstmt, 1, SQL_C_LONG, &level, 100, &cb_level);
					retcode = SQLBindCol(hstmt, 2, SQL_C_LONG, &exp, 100, &cb_exp);
					retcode = SQLBindCol(hstmt, 3, SQL_C_LONG, &hp, 100, &cb_hp);
					retcode = SQLBindCol(hstmt, 4, SQL_C_LONG, &x, 100, &cb_x);
					retcode = SQLBindCol(hstmt, 5, SQL_C_LONG, &y, 100, &cb_y);

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


bool DbConnection::load(Character* data) {
	bool success = false;

	size_t name_len = strlen(data->name);
	std::wstring name_wstr { data->name, data->name + name_len };

	wstring query = std::format(L"EXEC load_data \'{:{}}\'",
		name_wstr, static_cast<int>(MAX_ID_LENGTH));

	retcode = SQLExecDirect(hstmt, query.data(), SQL_NTS);
	if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLFetch(hstmt);
		if(retcode == SQL_ERROR/* || retcode == SQL_SUCCESS_WITH_INFO*/) {
			//show_error();
		}
		if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
            data->level = static_cast<unsigned short>(level);
            data->exp = static_cast<unsigned int>(exp);
            data->hp = static_cast<short>(hp);
			data->x = static_cast<short>(x);
			data->y = static_cast<short>(y);
			//strcpy_s(data->name, name);	// 이름은 이미 들어가있음
			success = true;
		}
	}
	else {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
	}

	SQLCancel(hstmt);

	return success;
}

bool DbConnection::add(Character* data) {
	size_t name_len = strlen(data->name);
	std::wstring name_wstr { data->name, data->name + name_len };

	wstring query = format(L"EXEC add_data \'{:{}}\', {}, {}, {}, {}, {}",
		name_wstr, static_cast<int>(MAX_ID_LENGTH),
        data->level, data->exp, data->hp, data->x, data->y);

	retcode = SQLExecDirect(hstmt, query.data(), SQL_NTS);
	if(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		return true;
	}
	else {
		HandleDiagnosticRecord(hstmt, SQL_HANDLE_STMT, retcode);
		return false;
	}
}

bool DbConnection::store(const Character* data) {
	size_t name_len = strlen(data->name);
	std::wstring name_wstr { data->name, data->name + name_len };

	wstring query = format(L"EXEC store_data \'{:{}}\', {}, {}, {}, {}, {}",
		name_wstr, static_cast<int>(MAX_ID_LENGTH), 
		data->level, data->exp, data->hp, data->x, data->y);

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
					bool success = load(&client->character);

					IoOperation op = IoOperation::LoginFail;
					if(success) {
						op = IoOperation::LoginOk;
					}
					else {
						// 회원가입(DB에 추가)
                        success = add(&client->character);
						if(success) {
							op = IoOperation::NewPlayer;
						}
					}
					OverlappedEx* overlapped = new OverlappedEx { op };
					PostQueuedCompletionStatus(Server::h_iocp, NULL, request.obj_id, &overlapped->overlapped);

					break;
				}
				case DbRequest::Store: {
					bool success = store(&request.data);
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
