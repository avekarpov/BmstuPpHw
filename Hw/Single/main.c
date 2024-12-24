#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <time.h>

// #define DEBUG

#define CONT_LOG(__FMT__) printf(__FMT__)
#define LOG(__FMT__, ...) printf(__FMT__ "\n", ##__VA_ARGS__)

#define NUM_CITIES 20
#define START_CITY 0
#define ADJ_PERCENT 40

int distance_matrix[NUM_CITIES][NUM_CITIES];

int rank;
int size;

int path[NUM_CITIES];
int used_cities[NUM_CITIES];

#ifdef DEBUG

void log_path() {
    CONT_LOG("Path [");
    for (int i = 0; i < NUM_CITIES; ++i) {
        printf("%d, ", path[i]);
    }
    printf("]\n");
}

#endif

void fill_matrix(int percent) {
    LOG("Generate matrix");

    for (int i = 0; i < NUM_CITIES; ++i) {
        for (int j = 0; j < NUM_CITIES; ++j) {
            if (i == j) {
                distance_matrix[i][j] = 0;
            } else {
                if (rand() % 100 < percent) {
                    distance_matrix[i][j] = rand() % 100 + 1;
                } else {
                    distance_matrix[i][j] = -1;
                }
            }
        }
    }

    CONT_LOG("Generated, matrix:\n[");
    for (int i = 0; i < NUM_CITIES; ++i) {
        printf(" [");
        for (int j = 0; j < NUM_CITIES; ++j) {
            printf("%d, ", distance_matrix[i][j]);
        }
        printf("]\n");
    }
    printf("]\n");
}

int next_master_path() {
    int i = NUM_CITIES - 1;

    while (0 < i) {
        used_cities[path[i]] = 0;

        ++path[i];
        while (path[i] < NUM_CITIES) {
            if (!used_cities[path[i]]) {
                if (distance_matrix[path[i - 1]][path[i]] != -1) {
                    used_cities[path[i]] = 1;
                    break;
                }
            }
            ++path[i];
        }

        if (path[i] < NUM_CITIES) {
            break;
        }

        --i;
    }

    if (i == 0) {
        return 0;
    }

    ++i;

    while (i < NUM_CITIES) {
        path[i] = 0;

        while (used_cities[path[i]]) {
            ++path[i];
        }
        used_cities[path[i]] = 1;

        ++i;
    }

    return 1;
}

int calculate_distance() {
    int result = 0;
    for (int i = 0; i < NUM_CITIES - 1; ++i) {
        if (distance_matrix[path[i]][path[i + 1]] == -1) {
            return -1;
        }

        result += distance_matrix[path[i]][path[i + 1]];
    }
    result += distance_matrix[path[NUM_CITIES - 1]][path[START_CITY]];
    return result;
}

int main(int argc, char **argv) {
    srand(0);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    LOG("Hello!");

    LOG("Preparing...");
    fill_matrix(ADJ_PERCENT);

    LOG("Start");

    double start = MPI_Wtime();

    LOG("Searching solution...");

    path[0] = START_CITY;
    used_cities[START_CITY] = 1;
    for (int i = 1, j = 0; i < NUM_CITIES; ++i) {
        while (used_cities[j]) {
            ++j;
        }
        path[i] = j;
        used_cities [j] = 1;
    }

    int best_distatnce = INT_MAX;
    int best_path[NUM_CITIES];

    while (1) {
        #ifdef DEBUG

        log_path();

        #endif

        int distance = calculate_distance();
        if (distance != -1 && distance < best_distatnce) {
            for (int i = 0; i < NUM_CITIES; ++i) {
                best_path[i] = path[i];
                best_distatnce = distance;
            }
        }

        if (!next_master_path()) {
            break;
        }
    }

    if (best_distatnce == INT_MAX) {
        LOG("Failed to find path");
    } else {
        CONT_LOG("Best path: [");
        for (int i = 0; i < NUM_CITIES; ++i) {
            printf("%d, ", best_path[i]);
        }
        printf("], with distance: %d\n", best_distatnce);
    }
    LOG("Total time: %f", MPI_Wtime() - start);

    LOG("End");

    MPI_Finalize();
}
