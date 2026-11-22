// boj1753 - 최단경로

// 그냥 다익스트라를 구현하는 문제이다.
// 어떻게 하더라.. 기억이 가물가물하네..
// 결국 내가 기존에 짜놨던 파이썬 코드 보고 한 번,
// 코드 다 지우고 내 힘으로 한 번 짜봤음..

#include <iostream>
#include <queue>
#include <vector>
#include <functional>

int INF = 1 << 30;

std::vector<int> Dijkstra(std::vector<std::vector<std::pair<int, int>>> graph, int start) {
    std::vector<int> dist(graph.size() + 1, INF);
    std::priority_queue<std::pair<int, int>, 
    std::vector<std::pair<int, int>>, 
    std::greater<std::pair<int, int>>> pq;

    dist[start] = 0;
    pq.push({0, start});

    while (!pq.empty()) {
        auto cur_info = pq.top();
        pq.pop();
        int cur_weight = cur_info.first;
        int cur_node = cur_info.second;

        if (dist[cur_node] < cur_weight) {
            continue;
        }

        for (auto [next_weight, next_node] : graph[cur_node]) {
            int w = cur_weight + next_weight;
            if (w < dist[next_node]) {
                dist[next_node] = w;
                pq.push({w, next_node});
            }
        }
    }

    return dist;

}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int V, E, K;

    std::cin >> V >> E >> K;

    std::vector<std::vector<std::pair<int, int>>> graph(V + 1);

    for (int i = 0; i < E; i++) {
        int u, v, w;
        std::cin >> u >> v >> w;
        graph[u].push_back({w, v});
    }

    std::vector<int> dist = Dijkstra(graph, K);

    for (int i = 1; i < V + 1; i++) {
        int distance = dist[i];
        if (distance == INF) {
            std::cout << "INF\n";
        }
        else {
            std::cout << distance << '\n';
        }
    }

    return 0;


}