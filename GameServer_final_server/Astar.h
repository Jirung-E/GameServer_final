#pragma once

#include <optional>
#include <vector>


std::optional<std::pair<short, short>> aStarNextStep(
    const std::vector<std::pair<short, short>>& valid_positions,
    short start_x, short start_y, short goal_x, short goal_y
);