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
    MapContent get(int x, int y) const;
    bool isValidPosition(int x, int y) const;

    std::vector<std::pair<int, int>> getContentPositions(MapContent content, int x, int y, int range) const;
    //std::vector<std::pair<int, int>> getValidPositions(int x, int y, int range) const;
    std::vector<std::pair<int, int>> getEmptyPositions(int x, int y, int range) const;
    std::vector<std::pair<int, int>> getSpawnPositions(int x, int y, int range) const;

    //std::vector<std::pair<int, int>> getValidPositions() const;
    std::vector<std::pair<int, int>> getEmptyPositions() const;
    std::vector<std::pair<int, int>> getSpawnPositions() const;

    //std::pair<int, int> getRandomValidPosition() const;
    std::pair<int, int> getRandomEmptyPosition() const;
    std::pair<int, int> getRandomSpawnPosition() const;
};