#include <iostream>

long fibonacci(long n) {
    long i = 2;
    long first = 0;
    long second = 1;
    long next = 0;
    if (n <= 1) {
        goto done;
    }
loop:
    next = first + second;
    first = second;
    second = next;
    i++;
    if (i <= n) {
        goto loop;
    }
done:
    return next;
}