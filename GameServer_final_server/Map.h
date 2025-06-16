#pragma once

#include "game_header.h"

#include <array>
#include <vector>


class Map {
private:
    // bitset���� �ϴ°ź��� �޸� ��뷮�� ���Ƶ� ������ ����
    std::array<std::array<bool, MAP_WIDTH>, MAP_HEIGHT> map;

public:
    Map();

public:
    bool isValidPosition(int x, int y) const;

    std::vector<std::pair<int, int>> getValidPositions(int x, int y, int range) const;
    std::vector<std::pair<int, int>> getValidPositions() const;

    std::pair<int, int> getRandomValidPosition() const;
};