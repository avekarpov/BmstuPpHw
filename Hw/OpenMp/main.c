#include <mpi.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

// #define DEBUG

#define CONT_LOG(__FMT__) printf("[%d{t%d}]: " __FMT__, rank, omp_get_thread_num())
#define LOG(__FMT__, ...) printf("[%d{t%d}]: " __FMT__ "\n", rank, omp_get_thread_num(), ##__VA_ARGS__)

#ifdef DEBUG
#define LOG_DEBUG(...) LOG(__VA_ARGS__)
#else
#define LOG_DEBUG(...)
#endif

#define NUM_CITIES 20
#define START_CITY 0
#define ADJ_PERCENT 40
#define MASTER_PATH_SIZE 5
#define SLAVE_PATH_SIZE = NUM_CITIES - MASTER_PATH_SIZE

int distance_matrix[NUM_CITIES][NUM_CITIES];

int rank;
int size;

int is_master() {
    if (rank == 0) {
        return 1;
    } else {
        return 0;
    }
}

#ifdef DEBUG

void log_path(int *path) {
    CONT_LOG("Path [");
    for (int i = 0; i < MASTER_PATH_SIZE; ++i) {
        printf("%d, ", path[i]);
    }
    for (int i = MASTER_PATH_SIZE; i < NUM_CITIES; ++i) {
        if (is_master()) {
            printf("*, ");
        } else {
            printf("%d, ", path[i]);
        }
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

void send_matrix() {
    for (int i = 1; i < size; ++i) {
        LOG("Send matrix to %d", i);
        MPI_Send(distance_matrix, NUM_CITIES * NUM_CITIES, MPI_INT, i, 1, MPI_COMM_WORLD);
    }
}

void receive_matrix() {
    LOG("Receive matrix");
    MPI_Recv(distance_matrix, NUM_CITIES * NUM_CITIES, MPI_INT, 0, 1 , MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}

int next_master_path(int *path, int *used_cities) {
    int i = MASTER_PATH_SIZE - 1;

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

    while (i < MASTER_PATH_SIZE) {
        path[i] = 0;

        while (used_cities[path[i]]) {
            ++path[i];
        }
        used_cities[path[i]] = 1;

        ++i;
    }

    return 1;
}

int next_slave_path(int *path, int *used_cities) {
    int i = NUM_CITIES - 1;

    while (MASTER_PATH_SIZE <= i) {
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

    if (i == MASTER_PATH_SIZE - 1) {
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

void send_master_path(int *path) {
    LOG_DEBUG("Waiting next request...");
    MPI_Status status;
    // TODO: try send NULL, 0 size
    int _;
    MPI_Recv(&_, 1, MPI_INT, MPI_ANY_SOURCE, 2, MPI_COMM_WORLD, &status);

    LOG_DEBUG("Send path to %d", status.MPI_SOURCE);
    MPI_Send(path, MASTER_PATH_SIZE, MPI_INT, status.MPI_SOURCE, 3, MPI_COMM_WORLD);
}

void receive_master_path(int *path) {
    LOG_DEBUG("Request next path");
    int _;
    MPI_Send(&_, 1, MPI_INT, 0, 2, MPI_COMM_WORLD);

    LOG_DEBUG("Receive path");
    MPI_Recv(path, MASTER_PATH_SIZE, MPI_INT, 0, 3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    #ifdef DEBUG

    CONT_LOG("Path: [");
    for (int i = 0; i < MASTER_PATH_SIZE; ++i) {
        printf("%d, ", path[i]);
    }
    for (int i = MASTER_PATH_SIZE; i < NUM_CITIES; ++i) {
        printf("*, ");
    }
    printf("]\n");

    #endif
}

int calculate_distance(int *path) {
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

void find_best_path(int *best_path) {
    int best_distatnce = INT_MAX;

    int path[NUM_CITIES];
    int used_cities[NUM_CITIES];

    while (1) {
        receive_master_path(path);

        if (path[0] == -1) {
            break;
        }

        for (int i = 0; i < NUM_CITIES; ++i) {
            used_cities[i] = 0;
        }
        for (int i = 0; i < MASTER_PATH_SIZE; ++i) {
            used_cities[path[i]] = 1;
        }
        for (int i = MASTER_PATH_SIZE, j = 0; i < NUM_CITIES; ++i) {
            while (used_cities[j]) {
                ++j;
            }
            path[i] = j;
            used_cities [j] = 1;
        }

        while (1) {
            #ifdef DEBUG

            log_path(path);

            #endif

            int distance = calculate_distance(path);

            if (distance != -1) {
                if (best_distatnce > distance) {
                    best_distatnce = distance;
                    for (int i = 0; i < NUM_CITIES; ++i) {
                        best_path[i] = path[i];
                    }
                }
            }

            if (!next_slave_path(path, used_cities)) {
                break;
            }
        }
    }
}

int main(int argc, char **argv) {
    srand(0);

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    LOG("Hello!");

    LOG("Preparing...");
    if (is_master()) {
        fill_matrix(ADJ_PERCENT);
        send_matrix();
    } else {
        receive_matrix();
    }

    LOG("Start");

    if (is_master()) {
        double start = MPI_Wtime();

        LOG("Searching solution...");

        int path[NUM_CITIES];
        int used_cities[NUM_CITIES];

        path[0] = START_CITY;
        used_cities[START_CITY] = 1;
        for (int i = 1, j = 0; i < MASTER_PATH_SIZE; ++i) {
            while (used_cities[j]) {
                ++j;
            }
            path[i] = j;
            used_cities [j] = 1;
        }

        while (1) {
            #ifdef DEBUG

            log_path(path);

            #endif

            send_master_path(path);

            if (!next_master_path(path, used_cities)) {
                break;
            }
        }

        // TODO: stupid way
        for (int i = 0; i < NUM_CITIES; ++i) {
            path[i] = -1;
        }
        for (int i = 1; i < size; ++i) {
            send_master_path(path); // 1st thread
            send_master_path(path); // 2nd thread
        }

        int best_distatnce = INT_MAX;
        int best_path[NUM_CITIES];

        for (int i = 1; i < size; ++i) {
            MPI_Recv(path, NUM_CITIES, MPI_INT, i, 4, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            int distance = calculate_distance(path);
            if (best_distatnce > distance) {
                best_distatnce = distance;
                for (int j = 0; j < NUM_CITIES; ++j) {
                    best_path[j] = path[j];
                }
            }
        }

        CONT_LOG("Best path: [");
        for (int i = 0; i < NUM_CITIES; ++i) {
            printf("%d, ", best_path[i]);
        }
        printf("], with distance: %d\n", best_distatnce);

        LOG("Total time: %f", MPI_Wtime() - start);

    } else {
        int l_best_path[NUM_CITIES];
        int r_best_path[NUM_CITIES];

        #pragma omp parallel sections
        {
            #pragma omp section
            {
                find_best_path(l_best_path);
            }

            #pragma omp section
            {
                find_best_path(r_best_path);
            }
        }

        int *best_path;
        if (calculate_distance(l_best_path) < calculate_distance(r_best_path)) {
            best_path = l_best_path;
        } else {
            best_path = r_best_path;
        }

        MPI_Send(best_path, NUM_CITIES, MPI_INT, 0, 4, MPI_COMM_WORLD);
    }

    LOG("End");

    MPI_Finalize();
}