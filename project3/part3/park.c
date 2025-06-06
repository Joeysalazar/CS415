/*
 * park.c: Duck Park Simulation with Monitor (Part 3)
 *
 * Compile with: gcc -g -Wall -pthread park.c -o park -lm
 *
 * Usage:
 *   ./park -n <num_passengers> -c <num_cars> -p <capacity_per_car> \
 *          -w <car_wait_time> -r <ride_duration>
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>

#define MAX_CARS 100
#define MAX_PASSENGERS 1000

/* Car status codes */
enum { CAR_WAITING = 0, CAR_LOADING = 1, CAR_RUNNING = 2 };

typedef struct {
    int ticket_queue;
    int ride_queue;
    int exploring;
    int on_ride;
    int total_served;
    int total_rides;
    int car_count;
    int car_status[MAX_CARS];
    int car_loaded[MAX_CARS];
} shared_state_t;

/* Command-line parameters */
static int num_passengers = 0;
static int num_cars = 0;
static int capacity = 0;
static int car_wait = 0;
static int ride_duration = 0;

/* Shared state */
static shared_state_t shared;

/* Synchronization */
static pthread_mutex_t ticket_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ride_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t car_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ride_cond = PTHREAD_COND_INITIALIZER;

/* Semaphores */
static sem_t ride_sem;
static sem_t unload_sem[MAX_PASSENGERS];

/* Current loading car */
static int current_car = -1;

/* Start time */
static struct timespec start_time;

/* Utility: elapsed time */
static void get_elapsed(char *buf, size_t len) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long sec = now.tv_sec - start_time.tv_sec;
    long nsec = now.tv_nsec - start_time.tv_nsec;
    if (nsec < 0) {
        sec -= 1;
        nsec += 1000000000L;
    }
    long hours = sec / 3600;
    long mins  = (sec % 3600) / 60;
    long secs  = sec % 60;
    snprintf(buf, len, "%02ld:%02ld:%02ld", hours, mins, secs);
}

typedef struct {
    int id;
} passenger_arg_t;

static void *passenger_thread(void *arg_void) {
    passenger_arg_t *arg = (passenger_arg_t *)arg_void;
    int id = arg->id;
    free(arg);

    int explore_time = (rand() % 10) + 1;
    sleep(explore_time);

    pthread_mutex_lock(&ride_mutex);
    shared.exploring--;
    pthread_mutex_unlock(&ride_mutex);

    char tbuf[16];
    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d finished exploring, heading to ticket booth\n", tbuf, id);

    pthread_mutex_lock(&ticket_mutex);
    shared.ticket_queue++;
    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d waiting in ticket queue\n", tbuf, id);
    shared.ticket_queue--;
    pthread_mutex_unlock(&ticket_mutex);

    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d acquired a ticket\n", tbuf, id);

    pthread_mutex_lock(&ride_mutex);
    shared.ride_queue++;
    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d joined the ride queue\n", tbuf, id);
    pthread_cond_broadcast(&ride_cond);
    pthread_mutex_unlock(&ride_mutex);

    sem_wait(&ride_sem);

    int car_id = current_car;
    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d boarded Car %d\n", tbuf, id, car_id + 1);

    pthread_mutex_lock(&ride_mutex);
    shared.ride_queue--;
    shared.on_ride++;
    pthread_mutex_unlock(&ride_mutex);

    sem_wait(&unload_sem[id - 1]);

    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d unboarded Car %d\n", tbuf, id, car_id + 1);

    pthread_mutex_lock(&ride_mutex);
    shared.on_ride--;
    shared.total_served++;
    pthread_mutex_unlock(&ride_mutex);

    return NULL;
}

typedef struct {
    int car_id;
} car_arg_t;

