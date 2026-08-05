#include<stdio.h>

class Solution {
public:
    int findComplement(int num) {
        long mask = 1;
        while (mask <= num)
            mask <<= 1;
        return (mask - 1) ^ num;
    }
};

int main()
{
    Solution solution;
    int num = 5; 
    int complement = solution.findComplement(num);
    printf("The complement of %d is %d\n", num, complement);
    return 0;
}