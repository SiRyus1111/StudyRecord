// 백준 7576 - 토마토

// 그냥 시작점이 여러 곳인 BFS 문제이다.
// 시작점이 여러 개인건 무시하고 그냥 모든 시작점을 처음에 큐에 넣은 후 BFS를 돌리면 된다.

// visited[next] = visited[prev] + 1로 하면 된다.(실제 인덱스는 저렇지 않음. 그냥 편의상 next / prev 쓴거임)

#include <iostream>
#include <vector>
#include <queue>
#include <algorithm>

std::vector<std::pair<int, int>> moves = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

std::vector<std::vector<int>> BFS(const std::vector<std::vector<int>>& graph, const std::vector<std::pair<int, int>>& start) {
    std::queue<std::pair<int, int>> q;
    std::vector<std::vector<int>> visited(graph.size(), std::vector<int>(graph[0].size(), -1));

    for (auto [y, x] : start) {
        q.push({y, x});
        visited[y][x] = 0;
    }

    while (!q.empty()) {
        auto info = q.front();
        int cur_y = info.first;
        int cur_x = info.second;
        q.pop();

        for (auto [y, x] : moves) {
            int next_y = cur_y + y;
            int next_x = cur_x + x;
            if (next_x >= 0 && next_x < graph[0].size() && next_y >= 0 && next_y < graph.size() && graph[next_y][next_x] == 0) {
                if (visited[next_y][next_x] == -1) {
                    visited[next_y][next_x] = visited[cur_y][cur_x] + 1;
                    q.push({next_y, next_x});
                }
            }
        }
    }

    return visited;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int M, N;

    std::cin >> M >> N;

    std::vector<std::vector<int>> graph(N, std::vector<int>(M, 0));
    std::vector<std::pair<int, int>> start;

    for(int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            int num;
            std::cin >> num;
            graph[i][j] = num;
            if (num == 1) {
                start.push_back({i, j});
            }
        }
    }

    auto visited = BFS(graph, start);

    int max = 0;

    for (int i = 0; i < visited.size(); i++) {
        for (int j = 0; j < visited[0].size(); j++) {
            if (graph[i][j] == 0 && visited[i][j] == -1) {
                std::cout << -1 << '\n';
                return 0;
            }
            max = std::max(visited[i][j], max);
        }
    }

    std::cout << max << '\n';

    return 0;
}