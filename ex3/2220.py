class Solution(object):
    def minBitFlips(self, start, goal):
        xor_result = start ^ goal
        count = 0
        while xor_result:
            xor_result &= xor_result - 1
            count += 1
        return count

def __main__():
    start = int(input("Enter the start integer: "))
    goal = int(input("Enter the goal integer: "))
    solution = Solution()
    result = solution.minBitFlips(start, goal)
    print(f"Minimum number of bit flips required: {result}")

if __name__ == "__main__":
    __main__()