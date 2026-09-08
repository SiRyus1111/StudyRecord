void gotoCond(short a, short *p) {
    if (!a) {
        goto L1;
    }
    else if(*p >= a) {
        goto L1;
    }
    *p = a;

L1:
    return;
}