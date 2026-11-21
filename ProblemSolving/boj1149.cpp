// boj1149 - RGB 거리

// 집이 N개 있을 때, 연속적으로 같은 색의 집을 칠하지 않고 가장 적은 비용으로 집을 칠하는 문제이다.

// dp[i][j]를 현재까지의 가장 적은 비용의 합으로 놓고,
// dp[i][0] = min(dp[i-1][1], dp[i-1][2]) 식으로 하면 되지 않을까?

#include <iostream>
#include <vector>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    int N;
    
    std::cin >> N;

    std::vector<std::vector<int>> dp(N, std::vector<int>(3));

    std::cin >> dp[0][0] >> dp[0][1] >> dp[0][2];

    for (int i = 1; i < N; i++) {
        int r, g, b;
        std::cin >> r >> g >> b;

        dp[i][0] = r + std::min(dp[i-1][1], dp[i-1][2]);
        dp[i][1] = g + std::min(dp[i-1][0], dp[i-1][2]);
        dp[i][2] = b + std::min(dp[i-1][0], dp[i-1][1]);
    }

    std::cout << std::min(std::min(dp[N-1][0], dp[N-1][1]), dp[N-1][2]);

    return 0;
}