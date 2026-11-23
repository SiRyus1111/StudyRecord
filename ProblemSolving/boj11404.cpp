// boj11404 - 플로이드

// 플로이드-워셜 알고리즘을 구현하면 되는 문제이다.
// 이건 그래프를 인접 리스트가 아니라 인접 행렬로 구현을 해야한다.
// 어렵겠다..

#include <iostream>
#include <vector>

const long long INF = 1LL << 40;

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int n, m;

    std::cin >> n >> m;

    std::vector<std::vector<long long>> graph(n + 1, std::vector<long long>(n + 1, INF));

    for (int i = 0; i < m; i++) {
        int a, b, c;
        std::cin >> a >> b >> c;
        
        if (graph[a][b] > c) {
            graph[a][b] = c;
        }
        
    }

    for (int k = 1; k <= n; k++) {
        for (int i = 1; i <= n; i++) {
            if (graph[i][k] == INF) {
                continue;
            }

            for (int j = 1; j <= n; j++) {
                if (graph[k][j] == INF || i == j) {
                    continue;
                }

                graph[i][j] = std::min(graph[i][j], graph[i][k] + graph[k][j]);
            }
        }
    }

    for (int i = 1; i <= n; i++) {
        for (int j = 1; j <= n; j++) {
            std::cout << ((graph[i][j] == INF) ? 0 : graph[i][j]) << ' ';
        }
        std::cout << '\n';
    }

    return 0;
}