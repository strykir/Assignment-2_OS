/*
 * new_alarm_mutex.c
 *
 * This is an enhancement to the alarm_mutex.c program, which
 * created an "alarm thread" for each alarm command. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of absolute expiration time. The list is
 * protected by a mutex, and the alarm thread sleeps for at
 * least 1 second, each iteration, to ensure that the main
 * thread can lock the mutex to add new work to the list.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 type;
    int                 id;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    char                message[64];
    char                req[13];
    int                 cancelled;

} alarm_t;

//So alarm_thread can look for display threads, link display threads together as nodes in a list
//Each node also contains the alarm type, alarms, and num of alarms for a given display thread.
//This gives each display thread access to its own data
//But probably have to treat display_alarms same as alarm_list in terms of synchronization
typedef struct display_thread_node{

    int num_of_alarms;
    int type;
    int end_of_life;
    pthread_t display_thread;
    struct alarm_tag *display_alarms[2]; //list of display alarms
    struct display_t *link; //link to next display thread in list

} display_t;



pthread_mutex_t new_alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t no_new_alarm = PTHREAD_COND_INITIALIZER;
alarm_t *alarm_list = NULL;
display_t *display_threads = NULL; 
time_t current_alarm = 0;
alarm_t *new_alarm = NULL;



//if you lock alarm_mutex...then we in deadlock
void *display_thread (void *arg){

  int avaiable = 0;
  int type;  
  int* num_of_alarms = malloc(sizeof(int));
  int status;
  
  time_t current_0, start_0, current_1, start_1; 
  display_t* thread_data = (display_t*)arg;
  
  /*status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");
 */
 time(&start_0);
 time(&start_1);
  while (1){
     
    //Display thread runs every 5 seconds...I think. It displays message every 5 seconds for sure
    //May have to add another thread to handle printing 


    //Originally planned to have this thread lock alarm mutex and run whenever no new alarm is added, but this prevents
    //main thread from running
    /*  while(new_alarm != NULL){
            printf("display thread waiting");
            status = pthread_cond_wait (&no_new_alarm, &alarm_mutex); 
            
            if (status != 0)
            err_abort (status, "Wait on cond");
        }
    */
     
    if(thread_data->display_alarms[0] == NULL && thread_data->display_alarms[1] == NULL){
      printf("Display Thread Terminated (%d) at %ld \n", pthread_self(), time(NULL));
      thread_data->end_of_life = 1;
    return NULL;
    }
   
    /* A.3.4.3. if the alarm type of an alarm assigned the display thread in the alarm list
       * has been changed, then the display thread will stop printing the message in that
       * alarm. Then the display thread will print:
       */ 
     
      if(thread_data->display_alarms[0] != NULL && thread_data->display_alarms[0]->type != thread_data->type){ 
        printf("Alarm(%d) Changed Type; Display Thread (%d) Stopped Printing Alarm Message at %ld: %d %d %s \n", thread_data->display_alarms[0]->id, pthread_self(), time(NULL), thread_data->display_alarms[0]->type, thread_data->display_alarms[0]->seconds, thread_data->display_alarms[0]->message);
        thread_data->display_alarms[0] = NULL;
        num_of_alarms = num_of_alarms -1 ;
      }  

      /* A.3.4.2. if an alarm assigned the display thread in the alarm list has been cancelled,
      * then the display thread will stop printing the message in that alarm. Then the display
      * thread will print:
      */

      if(thread_data->display_alarms[0] != NULL && thread_data->display_alarms[0]->cancelled == 1){
        printf("Alarm(%d) Cancelled; Display Thread (%d) Stopped Printing Alarm Message at %ld: %d %d %s \n", thread_data->display_alarms[0]->id, pthread_self(), time(NULL), thread_data->display_alarms[0]->type, thread_data->display_alarms[0]->seconds, thread_data->display_alarms[0]->message);
        free(thread_data->display_alarms[0]);
        thread_data->display_alarms[0] = NULL;
        num_of_alarms = num_of_alarms -1 ;
      }  

      /*  A.3.4.1. If the expiry time of an alarm assigned to the display thread in the alarm list
      *  has been reached, then the display thread will stop printing the message in that
      *  alarm. Then the display thread will print:
      */
     if(thread_data->display_alarms[0] != NULL && thread_data->display_alarms[0]->time < time(NULL)){
            //remove alarm from num_of_alarms           
            printf("Alarm(%d) Expired; Display Thread (%d) Stopped Printing Alarm Message at %ld: %d %d %s \n", thread_data->display_alarms[0]->id, pthread_self(), time(NULL), thread_data->display_alarms[0]->type, thread_data->display_alarms[0]->seconds, thread_data->display_alarms[0]->message);
            thread_data->display_alarms[0] = NULL;
            num_of_alarms = num_of_alarms -1 ;

        }

    if(thread_data->display_alarms[0] != NULL){

      /* A.3.4.5. For each alarm with an alarm type which the display thread is responsible
       * for and the alarm has been assigned by the alarm thread to that display thread, the
       * display thread will periodically print, every five (5) seconds, the message in that
       * alarm as follows:
       */
        time(&current_0);
        if (difftime(current_0, start_0) >= 90.0){
         printf("Alarm(%d) Message PERIODICALLY PRINTED BY Display Thread (%d) at %ld: %d %d %s \n", thread_data->display_alarms[0]->id, pthread_self(), time(NULL), thread_data->display_alarms[0]->type, thread_data->display_alarms[0]->seconds, thread_data->display_alarms[0]->message);
         start_0 = current_0;
        }
   }
    
     /* A.3.4.3. if the alarm type of an alarm assigned the display thread in the alarm list
       * has been changed, then the display thread will stop printing the message in that
       * alarm. Then the display thread will print:
       */ 
      if(thread_data->display_alarms[1] != NULL && thread_data->display_alarms[1]->type != thread_data->type){
        printf("Alarm(%d) Changed Type; Display Thread (%d) Stopped Printing Alarm Message at %ld: %d %d %s \n", thread_data->display_alarms[1]->id, pthread_self(), time(NULL), thread_data->display_alarms[1]->type, thread_data->display_alarms[1]->seconds, thread_data->display_alarms[1]->message);
        thread_data->display_alarms[1] = NULL;
        num_of_alarms = num_of_alarms -1 ;
      }  
      
     /* A.3.4.2. if an alarm assigned the display thread in the alarm list has been cancelled,
      * then the display thread will stop printing the message in that alarm. Then the display
      * thread will print:
      */

      if(thread_data->display_alarms[1] != NULL && thread_data->display_alarms[0]->cancelled == 1){
        printf("Alarm(%d) Cancelled; Display Thread (%d) Stopped Printing Alarm Message at %ld: %d %d %s \n", thread_data->display_alarms[1]->id, pthread_self(), time(NULL), thread_data->display_alarms[1]->type, thread_data->display_alarms[1]->seconds, thread_data->display_alarms[1]->message);
        free(thread_data->display_alarms[1]);
        thread_data->display_alarms[1] = NULL;
        num_of_alarms = num_of_alarms -1 ;
      } 
     
     /*  A.3.4.1. If the expiry time of an alarm assigned to the display thread in the alarm list
      *  has been reached, then the display thread will stop printing the message in that
      *  alarm. Then the display thread will print:
      */
     if(thread_data->display_alarms[1] != NULL && thread_data->display_alarms[1]->time < time(NULL)){
            //remove alarm from num_of_alarms           
            printf("Alarm(%d) Expired; Display Thread (%d) Stopped Printing Alarm Message at %ld: %d %d %s \n", thread_data->display_alarms[1]->id, pthread_self(), time(NULL), thread_data->display_alarms[1]->type, thread_data->display_alarms[1]->seconds, thread_data->display_alarms[1]->message);
            thread_data->display_alarms[1] = NULL;
            num_of_alarms = num_of_alarms -1 ;

        }

    if(thread_data->display_alarms[1] != NULL){

      /* A.3.4.5. For each alarm with an alarm type which the display thread is responsible
       * for and the alarm has been assigned by the alarm thread to that display thread, the
       * display thread will periodically print, every five (5) seconds, the message in that
       * alarm as follows:
       */
        time(&current_1);
        if (difftime(current_1, start_1) >= 90.0){
         printf("Alarm(%d) Message PERIODICALLY PRINTED BY Display Thread (%d) at %ld: %d %d %s \n", thread_data->display_alarms[1]->id, pthread_self(), time(NULL), thread_data->display_alarms[1]->type, thread_data->display_alarms[1]->seconds, thread_data->display_alarms[1]->message);
         start_1 = current_1;
        }
    }
 

  

    /*status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
    */        
          
  }
  

}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *current_alarms;
    
    display_t *new, *next_thread, **last_thread;
    int sleep_time;
    time_t now;
    int status;
    int capacity;
    int count = 0;
    display_t *new_display_thread; //new display thread

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits.
     */

    status = pthread_mutex_lock (&new_alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");
    while (1) {

        //If list is empty or no new alarms nothing to do just wait
        //Should also wait on main thread to signal when new alarm or type change happened
        
        while(new_alarm == NULL){
            
          status = pthread_cond_wait (&alarm_cond, &new_alarm_mutex);
            
            if (status != 0)
            err_abort (status, "Wait on cond");
        }
       
        

        /*
           A.3.3.1. For each newly inserted alarm or newly changed alarm with a type change
           in the alarm list, if no display threads responsible for the alarm type of the alarm
           currently exist, then create a new display thread for the alarm type of the alarm.
           Then the alarm thread will print:
           “First New Display Thread(<thread_id>) Created at <creation_time>: <type time
            message>”.    
        */

        /*
          *  A.3.3.2. For each newly inserted alarm or newly changed alarm with a type change
          *  in the alarm list, if all existing display threads responsible for the alarm type of the
          *  alarm have already been assigned two (2) alarms, then create a new display thread
          *  responsible for the alarm type of the alarm.Then the alarm thread will print:
          *  “Additional New Display Thread(<thread_id> Created at <creation_time>: <type time message>”.  
       */ 

            last_thread = &display_threads;
            next_thread = *last_thread;

            while (next_thread != NULL) {
                //if display_thread is same type and hass less than 2 alarms
                if (next_thread->display_thread != NULL && new_alarm->type == next_thread->type && next_thread -> num_of_alarms < 2) {
                    

                    if(next_thread -> display_alarms[1] ==NULL)
                    next_thread -> display_alarms[1] = new_alarm;
                    else
                    next_thread -> display_alarms[0] = new_alarm;
                    //assign this alarm to display thread
                    next_thread -> num_of_alarms = next_thread -> num_of_alarms +1;
                    
                    break;
                }
                last_thread = &next_thread->link;
                next_thread = next_thread->link;
            }

            //Either no display threads, or no display threads for this type... create new display thread
            if (next_thread == NULL) {
                
                //allocate memory for new display_thread_node
                new_display_thread = malloc(sizeof(display_t));
                
             
                
               

                *last_thread = new_display_thread;

              
                new_display_thread-> end_of_life = 0;
                new_display_thread -> num_of_alarms = new_display_thread -> num_of_alarms + 1;
                new_display_thread -> type = new_alarm->type;
                new_display_thread -> display_alarms[0] = new_alarm;

                status = pthread_create (&new_display_thread->display_thread, NULL, display_thread, new_display_thread);
                if (status != 0)
                err_abort (status, "Create display thread");
                
            
                
            }
           
           

            //after inserting new alarm, set new alarm pointer to null to signal to other display threads that they can use alarm list
            new_alarm = NULL;

            status = pthread_cond_signal(&no_new_alarm); 
            if (status != 0)
            err_abort (status, "Signal cond");
           
      

        /*
         * Unlock the mutex before waiting, so that the main
         * thread can lock it to insert a new alarm request. If
         * the sleep_time is 0, then call sched_yield, giving
         * the main thread a chance to run if it has been
         * readied by user input, without delaying the message
         * if there's no input.
         */
        /* status = pthread_mutex_unlock (&new_alarm_mutex);
           if (status != 0)
            err_abort (status, "Unlock mutex");
        */

       
    }
}

