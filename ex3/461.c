int hammingDistance(int x, int y) {
    int xor_result = x ^ y;
    int count = 0;
    
    while (xor_result > 0) {
        count += xor_result & 1;
        xor_result >>= 1;
    }
    
    return count;
}