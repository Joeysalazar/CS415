/*
 * part2/park.c: Duck Park Simulation (Part 2 — no monitor)
 *
 * This version is identical to Part 3’s multi‐threaded code, except the monitor thread
 * (and its printing) has been removed. All -n, -c, -p, -w, -r options are supported.
 *
 * Compile with:
 *   gcc -g -Wall -pthread park.c -o park -lm
 *
 * Usage:
 *   ./park -n <num_passengers> -c <num_cars> -p <capacity> -w <car_wait> -r <ride_duration>
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
    int ticket_queue;           // number waiting at ticket booth
    int ride_queue;             // number waiting to board
    int exploring;              // number currently “exploring”
    int on_ride;                // number currently on ride
    int total_served;           // how many have finished a ride
    int total_rides;            // how many departures have occurred
    int car_count;              // number of cars
    int car_status[MAX_CARS];   // status of each car (WAITING/LOADING/RUNNING)
    int car_loaded[MAX_CARS];   // how many boarded this run
} shared_state_t;

/* Command-line parameters */
static int num_passengers = 0;
static int num_cars = 0;
static int capacity = 0;
static int car_wait = 0;
static int ride_duration = 0;

/* Shared state (global, no monitor) */
static shared_state_t shared;

/* Synchronization primitives */
static pthread_mutex_t ticket_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ride_mutex   = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t car_mutex     = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  ride_cond     = PTHREAD_COND_INITIALIZER;

/* Semaphores */
static sem_t ride_sem;                 // signals passengers to board
static sem_t unload_sem[MAX_PASSENGERS]; // signals passengers to unboard

/* Which car is loading (only one at a time) */
static int current_car = -1;

/* Simulation start time (for timestamps) */
static struct timespec start_time;

/* Utility: print elapsed HH:MM:SS */
static void get_elapsed(char *buf, size_t len) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    long sec  = now.tv_sec - start_time.tv_sec;
    long nsec = now.tv_nsec - start_time.tv_nsec;
    if (nsec < 0) {
        sec  -= 1;
        nsec += 1000000000L;
    }
    long h = sec / 3600;
    long m = (sec % 3600) / 60;
    long s = sec % 60;
    snprintf(buf, len, "%02ld:%02ld:%02ld", h, m, s);
}

typedef struct {
    int id;
} passenger_arg_t;

