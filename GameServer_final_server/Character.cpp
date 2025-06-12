#include "Character.h"

#include "Session.h"

using namespace std;


Character::Character():
	x { 0 },
	y { 0 },
	name { },
	last_move_time { 0 }
{

}


bool Character::canSee(id_t target_id) const {
	shared_ptr<Session> target_session = Session::sessions.at(target_id);
	const Character& target_character = target_session->character;
	return canSee(target_character);
}

bool Character::canSee(const Character& other) const {
	if(abs(x - other.x) > VIEW_RANGE) return false;
	return abs(y - other.y) <= VIEW_RANGE;

	return false;
}
