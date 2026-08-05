#include <stdlib.h>

struct TrieNode {
    int next[2];
};

struct TrieNode nodePool[3000005];
int nodeCount;

int createNode() {
    nodePool[nodeCount].next[0] = -1;
    nodePool[nodeCount].next[1] = -1;
    return nodeCount++;
}

void insert(int root, int num) {
    int curr = root;
    for (int i = 30; i >= 0; i--) {
        int bit = (num >> i) & 1;
        if (nodePool[curr].next[bit] == -1) {
            nodePool[curr].next[bit] = createNode();
        }
        curr = nodePool[curr].next[bit];
    }
}

int findMaxXOR(int root, int num) {
    int curr = root;
    int max_xor = 0;
    for (int i = 30; i >= 0; i--) {
        int bit = (num >> i) & 1;
        int toggledBit = 1 - bit;
        if (nodePool[curr].next[toggledBit] != -1) {
            max_xor |= (1 << i);
            curr = nodePool[curr].next[toggledBit];
        } else {
            curr = nodePool[curr].next[bit];
        }
    }
    return max_xor;
}

int findMaximumXOR(int* nums, int numsSize) {
    if (numsSize < 2) return 0;
    
    nodeCount = 0;
    int root = createNode();
    
    for (int i = 0; i < numsSize; i++) {
        insert(root, nums[i]);
    }
    
    int maxResult = 0;
    for (int i = 0; i < numsSize; i++) {
        int currentXOR = findMaxXOR(root, nums[i]);
        if (currentXOR > maxResult) {
            maxResult = currentXOR;
        }
    }
    
    return maxResult;
}