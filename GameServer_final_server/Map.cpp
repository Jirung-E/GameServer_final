#include "Map.h"

#include <fstream>
#include <iostream>
#include <random>

using namespace std;


static const size_t MAP_SIZE = MAP_HEIGHT * MAP_WIDTH;

static random_device rd;
static default_random_engine dre { rd() };


Map::Map(): map { } {
    ifstream map_file { "../map_with_obstacles.mapcontents" };
    if(!map_file) {
        cout << "Map load failed" << endl;
        return;
    }

    for(int i = 0; i < MAP_HEIGHT; ++i) {
        for(int j = 0; j < MAP_WIDTH; ++j) {
            char c;
            map_file.get(c);
            if(map_file.eof()) {
                cout << "Map file ended unexpectedly" << endl;
                return;
            }
            map[i][j] = static_cast<MapContent>(c);
        }
        map_file.ignore(); // 줄바꿈 문자 무시
    }
}


MapContent Map::get(int x, int y) const {
    if(0 <= x && x < MAP_WIDTH && 0 <= y && y < MAP_HEIGHT) {
        return map[y][x];
    }
    return MapContent::Invalid; // 유효하지 않은 위치
}

bool Map::isValidPosition(int x, int y) const {
    if(0 <= x && x < MAP_WIDTH && 0 <= y && y < MAP_HEIGHT) {
        // 빈칸이거나 시작 위치인 경우
        return map[y][x] == MapContent::Empty || map[y][x] == MapContent::Spawn;
    }

    return false;
}


std::vector<std::pair<int, int>> Map::getContentPositions(MapContent content, int x, int y, int range) const {
    std::vector<std::pair<int, int>> positions;
    positions.reserve(static_cast<size_t>((2 * range + 1) * (2 * range + 1)));

    for(int i = -range; i <= range; ++i) {
        for(int j = -range; j <= range; ++j) {
            int new_x = x + i;
            int new_y = y + j;
            if(0 <= new_x && new_x < MAP_WIDTH && 0 <= new_y && new_y < MAP_HEIGHT) {
                if(map[new_y][new_x] == content) {
                    positions.push_back(std::make_pair(new_x, new_y));
                }
            }
        }
    }

    return positions;
}


//std::vector<std::pair<int, int>> Map::getValidPositions(int x, int y, int range) const {
//    std::vector<std::pair<int, int>> valid_positions;
//    valid_positions.reserve(static_cast<size_t>((2 * range + 1) * (2 * range + 1)));
//
//    for(int i = -range; i <= range; ++i) {
//        for(int j = -range; j <= range; ++j) {
//            int new_x = x + i;
//            int new_y = y + j;
//            if(0 <= new_x && new_x < MAP_WIDTH && 0 <= new_y && new_y < MAP_HEIGHT) {
//                if(map[new_y][new_x] == MapContent::Empty || map[new_y][new_x] == MapContent::Spawn) {
//                    valid_positions.push_back(std::make_pair(new_x, new_y));
//                }
//            }
//        }
//    }
//
//    return valid_positions;
//}

std::vector<std::pair<int, int>> Map::getEmptyPositions(int x, int y, int range) const {
    std::vector<std::pair<int, int>> empty_positions;
    empty_positions.reserve(static_cast<size_t>((2 * range + 1) * (2 * range + 1)));

    for(int i = -range; i <= range; ++i) {
        for(int j = -range; j <= range; ++j) {
            int new_x = x + i;
            int new_y = y + j;
            if(0 <= new_x && new_x < MAP_WIDTH && 0 <= new_y && new_y < MAP_HEIGHT) {
                if(map[new_y][new_x] == MapContent::Empty) {
                    empty_positions.push_back(std::make_pair(new_x, new_y));
                }
            }
        }
    }

    return empty_positions;
}

std::vector<std::pair<int, int>> Map::getSpawnPositions(int x, int y, int range) const {
    std::vector<std::pair<int, int>> spawn_positions;
    spawn_positions.reserve(static_cast<size_t>((2 * range + 1) * (2 * range + 1)));

    for(int i = -range; i <= range; ++i) {
        for(int j = -range; j <= range; ++j) {
            int new_x = x + i;
            int new_y = y + j;
            if(0 <= new_x && new_x < MAP_WIDTH && 0 <= new_y && new_y < MAP_HEIGHT) {
                if(map[new_y][new_x] == MapContent::Spawn) {
                    spawn_positions.push_back(std::make_pair(new_x, new_y));
                }
            }
        }
    }

    return spawn_positions;
}

//std::vector<std::pair<int, int>> Map::getValidPositions() const {
//    static bool initialized = false;
//    static std::vector<std::pair<int, int>> valid_positions;
//
//    if(!initialized) {
//        valid_positions.reserve(MAP_SIZE);
//
//        for(int i = 0; i < MAP_HEIGHT; ++i) {
//            for(int j = 0; j < MAP_WIDTH; ++j) {
//                if(map[i][j] == MapContent::Empty || map[i][j] == MapContent::Spawn) {
//                    valid_positions.emplace_back(j, i);
//                }
//            }
//        }
//
//        std::atomic_thread_fence(std::memory_order_seq_cst);
//        initialized = true;
//    }
//
//    return valid_positions;
//}

std::vector<std::pair<int, int>> Map::getEmptyPositions() const {
    static bool initialized = false;
    static std::vector<std::pair<int, int>> empty_positions;

    if(!initialized) {
        empty_positions.reserve(MAP_SIZE);

        for(int i = 0; i < MAP_HEIGHT; ++i) {
            for(int j = 0; j < MAP_WIDTH; ++j) {
                if(map[i][j] == MapContent::Empty) {
                    empty_positions.emplace_back(j, i);
                }
            }
        }

        std::atomic_thread_fence(std::memory_order_seq_cst);
        initialized = true;
    }

    return empty_positions;
}

std::vector<std::pair<int, int>> Map::getSpawnPositions() const {
    static bool initialized = false;
    static std::vector<std::pair<int, int>> spawn_positions;

    if(!initialized) {
        spawn_positions.reserve(MAP_SIZE);

        for(int i = 0; i < MAP_HEIGHT; ++i) {
            for(int j = 0; j < MAP_WIDTH; ++j) {
                if(map[i][j] == MapContent::Spawn) {
                    spawn_positions.emplace_back(j, i);
                }
            }
        }

        std::atomic_thread_fence(std::memory_order_seq_cst);
        initialized = true;
    }

    return spawn_positions;
}


//std::pair<int, int> Map::getRandomValidPosition() const {
//    static auto valid_positions = getValidPositions();
//    if(valid_positions.empty()) {
//        return { 0, 0 }; // 기본 위치
//    }
//
//    static uniform_int_distribution<size_t> uid { 0, valid_positions.size() - 1 };
//
//    return valid_positions[uid(dre)];
//}

std::pair<int, int> Map::getRandomEmptyPosition() const {
    static auto empty_positions = getEmptyPositions();
    if(empty_positions.empty()) {
        return { 0, 0 }; // 기본 위치
    }

    static uniform_int_distribution<size_t> uid { 0, empty_positions.size() - 1 };

    return empty_positions[uid(dre)];
}

std::pair<int, int> Map::getRandomSpawnPosition() const {
    static auto spawn_positions = getSpawnPositions();
    if(spawn_positions.empty()) {
        return { 0, 0 }; // 기본 위치
    }

    static uniform_int_distribution<size_t> uid { 0, spawn_positions.size() - 1 };

    return spawn_positions[uid(dre)];
}
