#pragma once

#include <chrono>
#include <concurrent_priority_queue.h>

#include "Defines.h"
#include "include/lua.hpp"

#pragma comment(lib, "lua54.lib")


namespace NpcAI {
    enum class AiEvent {
        RunAI,
        Move,
        Heal,
        Attack,
    };

    struct AiEventParameters {
        id_t obj_id;
        std::chrono::high_resolution_clock::time_point wakeup_time;
        AiEvent event_id;
        id_t target_id;
        constexpr bool operator<(const AiEventParameters& other) const {
            return (wakeup_time > other.wakeup_time);
        }
    };

    extern concurrency::concurrent_priority_queue<AiEventParameters> ai_event_queue;

    int lua_getX(lua_State* L);
    int lua_getY(lua_State* L);
    int lua_move(lua_State* L);
    int lua_sendMessage(lua_State* L);

    void npcAiLoop(HANDLE h_iocp);
}