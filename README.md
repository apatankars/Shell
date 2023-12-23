```
 ____  _          _ _   ____
/ ___|| |__   ___| | | |___ \
\___ \| '_ \ / _ \ | |   __) |
 ___) | | | |  __/ | |  / __/
|____/|_| |_|\___|_|_| |_____|
```
In Shell 2, a lot of functionality is added to our shell. We first began with by ignoring some signals when in the terminal, however in the child process, we want these to have an impact. So we reinstate the signal handlers to their default settings there. We also have to give terminal control if it is a foreground process which we do.

We determine if something should be in the foreground or background initally if the '&' is inputted as the last symbol. If so, we change the flag to indicate this behavior which then handles terminal control. If the job is in the background, we add it to the job list. If it is a foreground process, we wait for the process to finish. We then print out the appropriate message for the process depending on how it exited or suspended update the job list if it is suspended..

For background processes, we have a reaper function which handles checking if the background processes are terminated or suspended. We then print out the approriate messages and make the updates to the job list. 

We then implement the functionality for fg and bg which resume the processses. If a process is resumed in the foreground, we have to give it terminal control and then wait for the function and reap it appropiately. We then print the appropriate message and update the job list. 

We have extensive error checking througout to ensure that user inputs or failing function will not cause the shell to crash. Rather it prints out an informative message and then moves on to the next prompt.
