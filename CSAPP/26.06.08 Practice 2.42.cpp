#include <iostream>

int div16(int x) {
    int bias = (x >> 31) & 0xF; // 부호비트가 1인지 0인지에 따라 산술 시프트 연산과 &으로 마스킹해서 바이어스 뽑아버리기

    return (x + bias) >> 4;
}

int main() {
    int val;
    while (true) {
        std::cin >> val;
        std::cout << div16(val) << '\n';
    }

    return 0;
}
