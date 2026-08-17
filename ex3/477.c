int totalHammingDistance(int* nums, int numsSize) {
    int total = 0;
    
    for (int i = 0; i < 32; i++) {
        int count_ones = 0;
        for (int j = 0; j < numsSize; j++) {
            count_ones += (nums[j] >> i) & 1;
        }
        total += count_ones * (numsSize - count_ones);
    }
    
    return total;
}