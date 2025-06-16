#pragma once

#include <atomic>
#include <unordered_set>

#include "game_header.h"
#include "Defines.h"
#include "Vault.h"


class Character {
public:
	unsigned short level;
	unsigned int exp;
    short hp;
	short x;
	short y;

	char name[MAX_ID_LENGTH];

	size_t last_move_time;

public:
	Character();

public:
	bool canSee(id_t target_id) const;
	bool canSee(const Character& other) const;
};
