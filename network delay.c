#include <limits.h>

int networkDelayTime(int** times, int timesSize, int* timesColSize, int n, int k) {
    int dist[n + 1];
    int visited[n + 1];
    for (int i = 1; i <= n; i++) {
        dist[i] = INT_MAX;
        visited[i] = 0;
    }
    dist[k] = 0;
    
    for (int step = 1; step <= n; step++) {
        int u = -1;
        int minDist = INT_MAX;
        for (int i = 1; i <= n; i++) {
            if (!visited[i] && dist[i] < minDist) {
                minDist = dist[i];
                u = i;
            }
        }
        if (u == -1) {
            break;
        }
        visited[u] = 1;
        for (int i = 0; i < timesSize; i++) {
            int source = times[i][0];
            int target = times[i][1];
            int weight = times[i][2];
            
            if (source == u && dist[u] != INT_MAX) {
                if (dist[u] + weight < dist[target]) {
                    dist[target] = dist[u] + weight;
                }
            }
        }
    }
    
    int maxTime = 0;
    for (int i = 1; i <= n; i++) {
        if (dist[i] == INT_MAX) {
            return -1; 
        }
        if (dist[i] > maxTime) {
            maxTime = dist[i];
        }
    }
    
    return maxTime;
}