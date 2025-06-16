#include "Map.h"

#include <fstream>
#include <iostream>

using namespace std;


Map::Map() {
    ifstream map_file { "../map2.map", ios::binary };
    if(!map_file) {
        for(int i = 0; i < MAP_HEIGHT; ++i) {
            for(int j = 0; j < MAP_WIDTH; ++j) {
                map[i][j] = true;
            }
        }
        cout << "Map load failed" << endl;
        return;
    }

    const size_t size = MAP_HEIGHT * MAP_WIDTH / 8;
    array<char, size> map_data;
    map_file.read(map_data.data(), size);

    for(int i=0; i<size; ++i) {
        char c = map_data[i];
        for(int j = 0; j < 8; ++j) {
            size_t row = (i * 8 + j) / MAP_WIDTH;
            size_t col = (i * 8 + j) % MAP_WIDTH;
            if(row < MAP_HEIGHT && col < MAP_WIDTH) {
                *(char*)&(map[row][col]) = c;
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
    valid_positions.reserve((2 * range + 1) * (2 * range + 1));

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
    std::vector<std::pair<int, int>> valid_positions;
    valid_positions.reserve(MAP_HEIGHT * MAP_WIDTH);

    for(int i = 0; i < MAP_HEIGHT; ++i) {
        for(int j = 0; j < MAP_WIDTH; ++j) {
            if(map[i][j]) {
                valid_positions.emplace_back(j, i);
            }
        }
    }

    return valid_positions;
}
