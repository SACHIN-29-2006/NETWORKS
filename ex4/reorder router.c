#include <stdlib.h>

int minReorder(int n, int** connections, int connectionsSize, int* connectionsColSize) {
    int* head = (int*)malloc(n * sizeof(int));
    for (int i = 0; i < n; i++) {
        head[i] = -1;
    }

    int max_edges = 2 * connectionsSize;
    int* to = (int*)malloc(max_edges * sizeof(int));
    int* dir = (int*)malloc(max_edges * sizeof(int));
    int* next = (int*)malloc(max_edges * sizeof(int));
    int edge_cnt = 0;
    for (int i = 0; i < connectionsSize; i++) {
        int u = connections[i][0];
        int v = connections[i][1];
        to[edge_cnt] = v;
        dir[edge_cnt] = 1;
        next[edge_cnt] = head[u];
        head[u] = edge_cnt++;
        to[edge_cnt] = u;
        dir[edge_cnt] = 0;
        next[edge_cnt] = head[v];
        head[v] = edge_cnt++;
    }
    int* visited = (int*)calloc(n, sizeof(int));
    int* queue = (int*)malloc(n * sizeof(int));
    int head_q = 0, tail_q = 0;

    queue[tail_q++] = 0;
    visited[0] = 1;
    int reorder_count = 0;
    while (head_q < tail_q) {
        int curr = queue[head_q++];
        for (int e = head[curr]; e != -1; e = next[e]) {
            int neighbor = to[e];
            if (!visited[neighbor]) {
                visited[neighbor] = 1;
                reorder_count += dir[e]; 
                queue[tail_q++] = neighbor;
            }
        }
    }
    free(head);
    free(to);
    free(dir);
    free(next);
    free(visited);
    free(queue);

    return reorder_count;
}