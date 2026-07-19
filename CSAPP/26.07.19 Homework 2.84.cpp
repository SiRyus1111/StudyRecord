// 부동소수점 인자와 동일한 비트 표현을 가진 32비트 정수를 반환하는 함수
unsigned f2u(float n);

// x가 y보다 작거나 같은지 여부를 검사하는 함수
int float_le(float x, float y) {
    unsigned ux = f2u(x);
    unsigned uy = f2u(y);

    unsigned sx = ux >> 31;
    unsigned sy = uy >> 31;

    int both_zero = !((ux << 1) | (uy << 1));

    int neg_pos = sx & !sy;

    int both_pos_le = (!sx & !sy) & (((ux - uy) >> 31) | !(ux - uy));

    int both_neg_le = (sx & sy) & (((uy - ux) >> 31) | !(uy - ux));

    // 함수의 반환값(0 / 1)
    return (both_zero | neg_pos | both_pos_le | both_neg_le);
}

// 경우를 분리해보자..

// 1. x와 y가 부호가 다른 경우 - 바로 구별 가능
// x와 y가 부호가 다르면 x <= y를 바로 구별할 수 있음.
// x : 음수 / y : 양수라면 바로 1 나오고,
// x : 양수 / y : 음수라면 바로 0 나옴.

// 2. x와 y가 부호에 상관없이 0인 경우 - 바로 구별 가능
// x와 y가 부호에 상관없이 0이라면 바로 구별할 수 있음.
// 부호를 안보고 exp / frac만 보면 됨. 둘다 0인지 확인하고.

// 3. x와 y가 같은 부호인 경우 - 별도로 구별해야함
// 같은 부호라면 x와 y를 직접 비교해봐야함.
// 어차피 부호가 같으면 최상위 비트도 같을테니
// x와 y를 직접 비교해보면 됨.
// 둘다 양수 / 둘다 음수인 경우 따로따로 해서.
// 주의점 : 음수일 때와 양수일 때 비교하는 방향이 다름.