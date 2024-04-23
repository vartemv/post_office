#define _GNU_SOURCE
#include "lib.h"

void open_post_logic(human *person, shared_memory *vars, semaphores *sems){
    
    //going into choosed queue
    switch (person->service)
    {
        case 0:
            sem_wait(sems->queue_1);
            break;
        case 1:
            sem_wait(sems->queue_2);
            break;
        case 2:
            sem_wait(sems->queue_3);
            break;
    }

    //Called by worker
    sem_wait(sems->file);
    write_to_file(CALLED, person, vars->output_line);
    sem_post(sems->file);

    //Lock for synchronizing with worker
    sem_post(sems->lock);
       
    //Waiting   
    usleep((rand()%(10))*1000);

    //Second lock for synchronizing with worker
    sem_wait(sems->lock_2);
    
    //Going home
    sem_wait(sems->file);
    write_to_file(CUSTOMER_GOING_HOME, person, vars->output_line);
    sem_post(sems->file);

    return;
}

void zakaznik_logic(int *queues[3], semaphores *sems, shared_memory *vars, arguments *args){

    srand(getpid());

    human person;

    //Getting id of specific customer and incrementing overall number
    person.c_id = *(vars->overall_customer_id);
    *(vars->overall_customer_id) += 1;

    sem_wait(sems->file);
    write_to_file(CUSTOMER_STARTED, &person, vars->output_line);
    sem_post(sems->file);

    if(args->customer_time != 0)
        usleep((rand() % args->customer_time)*1000);

    if(*(vars->post_open))
    {
        //Choosing random queue
        int service_id = rand()%3;
        person.service = service_id;

        //Going into post
        sem_wait(sems->file);
        write_to_file(GOING_INTO_POST, &person, vars->output_line);
        sem_post(sems->file);
        
        //Incrementing amount od customers in the choosed queue
        sem_wait(sems->urednik);
        *queues[service_id] += 1;
        sem_post(sems->urednik);
        
        open_post_logic(&person, vars, sems);

        exit(EXIT_SUCCESS);
    }
    else
    {
        //If post is closed
        sem_wait(sems->file);
        write_to_file(CUSTOMER_GOING_HOME, &person, vars->output_line);
        sem_post(sems->file);

        exit(EXIT_SUCCESS);
    }    
}

