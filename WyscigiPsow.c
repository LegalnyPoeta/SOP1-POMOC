#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

void usage(char *prog)
{
    fprintf(stderr, "Usage: %s <n> <m>\n", prog);
    fprintf(stderr, "Where:\n");
    fprintf(stderr, "\t<n> : length of a track (must be > 20)\n");
    fprintf(stderr, "\t<m> : number of dogs in the race (must be > 2)\n");
    exit(EXIT_FAILURE);
}
volatile sig_atomic_t keep_running = 1;

void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        ERR("sigaction");
}

void sig_handler(int sig)
{
    if (sig == SIGINT)
        keep_running = 0;
}

typedef struct ArgsDog
{
    int id;
    unsigned int seed;
    int *track;
    int track_length;
    pthread_mutex_t *track_mutex;
} ArgsDog;

void cleanup_handler(void *arg)
{
    ArgsDog *arguments = (ArgsDog *)arg;
    for (int i = 0; i <= arguments->track_length; i++)
    {
        if (pthread_mutex_trylock(&arguments->track_mutex[i]) == EBUSY)
            pthread_mutex_unlock(&arguments->track_mutex[i]);
    }
}

void *dog_work(void *arg)
{
    ArgsDog *arguments = (ArgsDog *)arg;
    int dog_position = 0;
    int old_dog_position = -1;
    pthread_mutex_lock(&arguments->track_mutex[dog_position]);
    arguments->track[dog_position]++;
    pthread_mutex_unlock(&arguments->track_mutex[dog_position]);
    int *place = malloc(sizeof(int));
    pthread_cleanup_push(cleanup_handler, arg);
    pthread_cleanup_push(free, place);
    while (dog_position != arguments->track_length)
    {
        int time_to_sleep = 200 + (rand_r(&arguments->seed) % 1321);
        int forward_steps = 1 + (rand_r(&arguments->seed) % 5);
        old_dog_position = dog_position;

        dog_position += forward_steps;
        if (dog_position > arguments->track_length)
            dog_position = arguments->track_length - (dog_position - arguments->track_length);
        int first = old_dog_position < dog_position ? old_dog_position : dog_position;
        int last = old_dog_position > dog_position ? old_dog_position : dog_position;
        pthread_mutex_lock(&arguments->track_mutex[first]);
        if (first != last)
            pthread_mutex_lock(&arguments->track_mutex[last]);
        if (arguments->track[dog_position] > 0 && dog_position != arguments->track_length)
        {
            printf("waf waf waf!\n");
            dog_position = old_dog_position;
        }
        else
        {
            arguments->track[old_dog_position]--;

            arguments->track[dog_position]++;

            printf("Dog %d is at position %d\n", arguments->id, dog_position);
            if (dog_position == arguments->track_length)
            {
                printf("Dog %d finished the race! Place: %d\n", arguments->id, arguments->track[arguments->track_length]);
                *place = arguments->track[arguments->track_length];
            }
        }
        if (first != last)
            pthread_mutex_unlock(&arguments->track_mutex[last]);
        pthread_mutex_unlock(&arguments->track_mutex[first]);
        usleep(time_to_sleep * 1000);
    }
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(0);
    return place;
}

int main(int argc, char **argv)
{
    srand(time(NULL));
    if (argc != 3)
        usage(argv[0]);

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    if (n <= 20 || m <= 2)
        usage(argv[0]);

    int *track = malloc(sizeof(int) * n);
    ArgsDog *arguments = malloc(sizeof(ArgsDog) * m);
    pthread_mutex_t *track_mutex = malloc(sizeof(pthread_mutex_t) * n);

    if (track == NULL || arguments == NULL || track_mutex == NULL)
        ERR("malloc");

    for (int i = 0; i < n; i++)
    {
        track[i] = 0;
        pthread_mutex_init(&track_mutex[i], NULL);
    }

    pthread_t dogs[m];
    for (int i = 0; i < m; i++)
    {
        arguments[i].id = i + 1;
        arguments[i].seed = rand();
        arguments[i].track = track;
        arguments[i].track_length = n - 1;
        arguments[i].track_mutex = track_mutex;
        pthread_create(&dogs[i], NULL, dog_work, &arguments[i]);
    }
    sethandler(sig_handler, SIGINT);
    while (track[n - 1] < m)
    {
        usleep(1000000);
        for (int i = 0; i < n; i++)
        {
            pthread_mutex_lock(&track_mutex[i]);
            printf("%d ", track[i]);
            pthread_mutex_unlock(&track_mutex[i]);
        }
        printf("\n");
        if (!keep_running)
        {
            for (int i = 0; i < m; i++)
            {
                pthread_cancel(dogs[i]);
            }
            printf("The race was interrupted!\n");
            break;
        }
    }
    int ranking[m + 1];
    for (int i = 0; i < m; i++)
    {
        int *place;
        pthread_join(dogs[i], (void **)&place);
        if (place != NULL && place != PTHREAD_CANCELED)
        {
            ranking[*place] = i + 1;
            free(place);
        }
    }
    if (keep_running)
        printf("Podium:\n1st place: Dog %d\n2nd place: Dog %d\n3rd place: Dog %d\n", ranking[1], ranking[2], ranking[3]);

    for (int i = 0; i < n; i++)
        pthread_mutex_destroy(&track_mutex[i]);
    free(track_mutex);
    free(track);
    free(arguments);

    return EXIT_SUCCESS;
}