int input_validator(const char *keyword, int user_arg ) {
    //if input is valid, return flag that corresponds to the keyword
    if((strcmp(keyword, "Cancel_Alarm") == 0) && (user_arg == 2)) {
        return 1;
    }
    if((strcmp(keyword, "View_Alarms") == 0) && (user_arg == 1)) {
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

int main (int argc, char *argv[])
{
    int status;
    int first;
    int counter;
    int user_arg;
    char a_or_b = 'a';
    char line[128];
    char keyword[13];
    int flag_input;
    alarm_t *alarm, **last, *next;
    display_t *new, *next_thread, **last_thread;
    pthread_t thread;
    int sleep_time;
    time_t now;

    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
        

    while (1) {
        printf ("alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");
        
        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        //user_arg = sscanf(line, "%[^(\n](%d): T%s %d %128[^\n]", keyword, &alarm->id, alarm->type, &alarm->seconds, alarm->message);
        //flag_input = input_validator(keyword, user_arg);

        if (sscanf (line, "%[^(\n](%d): T%s %d %128[^\n]", 
            alarm->req, &alarm->id, &alarm->type, &alarm->seconds, alarm->message) < 1) {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        } else {
           
            status = pthread_mutex_lock (&new_alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            
            alarm->time = time (NULL) + alarm->seconds;
            alarm -> cancelled = 0;

            //Check for dead display threads
           last_thread = &display_threads;
           next_thread = *last_thread;

           while (next_thread != NULL) {
                //if display_thread is dead, remove from list. 
                if (next_thread->end_of_life == 1) {


                    *last_thread = next_thread->link;

                    display_t *temp = next_thread;
                    next_thread = next_thread->link;
                    free(temp);

                }

                last_thread = &next_thread->link;
                next_thread = next_thread->link;
            }

            //Main thread now responsible for alarm removal

            /*
             * A3.2.4. For each alarm in the alarm list, if the specified number of n seconds has expired,
             * then the main thread will remove that alarm from the alarm list, and it will print:
             * “Alarm(<alarm_id>): Alarm Expired at <time>: Alarm Removed From Alarm List ”,
             * where <time> is the actual time at which this was printed (<time> is expressed as the
             * number of seconds from the Unix Epoch Jan 1 1970 00:00.
            */
            last = &alarm_list;
            next = *last;

            while (next != NULL) {

                if (next->time <= time(NULL)) {

                    (*last) = next->link;
                    free(next);
                    break;
                }

                last = &next->link;
                next = next->link;
            }

            /*
             * A.3.2.1. For each valid Start_Alarm request received, the main thread will insert the
             * corresponding alarm with the specified Alarm_ID into the alarm list, in which all the
             * alarms are placed in the order of their Alarm_IDs. Then the main thread will print:
             * “Alarm( <alarm_id>) Inserted by Main Thread (<thread-id>) Into Alarm List at
             * <insert_time>: <time message>”.

            */
            if (strcmp(alarm->req, "Start_Alarm") == 0){
            
            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
             */

            //last is the address of the alarm_list pointer
            last = &alarm_list;
            //next is the alarm_list pointer itself
            next = *last;
            while (next != NULL) {
                if (next->id <= alarm->id) {
                    alarm->link = next;
                    *last = alarm;
                    break;
                }
                last = &next->link; //address of next node's link
                next = next->link;
            }
            /*
             * If we reached the end of the list, insert the new
             * alarm there. ("next" is NULL, and "last" points
             * to the link field of the last item, or to the
             * list header).
             */
            if (next == NULL) {
                *last = alarm;
                alarm->link = NULL;
            }
            
            printf("Alarm(%d) Inserted by Main Thread (%d) Into Alarm List at <%ld>: %d %s \n", alarm->id, pthread_self(), time (NULL), alarm->seconds, alarm->message);
            
            new_alarm = alarm;
             
            //signal alarm_cond because new alarm inserted
            status = pthread_cond_signal(&alarm_cond); 
            
            if (status != 0)
            err_abort (status, "Signal cond");
            

            }

           /*
            * A.3.2.2. For each valid Change_Alarm request received, the main thread will use the
            * specified Type, Time and Message values in the Change_Alarm request to replace the
            * Type, Time and Message values in the alarm with the specified Alarm_Id in the alarm list.
            * Then the main thread will print:
            * Alarm(<alarm_id>) Changed at <change_time>: <type time message>”.
           */

            if(strcmp(alarm->req, "Change_Alarm") == 0){
            
            last = &alarm_list;
            next = *last;
            while (next != NULL) {
                if (next->id == alarm->id) {
                    next->type = alarm->type;
                    next->time = alarm->time;
                    strcpy(next->message, alarm->message);

                   
                    break;
                }
                
                last = &next->link;
                next = next->link;
            }

            printf("Alarm(%d) Changed at %ld: %d %d %s \n", next->id, time(NULL), next->type, next->seconds, next->message);
            
            new_alarm = next;
            //signal alarm_cond because alarm changed
            status = pthread_cond_signal(&alarm_cond); 
            if (status != 0)
            err_abort (status, "Signal cond");

            }

            /*
             *  A.3.2.3. For each valid Cancel_Alarm request received, the main thread will remove the
             *  alarm with the specified Alarm_Id from the alarm list. Then the main thread will print:
             *  Alarm(<alarm_id>) Cancelled at <cancel_time>: <type time message>”. 
            */

            if(strcmp(alarm->req, "Cancel_Alarm") == 0){
            first = 1;
            last = &alarm_list;
            next = *last;
            while (next != NULL) {

                if (next->id == alarm->id) {
                    
                    next -> cancelled = 1;
                    (*last) = next->link;
                    free(alarm);
                    //free(next);

                 
                    break;
                }
                first = 0;
                last = &next->link;
                next = next->link;
            }

            printf("Alarm(%d) cancelled at %ld: %d %d %s \n", alarm->id, time(NULL), alarm->type, alarm->seconds, alarm->message);

            }
            
            /*
             *  A3.2.5. For each View_Alarms request received, the main thread will print out the
             *  following:
             *  - A list of all the current existing display threads, together with the alarms in the alarm
             *  list that the alarm thread has assigned to each display thread, in the following format:
            */
             if(strcmp(alarm->req, "View_Alarms") == 0){
             printf("View Alarms at %ld: <%ld>:\n", time(NULL), time(NULL));
               last_thread = &display_threads;
               next_thread = *last_thread;
               counter = 1;

              while (next_thread != NULL) {
                //if display_thread is same type and hass less than 2 alarms
                if (next_thread->display_thread != NULL) {

                    printf("%d. Display Thread <%ld> Assigned:\n", counter, next_thread->display_thread);

                    if (next_thread -> display_alarms[0] != NULL){
                    printf("%d%c. Alarm(%d): %d %ld %s\n", counter, a_or_b, next_thread->display_alarms[0]->id, next_thread->display_alarms[0]->type, next_thread->display_alarms[0]->seconds, next_thread->display_alarms[0]->message);
                    a_or_b = 'b';
                    }

                    if (next_thread -> display_alarms[1] != NULL){
                    printf("%d%c. Alarm(%d): %d %ld %s\n", counter, a_or_b, next_thread->display_alarms[1]->id, next_thread->display_alarms[1]->type, next_thread->display_alarms[1]->seconds, next_thread->display_alarms[1]->message);
                    }

                    a_or_b = 'a';
                    counter= counter +1;

                    
                }

                last_thread = &next_thread->link;
                next_thread = next_thread->link;
              }
                counter = 1;
             }



            
            

            status = pthread_mutex_unlock (&new_alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
    }
}
}