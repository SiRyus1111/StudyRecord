// boj1003 - 피보나치 함수

// 전형적인 DP를 이용해서 피보나치 수를 계산하는 문제인데,
// 이번에는 최종적으로 0과 1이 얼마나 많이 계산되는지에 대한 문제이다.

// std::pair로 0이 호출된 횟수 / 1이 호출된 횟수로 vector의 각 인덱스를 잡아놓고,
// 0 / 1에다가 초기값을 설정하는 식으로 풀면 된다.

// N == 0 / N == 1인 경우는 예외 상황이므로 N을 입력받은 직후 검사한다.

#include <iostream>
#include <vector>

int main() {

    int T;
    std::cin >> T;

    for (int i = 0; i < T; i++) {
        int N;
        std::cin >> N;

        // vec의 first : 0이 호출된 횟수
        // vec의 second : 1이 호출된 횟수

        // 초기값 처리
        if (N == 0) {
            std::cout << 1 << ' ' << 0 << '\n';
            continue;
        }
        else if (N == 1) {
            std::cout << 0 << ' ' << 1 << '\n';
            continue;
        }

        std::vector<std::pair<int, int>> vec;
        vec.resize(N + 1);

        // 초기값
        vec[0].first = 1;
        vec[0].second = 0;

        vec[1].first = 0;
        vec[1].second = 1;
    

        // 피보나치 점화식은 다들 알지?
        for (int i = 2; i <= N; i++) {
            vec[i].first = vec[i-1].first + vec[i-2].first;
            vec[i].second = vec[i-1].second + vec[i-2].second;
        }

        std::cout << vec[N].first << ' ' << vec[N].second << '\n';
    }


    return 0;
}