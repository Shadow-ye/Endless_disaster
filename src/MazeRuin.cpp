#include "MazeRuin.h"

#include <algorithm>
#include <queue>
#include <vector>

// ============================================================================
// 迷宫生成 + 唯一最短路径搜索
//
// 局部坐标约定：wall / route 都是 kSize×kSize（33×33）的行优先位图，
// 下标 = y * kSize + x；值为 1 表示墙（不可通行），0 表示地板。
//
// 整体流程：
//   建图 → 洗牌邻接表 → 迭代 DFS 抽生成树 → 广场开单门 → 凿墙
//   → 外墙开唯一入口 → 树上 BFS 求唯一通路 → 标记 route → audit 自检
//
// 关键性质：通路取自一棵无环的生成树，因此「入口 → 广场」的路线在结构上
// 必然唯一且最短，这正是第 5 节「唯一解法路径」的来源。
// ============================================================================
namespace {

// 通道节点只允许落在奇数坐标（1,3,…,31）上：这样偶数行/列天然就是「还没凿开的墙」，
// 两个相邻通道格之间必定隔着一格墙，凿通它才构成一步通路。
bool oddPath(int v) {
    return (v & 1) == 1 && v >= 1 && v <= 31;
}

// 是否属于中央广场（15×15，kPlaza0..kPlaza1）。
// 广场在建图时被压缩成一个「超级节点」，凿墙时再整块铺平成地板。
bool plazaTile(int x, int y) {
    return x >= MazeRuin::kPlaza0 && x <= MazeRuin::kPlaza1 && y >= MazeRuin::kPlaza0 && y <= MazeRuin::kPlaza1;
}

// 局部坐标 (x, y) → 一维数组下标（行优先）
int localId(int x, int y) {
    return y * MazeRuin::kSize + x;
}

}  // namespace

// 复位迷宫的全部运行时状态，并释放 wall / route 位图。
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

