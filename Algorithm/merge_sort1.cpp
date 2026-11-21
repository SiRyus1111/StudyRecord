#include <vector>
#include <iostream>

void Merge(std::vector<int>& array, int p, int q, int r) {
    int n_L = q - p + 1; // 왼쪽 배열의 크기
    int n_R = r - q; // 오른쪽 배열의 크기
    std::vector<int> L(n_L);
    std::vector<int> R(n_R);

    // 왼쪽 배열에 왼쪽 옮기기
    for (int i = 0; i < n_L; i++) {
        L[i] = array[p + i];
    }

    // 오른쪽 배열에 오른쪽 옮기기
    for (int i = 0; i < n_R; i++) {
        R[i] = array[q + 1 + i];
    }

    // 인덱스 정하기
    int i = 0;
    int j = 0;
    int k = p;

    // 정렬하기
    while (i < n_L && j < n_R) {
        if (L[i] <= R[j]) {
            array[k] = L[i];
            i++;
        }
        else {
            array[k] = R[j];
            j++;
        }
        k++;
    }

    // 남은 곳 정렬하기
    while (i < n_L) {
        array[k] = L[i];
        i++;
        k++;
    }
    while (j < n_R) {
        array[k] = R[j];
        j++;
        k++;
    }

}

void MergeSort(std::vector<int>& array, int p, int r) {
    if (p >= r) {
        return;
    }
    int q = (p + r) / 2;

    MergeSort(array, p, q);
    MergeSort(array, q + 1, r);
    Merge(array, p, q, r);
}

int main() {
    std::vector<int> vec = {1, 4, 3, 2, 5, 10, 2};

    MergeSort(vec, 0, vec.size() - 1);

    return 0;
}