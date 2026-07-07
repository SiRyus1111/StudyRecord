// 어떤 파일도 인클루드 하지 않는 것이 조건.
// 근데 솔직히 값을 확인하기 위한 출력 스트림은 좀 쓰자..

#define POS_INFINITY +1.0 / 0.0 // 뭔가 극한 느낌
#define NEG_INFINITY -1.0 / 0.0 // 이것도
#define NEG_ZERO -0.0 // 이건 그냥 대문짝만하게 박아버리기

// 다른 것도 가능하지 않을까?

#define POS_INF 1e400 // 오버플로우 느낌
#define NEG_INF -(POS_INF) // 그냥 POS_INF에다가 -붙인거
#define NEG_ZERO 0.0 / -1.0 // 뭔가 극한처럼 써먹기

int main() {

}