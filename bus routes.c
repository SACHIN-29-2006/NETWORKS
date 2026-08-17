#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

int numBusesToDestination(int** routes, int routesSize, int* routesColSize, int source, int target) {
    if (source == target) return 0;
    int maxStop = -1;
    for (int i = 0; i < routesSize; i++) {
        for (int j = 0; j < routesColSize[i]; j++) {
            if (routes[i][j] > maxStop) {
                maxStop = routes[i][j];
            }
        }
    }
    if (source > maxStop || target > maxStop) return -1;
    int* stopBusCount = (int*)calloc(maxStop + 1, sizeof(int));
    for (int i = 0; i < routesSize; i++) {
        for (int j = 0; j < routesColSize[i]; j++) {
            stopBusCount[routes[i][j]]++;
        }
    }
    int** stopToBuses = (int**)malloc((maxStop + 1) * sizeof(int*));
    for (int i = 0; i <= maxStop; i++) {
        if (stopBusCount[i] > 0) {
            stopToBuses[i] = (int*)malloc(stopBusCount[i] * sizeof(int));
        } else {
            stopToBuses[i] = NULL;
        }
    }
    int* currentIdx = (int*)calloc(maxStop + 1, sizeof(int));
    for (int i = 0; i < routesSize; i++) {
        for (int j = 0; j < routesColSize[i]; j++) {
            int stop = routes[i][j];
            stopToBuses[stop][currentIdx[stop]++] = i;
        }
    }
    bool* visitedBuses = (bool*)calloc(routesSize, sizeof(bool));
    bool* visitedStops = (bool*)calloc(maxStop + 1, sizeof(bool));
    int queueCapacity = (maxStop + 1 < 100000) ? 100000 : (maxStop + 1);
    int* queueStop = (int*)malloc(queueCapacity * sizeof(int));
    int* queueDist = (int*)malloc(queueCapacity * sizeof(int));
    int head = 0, tail = 0;
    queueStop[tail] = source;
    queueDist[tail] = 0;
    tail++;
    visitedStops[source] = true;

    int minBuses = -1;
    while (head < tail) {
        int currStop = queueStop[head];
        int currDist = queueDist[head];
        head++;

        if (currStop == target) {
            minBuses = currDist;
            break;
        }
        for (int i = 0; i < stopBusCount[currStop]; i++) {
            int busIdx = stopToBuses[currStop][i];
            if (visitedBuses[busIdx]) continue;
            visitedBuses[busIdx] = true;
            for (int j = 0; j < routesColSize[busIdx]; j++) {
                int nextStop = routes[busIdx][j];
                if (!visitedStops[nextStop]) {
                    visitedStops[nextStop] = true;
                    queueStop[tail] = nextStop;
                    queueDist[tail] = currDist + 1;
                    tail++;
                }
            }
        }
    }
    for (int i = 0; i <= maxStop; i++) {
        if (stopToBuses[i] != NULL) free(stopToBuses[i]);
    }
    free(stopToBuses);
    free(stopBusCount);
    free(currentIdx);
    free(visitedBuses);
    free(visitedStops);
    free(queueStop);
    free(queueDist);

    return minBuses;
}