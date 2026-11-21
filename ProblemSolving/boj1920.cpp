// boj1920 - 수 찾기

// 평범한 이분탐색 문제이다.
// 이게 입력이 최대 100000만개, 찾아야하는 수도 최대 100000개라 단순 선형 탐색으로는 못 푼다.
// 어차피 수를 정렬할 수 있는 김에, 이분탐색으로 풀면 된다.

#include <iostream>
#include <vector>
#include <algorithm>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int N, M;
    
    std::cin >> N;
    std::vector<int> Ns(N);

    for (int i = 0; i < N; i++) {
        std::cin >> Ns[i];
    }

    std::sort(Ns.begin(), Ns.end());

    std::cin >> M;
    std::vector<int> res(M, 0);

    for (int i = 0; i < M; i++) {
        // 각 Ms[n]은 한 번만 쓰이고 그 이후에는 쓰이지 않으므로
        // 한번만 입력받고 결과만 res에 저장한 후 그대로 버린다.
        int find_val;
        std::cin >> find_val;

        int start = 0; // start = 0
        int end = Ns.size() - 1; // end = Ns의 최대 인덱스
        int mid;

        while (start <= end) {
            mid = (start + end) / 2; // 문제 조건 상 오버플로우 걱정할 필요 없음

            // 평범한 이분탐색
            if (Ns[mid] == find_val) {
                res[i] = 1;
                break;
            } 
            else if(Ns[mid] < find_val) {
                start = mid + 1;
            }
            else {
                end = mid - 1;
            }
        }
    }

    // 결과 출력
    for (int ans : res) {
        std::cout << ans << '\n';
    }

    return 0;
}