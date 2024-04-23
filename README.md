# Main process:
* The main process immediately creates NZ customer processes and NU officer processes upon launch.
* Waits for a random time in the interval <F/2,F> using usleep.
* Prints: A: closing
* Waits for the termination of all the processes it has created. Once all these processes have finished, the main process
also terminates with exit code 0.
# Customer Process:
* Each customer process is uniquely identified by the idZ number, where 0 < idZ <= NZ.
* Upon launch, it prints: A: Z idZ: started
* Then it waits a random time in the interval <0,TZ> using usleep.

If the post office is closed:
* Prints: A: Z idZ: going home
* The process ends.

If the post office is open, it does the following:

* Randomly selects an action X — a number from the interval <1,3>.
* Prints: A: Z idZ: entering office for a service X
* Joins queue X and waits to be called by an officer.
* Prints: Z idZ: called by office worker
* Waits a random time in the interval <0,10> using usleep.
* Prints: A: Z idZ: going home
* The process ends.
# Officer Process:
* Each officer is uniquely identified by the idU number, where 0 < idU <= NU
* Upon launch, it prints: A: U idU: started

[start of cycle]:

* Officer serves a customer from queue X (randomly selected if any non-empty).
* Prints: A: U idU: serving a service of type X
* Waits for a random time in the interval <0,10> by calling usleep.
* Prints: A: U idU: service finished
* Continues back to [start of cycle].
If no customer is waiting in any queue and the post office is open, it:

* Prints: A: U idU: taking break
* Waits for a random time in the interval <0,TU> by calling usleep.
* Prints: A: U idU: break finished
* Continues back to [start of cycle].
If no customer is waiting in any queue and the post office is closed, it:

* Prints: A: U idU: going home
* The process ends.