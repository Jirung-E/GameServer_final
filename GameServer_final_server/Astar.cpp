#include "Astar.h"

#include <iostream>
#include <vector>
#include <queue>
#include <cmath>
#include <unordered_set>

#include "game_header.h"
#include "Network.h"

using namespace std;


static struct Vec2 {
    int x;
    int y;

    bool operator==(const Vec2& other) const {
        return x == other.x && y == other.y;
    }
};

// 해시 함수 정의 (unordered_set 사용용)
static struct Vec2Hash {
    size_t operator()(const Vec2& v) const {
        return hash<int>()(v.x) ^ (hash<int>()(v.y) << 1);
    }
};


static struct Node {
    Vec2 position;
    float g, h;
    float f;
    Node* parent;

    Node(Vec2 pos, float g_, float h_, Node* parent_ = nullptr): 
        position { pos }, 
        g { g_ }, 
        h { h_ }, 
        f { g_ + h_ }, 
        parent { parent_ }
    {

    }
};


// 우선순위 큐에서 f가 작은 순으로 정렬
static struct CompareNode {
    bool operator()(const Node* a, const Node* b) const {
        return a->f > b->f;
    }
};

static float heuristic(const Vec2& a, const Vec2& b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}


optional<pair<short, short>> aStarNextStep(
    short start_x, short start_y, short goal_x, short goal_y
) {
    priority_queue<Node*, vector<Node*>, CompareNode> open_list;
    unordered_set<Vec2, Vec2Hash> closed_set;

    vector<Vec2> directions { {-1,0}, {1,0}, {0,-1}, {0,1} };

    Vec2 start { start_x, start_y };
    Vec2 goal { goal_x, goal_y };

    Node* start_node = new Node(start, 0, heuristic(start, goal));
    open_list.push(start_node);

    while(!open_list.empty()) {
        Node* current = open_list.top();
        open_list.pop();

        if(current->position == goal) {
            vector<Vec2> path;
            while(current) {
                path.push_back(current->position);
                current = current->parent;
            }
            reverse(path.begin(), path.end());
            if(path.size() > 1) {
                while(!open_list.empty()) {
                    Node* node = open_list.top();
                    open_list.pop();
                    delete node; // 메모리 해제
                }
                return make_pair(path[1].x, path[1].y); // 다음 위치 반환
            }
        }

        closed_set.insert(current->position);

        for(const Vec2& dir : directions) {
            Vec2 neighbor = { current->position.x + dir.x, current->position.y + dir.y };

            if(neighbor.x < 0 || neighbor.x >= MAP_WIDTH ||
                neighbor.y < 0 || neighbor.y >= MAP_HEIGHT ||
                !Server::map.isValidPosition(neighbor.x, neighbor.y) ||
                closed_set.count(neighbor) > 0
            ) {
                continue;
            }

            float cost = sqrt(dir.x * dir.x + dir.y * dir.y);
            float g = current->g + cost;
            float h = heuristic(neighbor, goal);
            Node* neighbor_node = new Node(neighbor, g, h, current);

            open_list.push(neighbor_node);
        }
    }

    // open_list 클리어
    while(!open_list.empty()) {
        Node* node = open_list.top();
        open_list.pop();
        delete node; // 메모리 해제
    }
    return {}; // 경로 없음
}
