// boj2178 - 미로 탐색

// 평범한 격자 그래프 + BFS 문제이다.

// 격자 그래프에서는 상 / 하 / 좌 / 우로밖에 움직일 수 없다.
// 즉, 다음 노드 : 현재 노드에서 상 / 하 / 좌 / 우로 움직이는 것 이라고 할 수 있다.

// 그리고 이게 그래프가 0 / 1로 구성되어있으니까,
// graph에서 0이면 밖으로 나간 것과 동일하게 생각하고,
// visited에서 -1(초기값)이 아니면 이미 방문한 걸로 생각하면 될 듯?

#include <vector>
#include <queue>
#include <iostream>

std::vector<std::pair<int, int>> moves = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

int N, M;

void BFS(const std::vector<std::vector<int>>& graph) {
    std::vector<std::vector<int>> visited(N, std::vector<int>(M, -1));
    std::queue<std::pair<int, int>> q;

    visited[0][0] = 1;
    q.push({0, 0});

    while (!q.empty()) {
        auto cur_info = q.front();
        q.pop();

        for (auto [x, y] : moves) {
            int next_x = cur_info.first + x;
            int next_y = cur_info.second + y;

            if (next_x < 0 || next_x >= M || next_y < 0 || next_y >= N || graph[next_y][next_x] == 0) {
                continue;
            }

            if (visited[next_y][next_x] == -1) {
                visited[next_y][next_x] = visited[cur_info.second][cur_info.first] + 1;
                q.push({next_x, next_y});
            }
        }
    }

    std::cout << visited[N-1][M-1] << '\n';

}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cin >> N >> M;

    std::vector<std::vector<int>> graph(N, std::vector<int>(M, 0));

    for (int i = 0; i < N; i++) {
        std::string line;
        std::cin >> line;

        for (int j = 0; j < M; j++) {
            graph[i][j] = line[j] - '0';
        }

    }

    BFS(graph);

    return 0;
}