/* Passenger thread function */
static void *passenger_thread(void *arg_void) {
    passenger_arg_t *arg = (passenger_arg_t *)arg_void;
    int id = arg->id;
    free(arg);

    /* 1) “Explore” for 1–10 seconds */
    int explore_time = (rand() % 10) + 1;
    sleep(explore_time);

    /* Mark “exploring” → 0 */
    pthread_mutex_lock(&ride_mutex);
      shared.exploring--;
    pthread_mutex_unlock(&ride_mutex);

    char tbuf[16];
    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d finished exploring, heading to ticket booth\n", tbuf, id);

    /* 2) Ticket booth (only one at a time) */
    pthread_mutex_lock(&ticket_mutex);
      shared.ticket_queue++;
      get_elapsed(tbuf, sizeof(tbuf));
      printf("[Time: %s] Passenger %d waiting in ticket queue\n", tbuf, id);
      /* “Acquire ticket” immediately */
      shared.ticket_queue--;
    pthread_mutex_unlock(&ticket_mutex);

    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d acquired a ticket\n", tbuf, id);

    /* 3) Join ride queue */
    pthread_mutex_lock(&ride_mutex);
      shared.ride_queue++;
      get_elapsed(tbuf, sizeof(tbuf));
      printf("[Time: %s] Passenger %d joined the ride queue\n", tbuf, id);
      pthread_cond_broadcast(&ride_cond);
    pthread_mutex_unlock(&ride_mutex);

    /* 4) Wait to be signaled to board */
    sem_wait(&ride_sem);

    int car_id = current_car;
    get_elapsed(tbuf, sizeof(tbuf));
    printf("[Time: %s] Passenger %d boarded Car %d\n", tbuf, id, car_id + 1);

    pthread_mutex_lock(&ride_mutex);
      shared.ride_queue--;
      shared.on_ride++;
    pthread_mutex_unlock(&ride_mutex);

    /* 5) Wait for unload signal */
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

/* Car thread function */
static void *car_thread(void *arg_void) {
    car_arg_t *arg = (car_arg_t *)arg_void;
    int cid = arg->car_id;
    free(arg);

    while (1) {
        /* 1) Wait until at least one passenger or timeout */
        struct timespec now, timeout;
        clock_gettime(CLOCK_REALTIME, &now);
        timeout.tv_sec  = now.tv_sec + car_wait;
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

        /* Only one car loads at a time */
        pthread_mutex_lock(&car_mutex);
          current_car = cid;

          pthread_mutex_lock(&ride_mutex);
            int can_load = (shared.ride_queue < capacity)
                               ? shared.ride_queue
                               : capacity;
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

          /* Signal “can_load” passengers to board */
          for (int i = 0; i < can_load; i++) {
              sem_post(&ride_sem);
              /* tiny pause so that prints don’t jumble */
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

        /* 2) Simulate the ride */
        sleep(ride_duration);

        /* 3) Unload */
        pthread_mutex_lock(&ride_mutex);
          shared.car_status[cid] = CAR_WAITING;
        pthread_mutex_unlock(&ride_mutex);

        char tbuf2[16];
        get_elapsed(tbuf2, sizeof(tbuf2));
        printf("[Time: %s] Car %d finished its run\n", tbuf2, cid + 1);
        get_elapsed(tbuf2, sizeof(tbuf2));
        printf("[Time: %s] Car %d invoked unload()\n", tbuf2, cid + 1);

        int to_unload = shared.car_loaded[cid];
        for (int i = 0; i < to_unload; i++) {
            /* Broadcast enough posts so that exactly 'to_unload' waiting passengers wake */
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

/* main() parses arguments, spawns threads, but no monitor thread here */
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
                        "Usage: %s -n <num_passengers> -c <num_cars> -p <capacity> "
                        "-w <car_wait> -r <ride_duration>\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    if (num_passengers <= 0 || num_cars <= 0 || capacity <= 0 ||
        car_wait <= 0 || ride_duration <= 0) {
        fprintf(stderr,
                "All parameters must be positive integers.\n"
                "Usage: %s -n <num_passengers> -c <num_cars> -p <capacity> "
                "-w <car_wait> -r <ride_duration>\n",
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

    /* Initialize shared state */
    memset(&shared, 0, sizeof(shared_state_t));
    shared.car_count = num_cars;
    shared.exploring = num_passengers;

    /* Initialize semaphores */
    sem_init(&ride_sem, 0, 0);
    for (int i = 0; i < num_passengers; i++) {
        sem_init(&unload_sem[i], 0, 0);
    }

    printf("===== DUCK PARK SIMULATION (Part 2) =====\n");
    printf("- #passengers: %d, #cars: %d, capacity: %d, wait: %d, ride: %d\n",
           num_passengers, num_cars, capacity, car_wait, ride_duration);

    /* Create car threads */
    pthread_t car_threads[MAX_CARS];
    for (int i = 0; i < num_cars; i++) {
        car_arg_t *carg = malloc(sizeof(car_arg_t));
        carg->car_id = i;
        pthread_create(&car_threads[i], NULL, car_thread, carg);
    }

    /* Create passenger threads */
    pthread_t passenger_threads[MAX_PASSENGERS];
    for (int i = 0; i < num_passengers; i++) {
        passenger_arg_t *parg = malloc(sizeof(passenger_arg_t));
        parg->id = i + 1;
        pthread_create(&passenger_threads[i], NULL, passenger_thread, parg);
    }

    /* Join passengers */
    for (int i = 0; i < num_passengers; i++) {
        pthread_join(passenger_threads[i], NULL);
    }

    /* Join cars */
    for (int i = 0; i < num_cars; i++) {
        pthread_join(car_threads[i], NULL);
    }

    return 0;
}