// 生成一座迷宫遗迹。
// 参数：originTileX/originTileY 为迷宫左上角在全局瓦片坐标中的位置；
//       mazeSeed 为随机种子（0 视为 1），同一个 seed 必然生成同一座迷宫（可复现）。
// 返回：true = 生成成功且通过 audit 自检；false = 本次随机结果不合格，
//       调用方（Session::spawnRuin）会换中心点 / 种子重试。
bool MazeRuin::generate(int originTileX, int originTileY, uint32_t mazeSeed) {
    // ---- 0. 状态复位：wall / route 先整体清零，wall 随后由树边逐格凿通 ----
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

    // 种子化 PRNG：黄金比例增量 + mixHash 混合，与 Session::nextRand 同一套写法
    uint32_t rng = seed;
    auto next = [&]() {
        rng = mixHash(rng + 0x9e3779b9u);
        return rng;
    };
    // Fisher–Yates 洗牌：随机化邻接表（决定 DFS 生成树形态）、候选门位、外墙入口格
    auto shuffle = [&](std::vector<int>& values) {
        for (int i = int(values.size()) - 1; i > 0; --i) {
            const int j = int(next() % uint32_t(i + 1));
            std::swap(values[i], values[j]);
        }
    };

    // ---- 1. 建图：把每个奇数坐标的通道格建成一个图节点，广场压缩成超级节点 ----
    struct Cell {
        int x;
        int y;
    };
    std::vector<Cell> cells;                 // 节点编号 → 局部坐标
    std::vector<int> id(kSize * kSize, -1);  // 局部坐标 → 节点编号（-1 表示不是节点）
    for (int y = 1; y <= 31; y += 2) {
        for (int x = 1; x <= 31; x += 2) {
            if (plazaTile(x, y)) {
                continue;  // 广场内的格子不单独建节点，统一由 plaza 超级节点代表
            }
            id[localId(x, y)] = int(cells.size());
            cells.push_back({x, y});
        }
    }
    const int plaza = int(cells.size());  // 广场超级节点的编号（排在所有普通节点之后）
    const int nodeCount = plaza + 1;
    std::vector<std::vector<int>> adj(nodeCount);  // 无向图邻接表
    // 步长 2 的四个方向：两个节点中间必定隔着「一格待凿的墙」
    const int dirs[4][2] = {{2, 0}, {-2, 0}, {0, 2}, {0, -2}};
    for (int i = 0; i < plaza; ++i) {
        for (const auto& d : dirs) {
            const int nx = cells[i].x + d[0];
            const int ny = cells[i].y + d[1];
            if (!oddPath(nx) || !oddPath(ny)) {
                continue;  // 越界或落到偶数坐标，构不成通道格
            }
            if (plazaTile(nx, ny)) {
                // 邻居落在广场内：与广场超级节点双向连边（此时只登记候选，DFS 不会走它）
                adj[i].push_back(plaza);
                adj[plaza].push_back(i);
                continue;
            }
            const int j = id[localId(nx, ny)];
            // 只允许小编号连大编号，保证同一条无向边只加一次
            if (j > i) {
                adj[i].push_back(j);
                adj[j].push_back(i);
            }
        }
    }
    // 洗牌邻接表：DFS 的选边顺序随机 ⇒ 生成树形态随机 ⇒ 每座迷宫长得都不一样
    for (auto& nbrs : adj) {
        shuffle(nbrs);
    }

    // ---- 2. 迭代式随机 DFS（递归回溯法）抽生成树 ----
    // cursor[u] = 节点 u 下一条待试探的边下标。它替代了递归版本的循环变量，
    // 让「回溯到 u」时能从上次的位置继续，避免重复扫描同一条边。
    std::vector<char> seen(plaza, 0);      // 普通节点是否已入树（广场不参与本阶段）
    std::vector<int> cursor(plaza, 0);
    std::vector<std::pair<int, int>> tree;  // 生成树的边集合
    std::vector<int> stack;                 // 显式栈，替代递归
    stack.push_back(0);
    seen[0] = 1;
    while (!stack.empty()) {
        const int u = stack.back();
        if (cursor[u] >= int(adj[u].size())) {
            stack.pop_back();  // u 的所有边都试过了，回溯到父节点
            continue;
        }
        const int v = adj[u][cursor[u]++];
        // v >= plaza 即广场节点：故意跳过，留到第 3 步单独接唯一一道门
        if (v < 0 || v >= plaza || seen[v]) {
            continue;  // 非法下标 / 广场 / 已入树，跳过这条边
        }
        seen[v] = 1;                 // 选边成功：v 入树
        tree.push_back({u, v});
        stack.push_back(v);          // 立即深入 v
    }
    // 连通性兜底：普通节点应全部被访问到，否则说明建图有问题，直接放弃本次生成
    for (int i = 0; i < plaza; ++i) {
        if (!seen[i]) {
            active = false;
            return false;
        }
    }

    // ---- 3. 广场开单门：在「与广场相邻」的节点里随机挑一个，只接一条树边 ----
    // 这是「进广场有且仅有一条通路」的算法保证：广场在生成树上的度恒为 1。
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
        active = false;  // 没有任何格子贴着广场，无法开门
        return false;
    }
    shuffle(gates);
    tree.push_back({gates.front(), plaza});  // 只取洗牌后的第一个候选

    // ---- 4. 凿墙：把生成树的每条边翻译成 wall 位图上的可通行格 ----
    auto carve = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < kSize && y < kSize) {
            wall[localId(x, y)] = 0;  // 越界直接忽略，边界处理不散落到调用处
        }
    };
    for (const auto& edge : tree) {
        int a = edge.first;
        int b = edge.second;
        if (a == plaza || b == plaza) {
            // 广场边：从贴着广场的通道格朝广场方向推一格，凿出唯一的「门」
            const int cell = a == plaza ? b : a;
            const int cx = cells[cell].x;
            const int cy = cells[cell].y;
            int dx = 0;
            int dy = 0;
            if (cx < kPlaza0) {
                dx = 1;   // 格子位于广场左侧 → 向右推
            } else if (cx > kPlaza1) {
                dx = -1;  // 位于右侧 → 向左推
            } else if (cy < kPlaza0) {
                dy = 1;   // 位于上方 → 向下推
            } else if (cy > kPlaza1) {
                dy = -1;  // 位于下方 → 向上推
            }
            carve(cx, cy);
            carve(cx + dx, cy + dy);
            doorX = cx + dx;
            doorY = cy + dy;  // 记录门坐标，供 route 标记与 audit 使用
            continue;
        }
        // 普通树边 a—b：两端通道格 + 正中间那格墙（两点间距为 2，中点必是墙）
        const int ax = cells[a].x;
        const int ay = cells[a].y;
        const int bx = cells[b].x;
        const int by = cells[b].y;
        carve(ax, ay);
        carve(bx, by);
        carve((ax + bx) / 2, (ay + by) / 2);
    }
    // 广场整块（15×15）铺平成地板，中心留作 boss 场地
    for (int y = kPlaza0; y <= kPlaza1; ++y) {
        for (int x = kPlaza0; x <= kPlaza1; ++x) {
            carve(x, y);
        }
    }

    // ---- 5. 外墙开唯一入口：在贴外墙的通道格里随机选一个，向外凿穿一格 ----
    std::vector<int> border;
    for (int i = 0; i < plaza; ++i) {
        const int x = cells[i].x;
        const int y = cells[i].y;
        if (x == 1 || x == 31 || y == 1 || y == 31) {
            border.push_back(i);  // 位于最外圈一层通道格
        }
    }
    if (border.empty()) {
        active = false;
        return false;
    }
    const int entranceCell = border[int(next() % uint32_t(border.size()))];  // 等概率抽一个
    const int ex = cells[entranceCell].x;
    const int ey = cells[entranceCell].y;
    const bool openX = ex == 1 || ex == 31;  // 该格贴到左右外墙
    const bool openY = ey == 1 || ey == 31;  // 该格贴到上下外墙
    bool useX = openX;
    if (openX && openY) {
        // 角落格子同时贴两条外墙：用一个随机位决定从哪条边开
        useX = (next() & 1u) != 0u;
    }
    if (useX) {
        entranceX = ex == 1 ? 0 : kSize - 1;  // 推到 x=0 或 x=32 的外墙格
        entranceY = ey;
    } else {
        entranceX = ex;
        entranceY = ey == 1 ? 0 : kSize - 1;  // 推到 y=0 或 y=32 的外墙格
    }
    carve(entranceX, entranceY);

    // ---- 6. 求解唯一最短路径：在生成树上从入口 BFS 到广场 ----
    // 图是树（无环），所以 BFS 得到的这条路径既是最短路径、也是唯一路径；
    // 父指针 parent[] 同时用于下面的 route 标注与唯一性说明。
    std::vector<std::vector<int>> treeAdj(nodeCount);  // 只含树边的邻接表（无环）
    for (const auto& edge : tree) {
        treeAdj[edge.first].push_back(edge.second);
        treeAdj[edge.second].push_back(edge.first);
    }
    std::vector<int> parent(nodeCount, -1);  // -1 = 未访问，入口指向自己表示根
    std::queue<int> pending;
    parent[entranceCell] = entranceCell;
    pending.push(entranceCell);
    while (!pending.empty()) {
        const int u = pending.front();
        pending.pop();
        if (u == plaza) {
            break;  // 已到广场，无需再扩展（BFS 保证此时路径最短）
        }
        for (int v : treeAdj[u]) {
            if (parent[v] >= 0) {
                continue;  // 树上已访问过，跳过
            }
            parent[v] = u;
            pending.push(v);
        }
    }
    if (parent[plaza] < 0) {
        active = false;  // 入口与广场不连通，理论上不该发生
        return false;
    }

    // ---- 7. 标记唯一解法路径 route：供渲染 / 雷达 / 迷宫小地图使用 ----
    auto mark = [&](int x, int y) {
        if (x >= 0 && y >= 0 && x < kSize && y < kSize) {
            route[localId(x, y)] = 1;  // 1 = 位于唯一通路上
        }
    };
    // 沿父指针从广场回溯到入口（parent[入口] == 入口 时终止），再翻转成「入口 → 广场」
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
    mark(entranceX, entranceY);  // 先标记穿透外墙的那一格
    for (int i = 0; i < int(chain.size()); ++i) {
        const int node = chain[i];
        if (node == plaza) {
            break;  // 广场整块都是地板，无需逐格标记
        }
        mark(cells[node].x, cells[node].y);  // 通道格本身
        if (i + 1 >= int(chain.size())) {
            break;
        }
        const int nextNode = chain[i + 1];
        if (nextNode == plaza) {
            // 链尾接的是广场：先标门，再按「先水平、后垂直」的折线走到广场中心
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
        // 两个通道格之间隔着一格墙，那一格在凿墙阶段已打通，同样标进通路
        const int mx = (cells[node].x + cells[nextNode].x) / 2;
        const int my = (cells[node].y + cells[nextNode].y) / 2;
        mark(mx, my);
    }

    // ---- 8. 生成即验证：自检不过就丢弃整座迷宫，交给调用方重试 ----
    if (!audit()) {
        active = false;
        wall.clear();
        route.clear();
        return false;
    }
    active = true;
    return true;
}

