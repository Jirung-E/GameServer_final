#pragma once

#include "Session.h"
#include "Vault.h"
#include "NpcAi.h"


class NpcSession: public Session {
	Vault<lua_State*> lua;
	std::atomic_bool is_active;

public:
	NpcSession(id_t id, int x, int y);
	~NpcSession();

public:
	virtual bool isNpc() const override { return true; }

	virtual void revive() override;

	void wakeup();
	void runAI();
	void event_characterMove(id_t character_id);

	template<ChronoDuration T>
	void postMsg(NpcAI::AiEvent event, T delay, id_t target_id = -1) {
		NpcAI::ai_event_queue.push(NpcAI::AiEventParameters {
			getId(),
			std::chrono::high_resolution_clock::now() + delay,
			event,
			target_id
			});
	}
};