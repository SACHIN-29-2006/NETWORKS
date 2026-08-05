#include <vector>

class Solution {
public:
    std::vector<int> countBits(int n) {
        std::vector<int> ans(n + 1, 0);
        for (int i = 1; i <= n; ++i) {
            ans[i] = ans[i >> 1] + (i & 1);
        }
        return ans;
    }
};

int main() { 
    
    Solution solution;
    int n = 5; 
    std::vector<int> result = solution.countBits(n);
    
    for (int i = 0; i <= n; ++i) {
        printf("Number of 1's in binary representation of %d is %d\n", i, result[i]);
    }
    
    return 0;
}