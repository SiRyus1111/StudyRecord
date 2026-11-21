#include <iostream>
#include <vector>

void insertion_sort(std::vector<int>& array) {
    for (int i = 1; i < array.size(); i++) {
        int key = array[i];
        int j = i - 1;
        
        while (j >= 0) {
            if (array[j] <= key) break;
            array[j + 1] = array[j];
            j--;
        }

        array[j + 1] = key;
    }

    return;
}

int main() {
    std::vector<int> vec = {1, 4, 3, 2, 5, 10, 2};

    insertion_sort(vec);
}