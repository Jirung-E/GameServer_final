#pragma once

#include <chrono>

#include "game_header.h"


using id_t = int;

template<typename T>
concept ChronoDuration = std::is_base_of_v<std::chrono::duration<typename T::rep, typename T::period>, T>;


constexpr int BUF_SIZE = 1024 * 8; // 8KB

constexpr int VIEW_RANGE = 7;	// 15 * 15
constexpr int SECTOR_SIZE = 20;

constexpr static int ceilDiv(int a, int b) {
	return (a + b - 1) / b;
}

constexpr int NUM_SECTOR_X = ceilDiv(MAP_WIDTH, SECTOR_SIZE);
constexpr int NUM_SECTOR_Y = ceilDiv(MAP_HEIGHT, SECTOR_SIZE);
