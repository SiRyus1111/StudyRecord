typedef unsigned float_bits;

float_bits float_negate(float_bits f){
    unsigned sign_bit = f >> 31;
    unsigned exp = f >> 23 & 0xFF; // exp 필드의 크기인 8비트만 마스크로 빼오기
    unsigned frac = f & 0x7FFFFF; // frac 구간 = 23비트이므로 하위 23비트만 마스크로 빼오기

    if (exp == 0xFF && (frac != 0)){
        return f;
    }
    else {
        float_bits neg_f = 0; // 0x00000000
        neg_f = ((sign_bit ^ 0x00000001) << 31) | (exp << 23) | frac;

        return neg_f;
    }
}