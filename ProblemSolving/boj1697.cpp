// boj1697 - 숨바꼭질

// 이거 솔직히 일반적으로는 DP문제인 줄 알듯..

// 하지만 이거 BFS로 풀 수 있다.
// 다음 노드를 현재 노드 * 2 / 현재 노드 - 1 / 현재 노드 + 1로 정의해놓고,
// 목적지에 도달할 때 까지 visited 배열에다가 + 1하면서 기록해놓으면 된다..

#include <iostream>
#include <vector>
#include <queue>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int N, K;

    std::cin >> N >> K;

    if (K <= N) {
        std::cout << N - K << '\n';
        return 0;
    }

    std::vector<int> visited(K * 2, -1);

    int i = N;
    std::queue<int> q;
    q.push(i);
    visited[i] = 0;

    while (!q.empty()) {
        int curr = q.front();
        q.pop();

        int nexts[] = {curr * 2, curr - 1, curr + 1};

        for (int next : nexts) {
            if (next >= 0 && next < K * 2 && visited[next] == -1){
                visited[next] = visited[curr] + 1;
                if (next == K) {
                    std::cout << visited[next] << '\n';
                    return 0;
                }
                q.push(next);
            } 
        }
    }

}