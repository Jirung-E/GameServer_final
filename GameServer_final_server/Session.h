#pragma once

#include <array>
#include <atomic>
#include <unordered_set>
#include <concurrent_unordered_map.h>

#include "Character.h"
#include "Defines.h"
#include "Vault.h"


enum class SessionState {
	Alloc,
	InGame,
	Free,
};


class Session abstract {
public:
	static concurrency::concurrent_unordered_map<
		id_t, std::atomic<std::shared_ptr<Session>>
	> sessions;

	static std::array<std::array<
		Vault<std::unordered_set<Session*>>,
		NUM_SECTOR_X>, NUM_SECTOR_Y
	> sectors;

private:
	id_t id;

public:
	std::atomic<SessionState> state;
	Character character;

public:
	Session(id_t id);
	~Session();

public:
	id_t getId() const { return id; }

	virtual bool isNpc() const abstract;
	virtual bool isPc() const { return !isNpc(); }
};