static void *car_thread(void *arg_void) {
    car_arg_t *arg = (car_arg_t *)arg_void;
    int cid = arg->car_id;
    free(arg);

    while (1) {
        struct timespec now, timeout;
        clock_gettime(CLOCK_REALTIME, &now);
        timeout.tv_sec = now.tv_sec + car_wait;
        timeout.tv_nsec = now.tv_nsec;

        pthread_mutex_lock(&ride_mutex);
        shared.car_status[cid] = CAR_WAITING;

        int rc = 0;
        while (shared.ride_queue < 1 && rc != ETIMEDOUT) {
            rc = pthread_cond_timedwait(&ride_cond, &ride_mutex, &timeout);
        }
        if (shared.total_served >= num_passengers) {
            pthread_mutex_unlock(&ride_mutex);
            break;
        }
        shared.car_status[cid] = CAR_LOADING;
        pthread_mutex_unlock(&ride_mutex);

        pthread_mutex_lock(&car_mutex);
        current_car = cid;

        pthread_mutex_lock(&ride_mutex);
        int can_load = (shared.ride_queue < capacity ? shared.ride_queue : capacity);
        shared.car_loaded[cid] = can_load;
        pthread_mutex_unlock(&ride_mutex);

        char tbuf[16];
        get_elapsed(tbuf, sizeof(tbuf));
        if (can_load > 0) {
            printf("[Time: %s] Car %d invoked load()\n", tbuf, cid + 1);
        } else {
            pthread_mutex_unlock(&car_mutex);
            continue;
        }

        for (int i = 0; i < can_load; i++) {
            sem_post(&ride_sem);
            struct timespec pause = {0, 10000000};
            nanosleep(&pause, NULL);
        }

        pthread_mutex_lock(&ride_mutex);
        shared.car_status[cid] = CAR_RUNNING;
        shared.total_rides++;
        pthread_mutex_unlock(&ride_mutex);

        get_elapsed(tbuf, sizeof(tbuf));
        printf("[Time: %s] Car %d departed for its run\n", tbuf, cid + 1);

        pthread_mutex_unlock(&car_mutex);

        sleep(ride_duration);

        pthread_mutex_lock(&ride_mutex);
        shared.car_status[cid] = CAR_WAITING;
        pthread_mutex_unlock(&ride_mutex);

        get_elapsed(tbuf, sizeof(tbuf));
        printf("[Time: %s] Car %d finished its run\n", tbuf, cid + 1);

        get_elapsed(tbuf, sizeof(tbuf));
        printf("[Time: %s] Car %d invoked unload()\n", tbuf, cid + 1);

        int to_unload = shared.car_loaded[cid];
        for (int i = 0; i < to_unload; i++) {
            for (int pid = 0; pid < num_passengers; pid++) {
                sem_post(&unload_sem[pid]);
            }
        }

        pthread_mutex_lock(&ride_mutex);
        shared.car_loaded[cid] = 0;
        pthread_mutex_unlock(&ride_mutex);
    }

    return NULL;
}

