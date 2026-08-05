class Solution:
    def subarrayBitwiseORs(self, arr)
        all_ors = set()
        current_ors = set()
        
        for num in arr:
            current_ors = {or_val | num for or_val in current_ors} | {num}
            all_ors |= current_ors
            
        return len(all_ors)


s = Solution()

print(s.subarrayBitwiseORs([1,2,4]))
print(s.subarrayBitwiseORs([1,3,2,4]))
print(s.subarrayBitwiseORs([2,2,2]))