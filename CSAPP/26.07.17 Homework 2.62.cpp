#include <iostream>

bool int_shifts_are_arithmetic() {
    int num = -1;
    /*
    num = 1 << ((sizeof(int) << 3) - 1); // 최상위 비트만 1로 만들기
    // 왼쪽 시프트만 쓰므로 논리 / 산술 시프트에 대한 편차 발생하지 않음
    */
    
    int mask = num;

    num = num >> 1; // 한칸 오른쪽 시프트. 산술이면 최상위 비트 = 1, 논리라면 최상위 비트 = 0

    return (num & mask) == mask;
}

int main(){
    
}