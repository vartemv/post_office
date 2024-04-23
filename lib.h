#include <unistd.h>          //Provides API for POSIX(or UNIX) OS for system calls
#include <stdio.h>           //Standard I/O Routines
#include <stdlib.h>          //For exit() and rand()
#include <pthread.h>         //Threading APIs
#include <semaphore.h>       //Semaphore APIs
#include <stdbool.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <ctype.h>
 

#define CREATE_PROCESS(id, type) ((process){id, type})
#define is_num(n) (n >= '0' && n <= '9')

#define FILE_NAME "proj2.out"
#define FILE_MODE "a"

//Cases to write into the file
#define CUSTOMER_STARTED 20
#define CUSTOMER_GOING_HOME 21
#define POST_IS_CLOSING 22
#define WORKER_STARTED 23
#define GOING_INTO_POST 24
#define CALLED 25
#define WORKER_GOING_HOME 26
#define T_BREAK 27
#define F_BREAK 28
#define SERVING_A_SERVICE 29
#define SERVICE_FINISHED 30

typedef struct{

    sem_t *queue_1; //Semaphore for queue dopisy
    sem_t *queue_2; //Semaphore for queue baliky
    sem_t *queue_3; //Semaphore for queue penezni sluzby
    sem_t *urednik; //Semaphore for changing amount of customers in the queues counter
    sem_t *lock;    //Semaphore for synchronizing worker and customer
    sem_t *file;    //Semaphore for writing into the file
    sem_t *lock_2;  //Second semaphore for synchronizing worker and customer

} semaphores;

typedef struct{

    int  *overall_customer_id;
    int  *overall_worker_id;
    int  *output_line;
    bool *post_open;

} shared_memory;

typedef struct
{
    int id;
    char type;

} process;

typedef struct
{
    int c_id;
    int w_id;
    int service;

} human;

//Struct for future user arguments
typedef struct 
{
    int number_of_customers;
    int number_of_workers;
    int close_time;
    int prestavka_urednika;
    int customer_time;
    
} arguments;

void write_to_file(int action_id, human *person, int *output_line)
{
    FILE *output = fopen(FILE_NAME, FILE_MODE);

    switch (action_id)
    {
        case CUSTOMER_STARTED:
        fprintf(output, "%d: Z %d: started\n", *output_line, person->c_id);
        break;
        
        case CUSTOMER_GOING_HOME:
        fprintf(output, "%d: Z %d: going home\n", *output_line, person->c_id);
        break;

        case POST_IS_CLOSING:
        fprintf(output, "%d: closing\n", *output_line);
        break;

        case WORKER_STARTED:
        fprintf(output, "%d: U %d: started\n", *output_line, person->w_id);
        break;

        case GOING_INTO_POST:
        fprintf(output, "%d: Z %d: entering office for a service %d\n", *output_line, person->c_id, person->service + 1);
        break;

        case CALLED:
        fprintf(output, "%d: Z %d: called by office worker\n", *output_line, person->c_id);
        break;

        case WORKER_GOING_HOME:
        fprintf(output, "%d: U %d: going home\n", *output_line, person->w_id);
        break;

        case SERVING_A_SERVICE:
        fprintf(output, "%d: U %d: serving a service of type %d\n", *output_line, person->w_id, person->service + 1);
        break;

        case T_BREAK:
        fprintf(output, "%d: U %d: taking break\n", *output_line, person->w_id);
        break;

        case F_BREAK:
        fprintf(output, "%d: U %d: break finished\n", *output_line, person->w_id);
        break;

        case SERVICE_FINISHED:
        fprintf(output, "%d: U %d: service finished\n", *output_line, person->w_id);
        break;
    }
    *output_line+=1;
    
    fclose(output);
}

//Create new forks
process fork_many(unsigned int n, const char type)
{
    for (uint id = 1; id <= n; id++)
        if (fork() == 0)
            return CREATE_PROCESS(getpid(), type);

    return (process){0};
}


//Usual input processing
int processing_input(arguments *args, int argc, char **argv){
    
    if (argc != 6){
        fprintf(stderr, "You have invalid amount of args");
        return 0;
    }

    int arg_id = 0;

    for (int i = 1; i < 6; i++){
        if (argv[i][0] == '\0')
        {
            printf("end\n");
            return 0;
        }

        for (int j = 0; argv[i][j]; j++){
            if (!is_num(argv[i][j])){
                fprintf(stderr, "You have float or wrong value in your input \n");
                return 0; 
                }
        }

        int spec_arg = atoi(argv[i]);

        switch(arg_id){

            case 0:
                if(spec_arg < 0){
                    fprintf(stderr, "You have invalid amount of customers");
                    return 0;
                }
                args->number_of_customers = spec_arg;
                break;
            case 1:
                if(spec_arg <= 0){
                    fprintf(stderr, "You have invalid amount of workers");
                    return 0;
                }
                args->number_of_workers = spec_arg;
                break;
            case 2:
                if(spec_arg >= 0 && spec_arg <= 10000){
                    args->customer_time = spec_arg;
                    break;
                }
                else{
                    fprintf(stderr, "Your TZ is out of bonds");
                    return 0;
                }
            case 3:
                if(spec_arg >= 0 && spec_arg <= 100){
                    args->prestavka_urednika = spec_arg;
                    break;
                }
                else{
                    fprintf(stderr, "Your TU is out of bonds");
                    return 0;
                }
            case 4:
                if(spec_arg >= 0 && spec_arg <= 10000){
                    args->close_time = spec_arg; 
                    break;
                }
                else{
                    fprintf(stderr, "Your F is out of bonds");
                    return 0;
                }
        }
        arg_id += 1;
    }
    return 1;
}

//Closing all used semaphores and shared memory variables
void complete_cleanup(semaphores *sem, shared_memory *vars, int *queues[3]){
    sem_close(sem->file);

    sem_close(sem->queue_1);

    sem_close(sem->queue_2);

    sem_close(sem->queue_3);

    sem_close(sem->lock);

    sem_close(sem->urednik);

    sem_close(sem->lock_2);

    if((munmap((vars->output_line), sizeof(vars->output_line))) == -1 ||
    (munmap((vars->overall_customer_id), sizeof(vars->overall_customer_id))) == -1 ||
    (munmap((vars->overall_worker_id), sizeof(vars->overall_worker_id))) == -1 ||
    (munmap((vars->post_open), sizeof(vars->post_open))) == -1 ||
    (munmap((queues[0]), sizeof(queues[1]))) == -1 ||
    (munmap((queues[1]), sizeof(queues[2]))) == -1 ||
    (munmap((queues[2]), sizeof(queues[3]))) == -1 )
    {
        fprintf(stderr, "Error: shared memory deinitialization failed.\n");
        exit(EXIT_FAILURE);
    }
}

