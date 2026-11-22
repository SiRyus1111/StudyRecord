// boj1654 - 랜선 자르기

// 전형적인 파라매트릭 서치 문제이다.
// 일단 해당 길이까지 되는지 / 안되는지 보고 되면 늘리고 안되면 줄이면 된다.
// 첫 길이는 제일 긴 랜선으로 고르면 된다.

#include <iostream>
#include <vector>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int K, N;

    std::cin >> K >> N;

    std::vector<long long> lans(K);

    long long max = 0;

    for (int i = 0; i < K; i++) {
        std::cin >> lans[i];
        if (max < lans[i]) {
            max = lans[i];
        }
    }

    long long start = 1;
    long long end = max;
    long long mid;
    long long ans;

    while (start <= end) {
        mid = (start + end) / 2;

        int sum = 0;
        for (long long len : lans) {
            sum += len / mid;
        }

        if (sum < N) {
            end = mid - 1; 
        }
        else {
            start = mid + 1;
            ans = mid;
        }
    }

    std::cout << ans;

    return 0;
}