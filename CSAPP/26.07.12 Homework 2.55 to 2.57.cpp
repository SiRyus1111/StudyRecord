#include <iostream>
#include <cstdio>

void show_bytes(char* ptr, int length){

    for (int i = 0; i < length; i++) {
        printf("%x ", ptr[i]);
    }
    printf("\n");
}

int main(){
    int test1 = 0x12345678;
    short test2 = 0x1234;
    long test3 = 0x12345678;

    show_bytes((char*) &test1, sizeof(test1));
    show_bytes((char*) &test2, sizeof(test2));
    show_bytes((char*) &test3, sizeof(test3));
}