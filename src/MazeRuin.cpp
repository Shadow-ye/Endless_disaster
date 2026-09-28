#include "MazeRuin.h"

#include <algorithm>
#include <queue>
#include <vector>

namespace {

bool oddPath(int v) {
    return (v & 1) == 1 && v >= 1 && v <= 31;
}

bool plazaTile(int x, int y) {
    return x >= MazeRuin::kPlaza0 && x <= MazeRuin::kPlaza1 && y >= MazeRuin::kPlaza0 && y <= MazeRuin::kPlaza1;
}

int localId(int x, int y) {
    return y * MazeRuin::kSize + x;
}

}  // namespace

void MazeRuin::clear() {
    active = false;
    phase = Phase::None;
    bossDead = false;
    enteredPlaza = false;
    cooldown = 0.f;
    arrive = 0.f;
    pulseR = -1.f;
    pulseHit = false;
    attackStep = 0;
    attackCd = 1.2f;
    wall.clear();
    route.clear();
}

bool MazeRuin::generate(int originTileX, int originTileY, uint32_t mazeSeed) {
    originX = originTileX;
    originY = originTileY;
    seed = mazeSeed == 0 ? 1u : mazeSeed;
    bossDead = false;
    enteredPlaza = false;
    arrive = 0.f;
    pulseR = -1.f;
    pulseHit = false;
    attackStep = 0;
    attackCd = 1.2f;
    wall.assign(kSize * kSize, 1);
    route.assign(kSize * kSize, 0);

    uint32_t rng = seed;
    auto next = [&]() {
        rng = mixHash(rng + 0x9e3779b9u);
        return rng;
    };
    auto shuffle = [&](std::vector<int>& values) {
        for (int i = int(values.size()) - 1; i > 0; --i) {
            const int j = int(next() % uint32_t(i + 1));
            std::swap(values[i], values[j]);
        }
    };

    struct Cell {
        int x;
        int y;
    };
    std::vector<Cell> cells;
    std::vector<int> id(kSize * kSize, -1);
    for (int y = 1; y <= 31; y += 2) {
        for (int x = 1; x <= 31; x += 2) {
            if (plazaTile(x, y)) {
                continue;
            }
            id[localId(x, y)] = int(cells.size());
            cells.push_back({x, y});
        }
    }
    const int plaza = int(cells.size());
    const int nodeCount = plaza + 1;
    std::vector<std::vector<int>> adj(nodeCount);
    const int dirs[4][2] = {{2, 0}, {-2, 0}, {0, 2}, {0, -2}};
    for (int i = 0; i < plaza; ++i) {
        for (const auto& d : dirs) {
            const int nx = cells[i].x + d[0];
            const int ny = cells[i].y + d[1];
            if (!oddPath(nx) || !oddPath(ny)) {
                continue;
            }
            if (plazaTile(nx, ny)) {
                adj[i].push_back(plaza);
                adj[plaza].push_back(i);
                continue;
            }
            const int j = id[localId(nx, ny)];
            if (j > i) {
                adj[i].push_back(j);
                adj[j].push_back(i);
            }
        }
    }
    for (auto& nbrs : adj) {
        shuffle(nbrs);
    }

    std::vector<char> seen(plaza, 0);
    std::vector<int> cursor(plaza, 0);
    std::vector<std::pair<int, int>> tree;
    std::vector<int> stack;
    stack.push_back(0);
    seen[0] = 1;
    while (!stack.empty()) {
        const int u = stack.back();
        if (cursor[u] >= int(adj[u].size())) {
            stack.pop_back();
            continue;
        }
        const int v = adj[u][cursor[u]++];
        if (v < 0 || v >= plaza || seen[v]) {
            continue;
        }
        seen[v] = 1;
        tree.push_back({u, v});
        stack.push_back(v);
    }
    for (int i = 0; i < plaza; ++i) {
        if (!seen[i]) {
            active = false;
            return false;
        }
    }
    std::vector<int> gates;
    for (int i = 0; i < plaza; ++i) {
        for (int v : adj[i]) {
            if (v == plaza) {
                gates.push_back(i);
                break;
            }
        }
    }
    if (gates.empty()) {
        active = false;
        return false;
    }
    shuffle(gates);
    tree.push_back({gates.front(), plaza});
    auto carve = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < kSize && y < kSize) {
            wall[localId(x, y)] = 0;
        }
    };
    for (const auto& edge : tree) {
        int a = edge.first;
        int b = edge.second;
        if (a == plaza || b == plaza) {
            const int cell = a == plaza ? b : a;
            const int cx = cells[cell].x;
            const int cy = cells[cell].y;
            int dx = 0;
            int dy = 0;
            if (cx < kPlaza0) {
                dx = 1;
            } else if (cx > kPlaza1) {
                dx = -1;
            } else if (cy < kPlaza0) {
                dy = 1;
            } else if (cy > kPlaza1) {
                dy = -1;
            }
            carve(cx, cy);
            carve(cx + dx, cy + dy);
            doorX = cx + dx;
            doorY = cy + dy;
            continue;
        }
        const int ax = cells[a].x;
        const int ay = cells[a].y;
        const int bx = cells[b].x;
        const int by = cells[b].y;
        carve(ax, ay);
        carve(bx, by);
        carve((ax + bx) / 2, (ay + by) / 2);
    }
    for (int y = kPlaza0; y <= kPlaza1; ++y) {
        for (int x = kPlaza0; x <= kPlaza1; ++x) {
            carve(x, y);
        }
    }

    std::vector<int> border;
    for (int i = 0; i < plaza; ++i) {
        const int x = cells[i].x;
        const int y = cells[i].y;
        if (x == 1 || x == 31 || y == 1 || y == 31) {
            border.push_back(i);
        }
    }
    if (border.empty()) {
        active = false;
        return false;
    }
    const int entranceCell = border[int(next() % uint32_t(border.size()))];
    const int ex = cells[entranceCell].x;
    const int ey = cells[entranceCell].y;
    const bool openX = ex == 1 || ex == 31;
    const bool openY = ey == 1 || ey == 31;
    bool useX = openX;
    if (openX && openY) {
        useX = (next() & 1u) != 0u;
    }
    if (useX) {
        entranceX = ex == 1 ? 0 : kSize - 1;
        entranceY = ey;
    } else {
        entranceX = ex;
        entranceY = ey == 1 ? 0 : kSize - 1;
    }
    carve(entranceX, entranceY);

    std::vector<std::vector<int>> treeAdj(nodeCount);
    for (const auto& edge : tree) {
        treeAdj[edge.first].push_back(edge.second);
        treeAdj[edge.second].push_back(edge.first);
    }
    std::vector<int> parent(nodeCount, -1);
    std::queue<int> pending;
    parent[entranceCell] = entranceCell;
    pending.push(entranceCell);
    while (!pending.empty()) {
        const int u = pending.front();
        pending.pop();
        if (u == plaza) {
            break;
        }
        for (int v : treeAdj[u]) {
            if (parent[v] >= 0) {
                continue;
            }
            parent[v] = u;
            pending.push(v);
        }
    }
    if (parent[plaza] < 0) {
        active = false;
        return false;
    }

    auto mark = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < kSize && y < kSize) {
            route[localId(x, y)] = 1;
        }
    };
    std::vector<int> chain;
    for (int cur = plaza; cur >= 0; cur = parent[cur]) {
        chain.push_back(cur);
        if (cur == entranceCell) {
            break;
        }
        if (parent[cur] == cur) {
            break;
        }
    }
    std::reverse(chain.begin(), chain.end());
    mark(entranceX, entranceY);
    for (int i = 0; i < int(chain.size()); ++i) {
        const int node = chain[i];
        if (node == plaza) {
            break;
        }
        mark(cells[node].x, cells[node].y);
        if (i + 1 >= int(chain.size())) {
            break;
        }
        const int nextNode = chain[i + 1];
        if (nextNode == plaza) {
            mark(doorX, doorY);
            int x = doorX;
            int y = doorY;
            while (x != kCenter) {
                x += x < kCenter ? 1 : -1;
                mark(x, y);
            }
            while (y != kCenter) {
                y += y < kCenter ? 1 : -1;
                mark(x, y);
            }
            break;
        }
        const int mx = (cells[node].x + cells[nextNode].x) / 2;
        const int my = (cells[node].y + cells[nextNode].y) / 2;
        mark(mx, my);
    }

    if (!audit()) {
        active = false;
        wall.clear();
        route.clear();
        return false;
    }
    active = true;
    return true;
}

