#include <iostream>

int divide_power2(int x, int k){
    // 쉽지!

    int sign_mask = x >> 31; // 산술 시프트임.

    // 여기서 x가 양수 / 음수에 따라 음수에만 바이어스가 적용되는 그런거..(이게 핵심)
    // 만들었다.. (100% 내 머리로 한건 아니긴 한데..)
    int bias = ((1 << k) - 1) & sign_mask;

    return ((x + bias) >> k);
}