// 生成后的唯一性 / 合法性自检。四项全部通过才算一座合格迷宫：
//   1) 外墙恰好 1 个入口，且与记录的 entranceX/Y 一致；
//   2) 广场四周恰好 1 道门，且与记录的 doorX/Y 一致；
//   3) 广场内部（15×15）全为地板；
//   4) 从入口到门的简单路径恰好 1 条（数到第 2 条立刻判失败）。
// 不通过时调用方会丢弃这座迷宫并重试，因此「唯一通路」是强保证而非期望。
bool MazeRuin::audit() const {
    if (wall.size() != size_t(kSize * kSize)) {
        return false;  // 位图尺寸不对，说明还没生成
    }
    // ---- 检查 1：统计四条外墙上被打通的格子数，必须恰好为 1 ----
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
        return false;  // 入口不唯一，或记录坐标上并没有真的打通
    }
    // ---- 检查 2：扫描广场外一圈边框格，统计可通行格（门）数量，必须恰好为 1 ----
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
        return false;  // 门不唯一，或记录坐标上并没有真的打通
    }
    // ---- 检查 3：广场内部不允许残留任何墙 ----
    for (int y = kPlaza0; y <= kPlaza1; ++y) {
        for (int x = kPlaza0; x <= kPlaza1; ++x) {
            if (wall[localId(x, y)] != 0) {
                return false;
            }
        }
    }

    // ---- 检查 4：入口 → 门 的简单路径计数（唯一最短路径的核心验证） ----
    // 只把「广场外的地板」当作可走格：门是广场边界，路径在门外就算到达终点，
    // 广场内部不参与搜索，避免绕广场一圈被误算成多条路径。
    auto floorOutside = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= kSize || y >= kSize) {
            return false;  // 越界
        }
        if (plazaTile(x, y)) {
            return false;  // 广场内部不参与本阶段搜索
        }
        return wall[localId(x, y)] == 0;
    };
    const int nbrs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};  // 4 邻域，步长 1
    // 显式栈的一帧：当前格 + 下一个待试探方向的下标（0..3）
    struct Frame {
        int x;
        int y;
        int next;
    };
    int paths = 0;                                  // 已完成的简单路径条数
    std::vector<char> seen(kSize * kSize, 0);       // 路径内去重（回溯时撤销）
    std::vector<Frame> walk;
    walk.push_back({entranceX, entranceY, 0});
    seen[localId(entranceX, entranceY)] = 1;
    while (!walk.empty() && paths < 2) {  // 数到 2 条即可断定不唯一，提前退出
        Frame& frame = walk.back();
        if (frame.x == doorX && frame.y == doorY) {
            // 走通一条完整路径：计数 + 撤销现场 + 回溯继续找第二条
            paths += 1;
            seen[localId(frame.x, frame.y)] = 0;
            walk.pop_back();
            continue;
        }
        if (frame.next >= 4) {
            // 四个方向都试完，回溯（同时撤销 visited 标记，恢复现场）
            seen[localId(frame.x, frame.y)] = 0;
            walk.pop_back();
            continue;
        }
        const int nx = frame.x + nbrs[frame.next][0];
        const int ny = frame.y + nbrs[frame.next][1];
        frame.next += 1;  // 注意：必须先自增游标，再判断是否深入，保证回溯后从下一方向继续
        if (!floorOutside(nx, ny) || seen[localId(nx, ny)]) {
            continue;
        }
        seen[localId(nx, ny)] = 1;
        walk.push_back({nx, ny, 0});
    }
    return paths == 1;  // 恰好一条：入口到门的通路唯一（必然也是最短）
}

bool MazeRuin::contains(int tileX, int tileY) const {
    if (!active) {
        return false;
    }
    const int lx = tileX - originX;
    const int ly = tileY - originY;
    return lx >= 0 && ly >= 0 && lx < kSize && ly < kSize;
}

bool MazeRuin::inClearZone(int tileX, int tileY) const {
    if (!active) {
        return false;
    }
    const int lx = tileX - originX;
    const int ly = tileY - originY;
    return lx >= -kClearMargin && ly >= -kClearMargin && lx < kSize + kClearMargin && ly < kSize + kClearMargin;
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