void worker_logic(semaphores *sems, shared_memory *vars, int *queues[3], arguments *args){

    srand(getpid());
    human person;

    //Getting id of specific worker and incrementing overall number
    person.w_id = *(vars->overall_worker_id);
    *(vars->overall_worker_id) += 1;

    sem_wait(sems->file);
    write_to_file(WORKER_STARTED, &person, vars->output_line);
    sem_post(sems->file);

//lable for goto 
check:
    
    //main cycle for serving services
    while(*queues[0] != 0 || *queues[1] != 0 || *queues[2] != 0){
        
        //Choosing non-empty queue
        sem_wait(sems->urednik);
        int choice = rand() % 3;
        while(*queues[choice] == 0){
            choice = rand() % 3;
        }
        person.service = choice;
        sem_post(sems->urednik);

        //Freeing customer from a queue
        switch(choice){
            case 0:
                sem_post(sems->queue_1);
                break;
            case 1:
                sem_post(sems->queue_2);
                break;
            case 2:
                sem_post(sems->queue_3);
                break;    
        }
        
        sem_wait(sems->urednik);
        *queues[choice] -= 1;
        sem_post(sems->urednik);
        
        //Lock for synchronizing with customer
        sem_wait(sems->lock);

        //Serving
        sem_wait(sems->file);
        write_to_file(SERVING_A_SERVICE, &person, vars->output_line);
        sem_post(sems->file);

        //Waiting
        usleep((rand()%(10))*1000);
        
        //Serving ends
        sem_wait(sems->file);
        write_to_file(SERVICE_FINISHED, &person, vars->output_line);
        sem_post(sems->file);

        //Second lock for synchronizing with customer
        sem_post(sems->lock_2);

    }
    
    //Checking if post is closed
    if (*(vars->post_open)){
        
        //Going to break
        sem_wait(sems->file);
        write_to_file(T_BREAK, &person, vars->output_line);
        sem_post(sems->file);

        //Waiting
        if (args->prestavka_urednika != 0)
            usleep((rand()%(args->prestavka_urednika))*1000);

        //Ending break
        sem_wait(sems->file);
        write_to_file(F_BREAK, &person, vars->output_line);
        sem_post(sems->file);
        
        //Going to the beginning of the cycle
        goto check;
    }

    //Going home
    sem_wait(sems->file);
    write_to_file(WORKER_GOING_HOME, &person, vars->output_line);
    sem_post(sems->file);

    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv){

    //Getting id of main process
    pid_t main_id = getpid();

    //Creating all necessary variables
    process proc;
    semaphores sems;
    shared_memory vars;
    arguments user_args;

    //Variable for counting customers in each queue
    int *queues[3];
    
    //Variable to check if input was processed in the right way
    int args_ok = processing_input(&user_args, argc, argv);
    if (!args_ok)
        return 1;

    //Initializing necessary semaphore and shared memory variables
    if (main_id == getpid()){
    fclose(fopen(FILE_NAME, "w"));
   
    if(((sems.file = sem_open("output", O_CREAT | O_WRONLY, 0666, 1)) == SEM_FAILED) ||
       ((sems.queue_1 = sem_open("dopisy", O_CREAT | O_WRONLY, 0666, 0)) == SEM_FAILED)||
       ((sems.queue_2 = sem_open("baliky", O_CREAT | O_WRONLY, 0666, 0)) == SEM_FAILED)||
       ((sems.queue_3 = sem_open("penezni_sluzby", O_CREAT | O_WRONLY, 0666, 0)) == SEM_FAILED)||
       ((sems.lock = sem_open("lock", O_CREAT | O_WRONLY, 0666, 0)) == SEM_FAILED)||
       ((sems.urednik = sem_open("urednik", O_CREAT | O_WRONLY, 0666, 1)) == SEM_FAILED)||
       ((sems.lock_2 = sem_open("lock_2", O_CREAT | O_WRONLY, 0666, 0)) == SEM_FAILED)){

        perror("sem_open");
        return 1;

    };

    sem_unlink("output");
    sem_unlink("dopisy");
    sem_unlink("baliky");
    sem_unlink("penezni_sluzby");
    sem_unlink("urednik");
    sem_unlink("lock");
    sem_unlink("lock_2");

    if(((vars.overall_customer_id = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    || ((vars.output_line = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) 
    || ((vars.overall_worker_id = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    || ((queues[0] = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    || ((queues[1]= mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    || ((queues[2] = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)
    || ((vars.post_open = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED)){

        fprintf(stderr, "Error: shared memory initialization failed.\n");
        return 1;
    }
    *(vars.overall_worker_id) = 1;
    *(vars.overall_customer_id) = 1;
    *(vars.output_line) = 1;

    for (int i = 0; i < 3; i++)
    {
        *queues[i] = 0;
    }
    
    *(vars.post_open)=true;
    }

    //Creating customer processes 
    if (main_id == getpid()){
        proc = fork_many(user_args.number_of_customers, 'c');
    }

    //Creating worker processes 
    if (main_id == getpid()){
        proc = fork_many(user_args.number_of_workers, 'w');
        
    }   

    //Entering main executing block for child processes
    if (main_id != getpid()){
        if(proc.type == 'c')
            zakaznik_logic(queues, &sems, &vars, &user_args);
        else if(proc.type == 'w')
            worker_logic(&sems, &vars, queues, &user_args);
        
    }
    
    //Post closing
    if (main_id == getpid()){
        srand(getpid());
        //Random time of waiting before closing
        usleep(((rand() % (user_args.close_time - (user_args.close_time)/2 + 1)) + user_args.close_time/2)*1000);

        //Post closing
        sem_wait(sems.file);
        *(vars.post_open) = false;
        write_to_file(POST_IS_CLOSING, NULL, vars.output_line);
        sem_post(sems.file);
        
    }
    
    //Waiting for child processes to end
    while(wait(NULL) > 0);

    complete_cleanup(&sems, &vars, queues);

    return 0;
}