bool MazeRuin::audit() const {
    if (wall.size() != size_t(kSize * kSize)) {
        return false;
    }
    int entrances = 0;
    for (int i = 0; i < kSize; ++i) {
        if (wall[localId(i, 0)] == 0) {
            entrances += 1;
        }
        if (wall[localId(i, kSize - 1)] == 0) {
            entrances += 1;
        }
        if (i > 0 && i < kSize - 1 && wall[localId(0, i)] == 0) {
            entrances += 1;
        }
        if (i > 0 && i < kSize - 1 && wall[localId(kSize - 1, i)] == 0) {
            entrances += 1;
        }
    }
    if (entrances != 1 || wall[localId(entranceX, entranceY)] != 0) {
        return false;
    }
    int doors = 0;
    for (int y = kPlaza0; y <= kPlaza1; ++y) {
        if (wall[localId(kPlaza0 - 1, y)] == 0) {
            doors += 1;
        }
        if (wall[localId(kPlaza1 + 1, y)] == 0) {
            doors += 1;
        }
    }
    for (int x = kPlaza0; x <= kPlaza1; ++x) {
        if (wall[localId(x, kPlaza0 - 1)] == 0) {
            doors += 1;
        }
        if (wall[localId(x, kPlaza1 + 1)] == 0) {
            doors += 1;
        }
    }
    if (doors != 1 || wall[localId(doorX, doorY)] != 0) {
        return false;
    }
    for (int y = kPlaza0; y <= kPlaza1; ++y) {
        for (int x = kPlaza0; x <= kPlaza1; ++x) {
            if (wall[localId(x, y)] != 0) {
                return false;
            }
        }
    }

    auto floorOutside = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= kSize || y >= kSize) {
            return false;
        }
        if (plazaTile(x, y)) {
            return false;
        }
        return wall[localId(x, y)] == 0;
    };
    const int nbrs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    struct Frame {
        int x;
        int y;
        int next;
    };
    int paths = 0;
    std::vector<char> seen(kSize * kSize, 0);
    std::vector<Frame> walk;
    walk.push_back({entranceX, entranceY, 0});
    seen[localId(entranceX, entranceY)] = 1;
    while (!walk.empty() && paths < 2) {
        Frame& frame = walk.back();
        if (frame.x == doorX && frame.y == doorY) {
            paths += 1;
            seen[localId(frame.x, frame.y)] = 0;
            walk.pop_back();
            continue;
        }
        if (frame.next >= 4) {
            seen[localId(frame.x, frame.y)] = 0;
            walk.pop_back();
            continue;
        }
        const int nx = frame.x + nbrs[frame.next][0];
        const int ny = frame.y + nbrs[frame.next][1];
        frame.next += 1;
        if (!floorOutside(nx, ny) || seen[localId(nx, ny)]) {
            continue;
        }
        seen[localId(nx, ny)] = 1;
        walk.push_back({nx, ny, 0});
    }
    return paths == 1;
}

