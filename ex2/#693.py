class Solution:
    def hasAlternatingBits(self, n: int) -> bool:
        x = n ^ (n >> 1)
        return (x & (x + 1)) == 0


s = Solution()

print(s.hasAlternatingBits(5))
print(s.hasAlternatingBits(7))