#pragma once

#include <optional>
#include <vector>


std::optional<std::pair<short, short>> aStarNextStep(
    short start_x, short start_y, short goal_x, short goal_y
);