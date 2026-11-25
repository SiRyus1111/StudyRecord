// boj1932 - 정수 삼각형

// 평범한 dp문제이다.
// RGB 거리와 비슷한 논리로,
// dp[i][j]를 i번째 층의 j번째 원소까지의 최댓값으로 놓고,
// dp[i][j] = max(dp[i-1][j-1] dp[i][j])가 점화식으로 놓는다.
// 물론 양끝에 있는 원소도 따로 처리해야한다.

#include <iostream>
#include <vector>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int n;

    std::cin >> n;

    std::vector<std::vector<int>> tri(n, std::vector<int>(n, -1));
    std::vector<std::vector<int>> dp(n, std::vector<int>(n, -1));

    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            std::cin >> tri[i][j];
        }
    }

    if (n == 1) {
        std::cout << tri[0][0] << '\n';
        return 0;
    }

    dp[0][0] = tri[0][0];
    dp[1][0] = tri[1][0] + dp[0][0];
    dp[1][1] = tri[1][1] + dp[0][0];

    for (int i = 2; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            if (j == 0) {
                dp[i][0] = tri[i][0] + dp[i-1][0];
            }
            else if (j == i) {
                dp[i][j] = tri[i][j] + dp[i-1][j-1];
            }
            else {
                dp[i][j] = tri[i][j] + std::max(dp[i-1][j-1], dp[i-1][j]);
            }
        }
    }

    int max = 0;
    for (int num : dp[n-1]) {
        max = std::max(num, max);
    }

    std::cout << max << '\n';

    return 0;
}