static void *monitor_thread(void *_) {
    char tbuf[16];
    while (1) {
        sleep(5);
        get_elapsed(tbuf, sizeof(tbuf));

        pthread_mutex_lock(&ride_mutex);
        int tq = shared.ticket_queue;
        int rq = shared.ride_queue;
        int ex = shared.exploring;
        int onr = shared.on_ride;
        pthread_mutex_unlock(&ride_mutex);

        printf("\n[Monitor] System State at %s\n", tbuf);
        printf("Ticket Queue: %d\n", tq);
        printf("Ride Queue: %d\n", rq);
        for (int i = 0; i < num_cars; i++) {
            int status = shared.car_status[i];
            int loaded = shared.car_loaded[i];
            const char *sstr = (status == CAR_WAITING ? "WAITING" :
                                status == CAR_LOADING ? "LOADING" :
                                status == CAR_RUNNING ? "RUNNING" : "UNKNOWN");
            printf("Car %d Status: %s (%d/%d passengers)\n",
                   i + 1, sstr, loaded, capacity);
        }
        printf("Passengers in park: %d (%d exploring, %d in queues, %d on rides)\n\n",
               ex + rq + onr, ex, tq + rq, onr);

        if (shared.total_served >= num_passengers) {
            break;
        }
    }

    get_elapsed(tbuf, sizeof(tbuf));
    printf("\n[Monitor] FINAL STATISTICS:\n");
    printf("Total simulation time: %s\n", tbuf);
    printf("Total passengers served: %d\n", shared.total_served);
    printf("Total rides completed: %d\n", shared.total_rides);
    printf("Average wait time in ticket queue: N/A\n");
    printf("Average wait time in ride queue: N/A\n");
    printf("Average car utilization: N/A\n");

    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "n:c:p:w:r:")) != -1) {
        switch (opt) {
            case 'n':
                num_passengers = atoi(optarg);
                break;
            case 'c':
                num_cars = atoi(optarg);
                break;
            case 'p':
                capacity = atoi(optarg);
                break;
            case 'w':
                car_wait = atoi(optarg);
                break;
            case 'r':
                ride_duration = atoi(optarg);
                break;
            default:
                fprintf(stderr,
                        "Usage: %s -n <num_passengers> -c <num_cars> -p <capacity> -w <car_wait> -r <ride_duration>\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (num_passengers <= 0 || num_cars <= 0 || capacity <= 0 || car_wait <= 0 || ride_duration <= 0) {
        fprintf(stderr, "All parameters must be positive integers.\n");
        fprintf(stderr,
                "Usage: %s -n <num_passengers> -c <num_cars> -p <capacity> -w <car_wait> -r <ride_duration>\n",
                argv[0]);
        exit(EXIT_FAILURE);
    }
    if (num_cars > MAX_CARS) {
        fprintf(stderr, "Max supported cars is %d\n", MAX_CARS);
        exit(EXIT_FAILURE);
    }
    if (num_passengers > MAX_PASSENGERS) {
        fprintf(stderr, "Max supported passengers is %d\n", MAX_PASSENGERS);
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    clock_gettime(CLOCK_REALTIME, &start_time);

    memset(&shared, 0, sizeof(shared_state_t));
    shared.car_count = num_cars;
    shared.exploring = num_passengers;

    sem_init(&ride_sem, 0, 0);
    for (int i = 0; i < num_passengers; i++) {
        sem_init(&unload_sem[i], 0, 0);
    }

    printf("===== DUCK PARK SIMULATION =====\n");
    printf("[Monitor] Simulation started with parameters:\n");
    printf("- Number of passenger threads: %d\n", num_passengers);
    printf("- Number of cars: %d\n", num_cars);
    printf("- Capacity per car: %d\n", capacity);
    printf("- Park exploration time: 1-10 seconds\n");
    printf("- Car waiting period: %d seconds\n", car_wait);
    printf("- Ride duration: %d seconds\n\n", ride_duration);

    pthread_t mon_tid;
    pthread_create(&mon_tid, NULL, monitor_thread, NULL);

    pthread_t car_threads[MAX_CARS];
    for (int i = 0; i < num_cars; i++) {
        car_arg_t *carg = malloc(sizeof(car_arg_t));
        carg->car_id = i;
        pthread_create(&car_threads[i], NULL, car_thread, carg);
    }

    pthread_t passenger_threads[MAX_PASSENGERS];
    for (int i = 0; i < num_passengers; i++) {
        passenger_arg_t *parg = malloc(sizeof(passenger_arg_t));
        parg->id = i + 1;
        pthread_create(&passenger_threads[i], NULL, passenger_thread, parg);
    }

    for (int i = 0; i < num_passengers; i++) {
        pthread_join(passenger_threads[i], NULL);
    }

    for (int i = 0; i < num_cars; i++) {
        pthread_join(car_threads[i], NULL);
    }

    pthread_join(mon_tid, NULL);

    return 0;
}

