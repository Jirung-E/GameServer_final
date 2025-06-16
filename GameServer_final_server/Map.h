#pragma once

#include "game_header.h"

#include <array>
#include <vector>


enum class MapContent: char {
    Empty = ' ',
    Spawn = 'S',
    Obstacle = '#',
    Water = 'W',
    Invalid = 'X' // 유효하지 않은 위치
};


class Map {
public:


private:
    std::array<std::array<MapContent, MAP_WIDTH>, MAP_HEIGHT> map;

public:
    Map();

public:
    MapContent get(short x, short y) const;
    bool isValidPosition(short x, short y) const;

    std::vector<std::pair<short, short>> getContentPositions(MapContent content, short x, short y, short range) const;
    std::vector<std::pair<short, short>> getValidPositions(short x, short y, short range) const;
    std::vector<std::pair<short, short>> getEmptyPositions(short x, short y, short range) const;
    std::vector<std::pair<short, short>> getSpawnPositions(short x, short y, short range) const;

    //std::vector<std::pair<short, short>> getValidPositions() const;
    std::vector<std::pair<short, short>> getEmptyPositions() const;
    std::vector<std::pair<short, short>> getSpawnPositions() const;

    //std::pair<short, short> getRandomValidPosition() const;
    std::pair<short, short> getRandomEmptyPosition() const;
    std::pair<short, short> getRandomSpawnPosition() const;
};