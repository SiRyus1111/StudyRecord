// 백준 1991 - 트리 순회

// 이진 트리를 순회하는 문제이다.
// 전위 순회 : 루트(출력) -> 왼쪽 -> 오른쪽
// 중위 순회 : 왼쪽 -> 루트(출력) -> 오른쪽
// 후위 순회 : 왼쪽 -> 오른쪽 -> 루트(출력)

// 입력 받는게 제일 어려운 문제인 듯 한데,
// 그냥 Node 구조체 만들어서 초기값 -1로 박아놓고
// 그걸 배열로 만들어서 인덱스를 node_name - 'A'로 해놓으면 A = 0번 인덱스부터 아름답게 잘 된다..

#include <iostream>
#include <vector>

struct Node {
    int left = -1;
    int right = -1;
};

std::vector<Node> tree(26);

void preorder(int cur) {
    if (cur == -1) return;
    std::cout << static_cast<char>(cur + 'A');
    preorder(tree[cur].left);
    preorder(tree[cur].right);
}

void inorder(int cur) {
    if (cur == -1) return;
    inorder(tree[cur].left);
    std::cout << static_cast<char>(cur + 'A');
    inorder(tree[cur].right);    
}

void postorder(int cur) {
    if (cur == -1) return;
    postorder(tree[cur].left);
    postorder(tree[cur].right);
    std::cout << static_cast<char>(cur + 'A');
}

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    int N;
    std::cin >> N;

    for (int i = 0; i < N; i++) {
        char data, left, right;
        std::cin >> data >> left >> right;
        if ('A' <= left && 'Z' >= left) {
            tree[static_cast<int>(data - 'A')].left = static_cast<int>(left - 'A');
        }
        if ('A' <= right && 'Z' >= right) {
            tree[static_cast<int>(data - 'A')].right = static_cast<int>(right - 'A');
        }
        
    }

    preorder(0);
    std::cout << '\n';
    inorder(0);
    std::cout << '\n';
    postorder(0);
    std::cout << '\n';

    return 0;
}