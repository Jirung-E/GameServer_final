#include "Map.h"

#include <fstream>
#include <iostream>
#include <random>

using namespace std;


static const size_t MAP_SIZE = MAP_HEIGHT * MAP_WIDTH;


Map::Map(): map { } {
    ifstream map_file { "../map2.map", ios::binary };
    if(!map_file) {
        cout << "Map load failed" << endl;
        return;
    }

    for(int i=0; i<MAP_SIZE / 8; ++i) {
        char c;
        map_file >> c;
        for(int j = 0; j < 8; ++j) {
            int row = (i * 8 + j) / MAP_WIDTH;
            int col = (i * 8 + j) % MAP_WIDTH;
            if(row < MAP_HEIGHT && col < MAP_WIDTH) {
                //*(char*)&(map[row][col]) = c;
                map[row][col] = (c & (1 << (7 - j))) != 0;
            }
        }
    }
}


bool Map::isValidPosition(int x, int y) const {
    if(0 <= x && x < MAP_WIDTH && 0 <= y && y < MAP_HEIGHT) {
        return map[y][x];
    }

    return false;
}

std::vector<std::pair<int, int>> Map::getValidPositions(int x, int y, int range) const {
    std::vector<std::pair<int, int>> valid_positions;
    valid_positions.reserve(static_cast<size_t>((2 * range + 1) * (2 * range + 1)));

    for(int i = -range; i <= range; ++i) {
        for(int j = -range; j <= range; ++j) {
            int new_x = x + i;
            int new_y = y + j;
            if(0 <= new_x && new_x < MAP_WIDTH && 0 <= new_y && new_y < MAP_HEIGHT) {
                if(map[new_y][new_x]) {
                    valid_positions.push_back(std::make_pair(new_x, new_y));
                }
            }
        }
    }

    return valid_positions;
}

std::vector<std::pair<int, int>> Map::getValidPositions() const {
    static bool initialized = false;
    static std::vector<std::pair<int, int>> valid_positions;

    if(!initialized) {
        valid_positions.reserve(MAP_SIZE);

        for(int i = 0; i < MAP_HEIGHT; ++i) {
            for(int j = 0; j < MAP_WIDTH; ++j) {
                if(map[i][j]) {
                    valid_positions.emplace_back(j, i);
                }
            }
        }

        std::atomic_thread_fence(std::memory_order_seq_cst);
        initialized = true;
    }

    return valid_positions;
}


std::pair<int, int> Map::getRandomValidPosition() const {
    static auto valid_positions = getValidPositions();
    if(valid_positions.empty()) {
        return { 0, 0 }; // 기본 위치
    }

    static uniform_int_distribution<size_t> uid { 0, valid_positions.size() - 1 };
    static random_device random_device;
    static default_random_engine dre { random_device() };

    return valid_positions[uid(dre)];
}
