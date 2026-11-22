// boj1504 - 특정한 최단 경로

// 이거 다익스트라를 여섯 번 돌려야할 것 같지만,
// 사실 세 번이면 됨.

// 1. 1번 노드로부터
// 1-1. 첫 번째 필수 노드
// 1-2. 두 번째 필수 노드

// 2. 첫 번째 필수 노드로부터
// 2-1. 두 번째 필수 노드
// 2-2. 최종 노드

// 3. 두 번째 필수 노드로부터
// 3-1. 첫 번째 필수 노드
// 3-2. 최종 노드

// 이렇게 얻어서,

// 1-1 -> 2-1 -> 3-2
// 1-2 -> 3-1 -> 2-2

// 이렇게 두 가지로 하면 된다.

// 그리고 둘 다 INF를 넘으면,(>= INF)
// -1 출력하면 된다.

// 앞으로는 디버깅 노트를 써볼까 합니다.

// 1. 무한 대기
// pq에서 top()은 하는데 pop()을 안해서..

// 2. INF + INF + INF면 오버플로우
// long long으로 바꾸다가, 그냥 INF 값을 2억으로 낮춤.

#include <queue>
#include <iostream>
#include <vector>
#include <functional>

int INF = 200000000;

std::vector<int> Dijkstra(const std::vector<std::vector<std::pair<int, int>>>& graph, int start) {
    std::vector<int> dist(graph.size(), INF);
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

    int N, E;

    std::cin >> N >> E;

    std::vector<std::vector<std::pair<int, int>>> graph(N + 1);

    for (int i = 0; i < E; i++) {
        int a, b, c;
        std::cin >> a >> b >> c;

        graph[a].push_back({c, b});
        graph[b].push_back({c, a});
    }

    int v1, v2;
    std::cin >> v1 >> v2;

    std::vector<int> dist_from_1 = Dijkstra(graph, 1);
    std::vector<int> dist_from_v1 = Dijkstra(graph, v1);
    std::vector<int> dist_from_v2 = Dijkstra(graph, v2);

    long long path_1 = dist_from_1[v1] + dist_from_v1[v2] + dist_from_v2[N];
    long long path_2 = dist_from_1[v2] + dist_from_v2[v1] + dist_from_v1[N];

    if (path_1 >= INF && path_2 >= INF) {
        std::cout << -1 << '\n';
    }
    else if (path_1 >= path_2) {
        std::cout << path_2 << '\n';
    }
    else {
        std::cout << path_1 << '\n';
    }

    return 0;
}