bool MazeRuin::contains(int tileX, int tileY) const {
    if (!active) {
        return false;
    }
    const int lx = tileX - originX;
    const int ly = tileY - originY;
    return lx >= 0 && ly >= 0 && lx < kSize && ly < kSize;
}

bool MazeRuin::isWallAt(int tileX, int tileY) const {
    if (!contains(tileX, tileY) || wall.size() != size_t(kSize * kSize)) {
        return false;
    }
    return wall[localId(tileX - originX, tileY - originY)] != 0;
}

bool MazeRuin::inPlaza(int tileX, int tileY) const {
    if (!contains(tileX, tileY)) {
        return false;
    }
    const int lx = tileX - originX;
    const int ly = tileY - originY;
    return plazaTile(lx, ly);
}

bool MazeRuin::onRoute(int localX, int localY) const {
    if (localX < 0 || localY < 0 || localX >= kSize || localY >= kSize) {
        return false;
    }
    if (route.size() != size_t(kSize * kSize)) {
        return false;
    }
    return route[localId(localX, localY)] != 0;
}

bool MazeRuin::isPlazaRock(int localX, int localY) {
    return (localX == 12 && localY == 12) || (localX == 20 && localY == 12)
        || (localX == 12 && localY == 20) || (localX == 20 && localY == 20);
}

float MazeRuin::centerX() const {
    return (float(originX) + float(kCenter) + 0.5f) * float(kTile);
}

float MazeRuin::centerY() const {
    return (float(originY) + float(kCenter) + 0.5f) * float(kTile);
}

float MazeRuin::entranceWorldX() const {
    return (float(originX + entranceX) + 0.5f) * float(kTile);
}

float MazeRuin::entranceWorldY() const {
    return (float(originY + entranceY) + 0.5f) * float(kTile);
}
