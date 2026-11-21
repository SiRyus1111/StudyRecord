// boj2606 - 바이러스

// 그냥 평범한 BFS로 연결된 노드 전부 찾기 문제이다.
// 1번 노드부터 시작해서 BFS 후,
// 추가적으로 몇 개의 노드를 지나가는지를 출력하면 된다.
// 그냥 1번 노드도 같이 세어버리고 마지막에 -1하면 된다.

#include <iostream>
#include <vector>
#include <queue>

void BFS(std::vector<std::vector<int>> graph) {
    std::vector<bool> visited(graph.size() + 1, false);
    std::queue<int> q;

    int cnt = 0;

    visited[1] = true;
    q.push(1);

    while (!q.empty()) {
        int cur_node = q.front();
        q.pop();

        for (int next_node : graph[cur_node]) {
            if (visited[next_node] == false) {
                visited[next_node] = true;
                q.push(next_node);
                cnt++;
            }
        }
    }

    std::cout << cnt;
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int N;
    std::cin >> N;

    std::vector<std::vector<int>> graph(N + 1);

    int M;
    std::cin >> M;

    for (int i = 0; i < M; i++) {
        int a, b;
        std::cin >> a >> b;
        graph[a].push_back(b);
        graph[b].push_back(a);
    }

    BFS(graph);

    return 0;
}