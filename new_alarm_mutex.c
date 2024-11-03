/*
 * alarm_mutex.c
 *
 * This is an enhancement to the alarm_thread.c program, which
 * created an "alarm thread" for each alarm command. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of absolute expiration time. The list is
 * protected by a mutex, and the alarm thread sleeps for at
 * least 1 second, each iteration, to ensure that the main
 * thread can lock the mutex to add new work to the list.
 */
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include "errors.h"

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag
{
    struct alarm_tag *link;
    int id;
    int seconds;
    char type[2];
    time_t time; /* seconds from EPOCH */
    char message[128];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;

/*
 * The alarm thread's start routine.
 */
void *alarm_thread(void *arg)
{
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status;
    unsigned long main_thread = *(unsigned long*)arg;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits.
     */
    while (1)
    {
        status = pthread_mutex_lock(&alarm_mutex);
        if (status != 0)
            err_abort(status, "Lock mutex");
        alarm = alarm_list;

        /*
         * If the alarm list is empty, wait for one second. This
         * allows the main thread to run, and read another
         * command. If the list is not empty, remove the first
         * item. Compute the number of seconds to wait -- if the
         * result is less than 0 (the time has passed), then set
         * the sleep_time to 0.
         */
        if (alarm == NULL)
            sleep_time = 1;
        else
        {
            alarm_list = alarm->link;
            now = time(NULL);
            if (alarm->time <= now)
                sleep_time = 0;
            else
                sleep_time = alarm->time - now;
#ifdef DEBUG
            printf("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                   sleep_time, alarm->message);
#endif
        }

        /*
         * Unlock the mutex before waiting, so that the main
         * thread can lock it to insert a new alarm request. If
         * the sleep_time is 0, then call sched_yield, giving
         * the main thread a chance to run if it has been
         * readied by user input, without delaying the message
         * if there's no input.
         */
        status = pthread_mutex_unlock(&alarm_mutex);
        /*if display flag is zero, then the alarm is fresh and we will dispaly it*/

        if (status != 0)
            err_abort(status, "Unlock mutex");

        if (sleep_time > 0) {
            sleep(sleep_time);
        }
        else
            sched_yield();

        /*
         * If a timer expired, print the message and free the
         * structure.
         */
        if (alarm != NULL)
        {
            printf("(%d) %s\n", alarm->seconds, alarm->message);
            free(alarm);
        }
    }
}

void linked_list(alarm_t *alarm, unsigned long main_thread, alarm_t *next, alarm_t **last, int *status) {
    last = &alarm_list;
    next = *last;

    // Lock the mutex for thread safety
    *status = pthread_mutex_lock(&alarm_mutex);
    if (*status != 0) {
        err_abort(*status, "Lock mutex");
    }

    // Set the alarm time based on the current time and the specified seconds
    alarm->time = time(NULL) + alarm->seconds;

    // Insert the new alarm in the list sorted by expiration time
    while (next != NULL) {
        if (next->time >= alarm->time) {
            alarm->link = next;
            *last = alarm;
            break;
        }
        last = &(next->link);
        next = next->link;
    }

    // If we reached the end of the list, add the alarm at the end
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }

#ifdef DEBUG
    printf("[list: ");
    for (next = alarm_list; next != NULL; next = next->link)
        printf("%d(%d)[\"%s\"] ", next->time, next->time - time(NULL), next->message);
    printf("]\n");
#endif

    /*A.3.2.1 - For each valid Start_Alarm request received, the main thread will insert the
            corresponding alarm with the specified Alarm_ID into the alarm list, in which all the
            alarms are placed in the order of their Alarm_IDs. Then the main thread will print:
            “Alarm( <alarm_id>) Inserted by Main Thread (<thread-id>) Into Alarm List at
            <insert_time>: <time message>”.
            */

    // Print insertion message for each valid Start_Alarm request
    printf("Alarm(%d) Inserted by Main Thread (%lu) Into Alarm List at %lu: %s\n",
           alarm->id, main_thread, (unsigned long)time(NULL), alarm->message);

    // Unlock the mutex
    *status = pthread_mutex_unlock(&alarm_mutex);
    if (*status != 0) {
        err_abort(*status, "Unlock mutex");
    }
}

int input_validator(const char *keyword, int user_arg ) {
    //if input is valid, return flag that corresponds to the keyword
    if((strcmp(keyword, "Cancel_Alarm") == 0) && (user_arg == 2)) {
        return 1;
    }
    if((strcmp(keyword, "View_Alarm") == 0) && (user_arg == 1)) {
        return 2;
    }
    if((strcmp(keyword, "Start_Alarm") == 0) && (user_arg == 5)) {
        return 3;
    }
    if((strcmp(keyword, "Change_Alarm") == 0) && (user_arg == 5)) {
        return 4;
    }
    //otherwise we return 1
    return -1;
}

int main(int argc, char *argv[])
{
    //new
    char keyword[128];
    int user_arg;
    unsigned long main_thread = pthread_self();
    int flag_input;
    //given
    int status;
    char line[128];
    alarm_t *alarm, **last, *next;
    pthread_t thread;

    status = pthread_create(
        &thread, NULL, alarm_thread, &main_thread);
    if (status != 0)
        err_abort(status, "Create alarm thread");
    while (1)
    {
        printf("alarm> ");
        if (fgets(line, sizeof(line), stdin) == NULL) //user input is ctrl + D
            exit(0);
        if (strlen(line) <= 1) //user hit enter
            continue;
        alarm = (alarm_t *)malloc(sizeof(alarm_t));
        if (alarm == NULL)
            errno_abort("Allocate alarm");

        /*
        *We will first check whether the format of the alarm request
        *is consistent with either Cancel_Alar, View_Alarm, Start_Alarm, or Change_Alarm;
        *otherwise the alarm request will be rejected with an error message.
        *If a Message exceeds 128 characters, it will be truncated to 128 characters.
         */

        /*Input validator*/
        //user_arg has the number of arguments passed from stdin
        user_arg = sscanf(line, "%[^(\n](%d): T%d %d %128[^\n]", keyword, &alarm->id, &alarm->type, &alarm->seconds, alarm->message);
        flag_input = input_validator(keyword, user_arg);
        if (flag_input == -1) {
            fprintf(stderr, "Bad command\n");
            free(alarm);
        }

        else {
            printf("flag: %d\n", flag_input);
            linked_list(alarm, main_thread, next, last, &status);
        }
    }
}