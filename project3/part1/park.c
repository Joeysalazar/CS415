#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

int capacity = 1;
int car_passengers = 0;
sem_t load_sem;
sem_t unload_sem;
pthread_mutex_t car_mutex = PTHREAD_MUTEX_INITIALIZER;

void *car_thread(void *arg) {
    // Invoke load
    printf("[Car] Invoking load()\n");
    sem_post(&load_sem);  // Signal passenger to board

    // Wait until passenger has boarded
    sleep(1);  // simulate boarding delay

    // Car departs (run)
    printf("[Car] Running with %d passenger(s)\n", car_passengers);
    sleep(2);  // ride duration

    // Invoke unload
    printf("[Car] Invoking unload()\n");
    sem_post(&unload_sem);  // Signal passenger to unboard

    return NULL;
}

void *passenger_thread(void *arg) {
    // Wait for car to invoke load
    sem_wait(&load_sem);
    pthread_mutex_lock(&car_mutex);
    car_passengers++;
    printf("[Passenger] Boarded the car\n");
    pthread_mutex_unlock(&car_mutex);

    // Wait for car to invoke unload
    sem_wait(&unload_sem);
    pthread_mutex_lock(&car_mutex);
    car_passengers--;
    printf("[Passenger] Unboarded the car\n");
    pthread_mutex_unlock(&car_mutex);

    return NULL;
}

int main() {
    pthread_t carTid, passTid;
    // Initialize semaphores
    sem_init(&load_sem, 0, 0);
    sem_init(&unload_sem, 0, 0);

    // Create threads
    pthread_create(&carTid, NULL, car_thread, NULL);
    pthread_create(&passTid, NULL, passenger_thread, NULL);

    // Wait for both to finish
    pthread_join(passTid, NULL);
    pthread_join(carTid, NULL);

    // Clean up
    sem_destroy(&load_sem);
    sem_destroy(&unload_sem);
    pthread_mutex_destroy(&car_mutex);

    printf("Simulation (Part 1) complete.\n");
    return 0;
}
