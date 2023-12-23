#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "./jobs.h"
#ifndef WCONTINUED
#define WCONTINUED 8
#endif

/* My parse function reads in the input from the buffer and then parses through
 * it and populates the given arrays and files
 *
 * buffer - the buffer which is populated by the user input
 * *tokens - an array of tokens which are just all of the inputs separated by
 * their whitespace *argv - argv array populated by parse
 * **input_file - a string of the input file address (NULL if none)
 * **output_file - a string of the output file address (NULL if none)
 * *append_mode - an int which indicates whether to open the file in append mode
 * *prog_path - a string to the full file path to the executable
 * *argc - an int of the number of arguments in argv
 * *bg_flag - an int which represents if the process was passed with "&"" (0 for
 * foreground, 1 for background)
 *
 * returns 0 for a sucess and -1 for a failture
 */
int parse(char buffer[1024], char *tokens[512], char *argv[512],
          char **input_file, char **output_file, int *append_mode,
          char **prog_path, int *argc, int *bg_flag) {
    // Instantiate the variables
    int input_count = 0;
    int output_count = 0;
    argv[0] = NULL;

    // Check if the buffer is empty
    if (buffer[0] == '\0') {
        return -1;
    }

    char *token;
    char *buf = buffer;
    int tokenCount = 0;

    // This parses through buffer and populates the tokens array
    while ((token = strtok(buf, " \t\n")) != NULL) {
        tokens[tokenCount] = token;
        tokenCount++;
        buf = NULL;
    }

    // If the tokens array is empty, the buffer was just white-space
    if (tokens[0] == NULL) {
        return -1;
    }

    // Start populating the argv array
    *argc = 0;
    for (int i = 0; i < tokenCount; i++) {
        char *dest = NULL;  // Instantiate the variable to be null starting on
                            // each iteration
        if (strcmp(tokens[i], "&") == 0) {  // If it has the ampersand
            if (i == (tokenCount - 1)) {  // It has to be the last token or else
                                          // it just an argument in argv
                *bg_flag = 1;             // Set background flag
                continue;  // Move to next token (could also just exit)
            }
        }
        // Need to check if the command is "cd" in which case, it can be "cd
        // ../" where we do not want it to affected by strrchr, therefore we
        // check for it before that leg can happen

        // dest is now set to being the string up to the last instance of '/' in
        // the token. If there is no '/' in the token, it returns NULL
        dest = strrchr(tokens[i], '/');

        // Check if dest is not NULL meaning there is an executable. Also, it
        // makes sure that argv[0] is NULL therefore if we run into an instance
        // of a '/' any where else, it is considered to be an argument
        if (dest != NULL && argv[0] == NULL) {
            dest++;  // Increment the pointer to skip over '/'
            *prog_path =
                tokens[i];   // We want the program path to be the full path
            argv[0] = dest;  // argv[0] is meant to be the binary file path
            *argc =
                *argc + 1;  // Increment argc since we have populated the array

            // We now start checking for the redirection symbols and file paths
        } else if (strcmp(tokens[i], "<") == 0) {
            if (i + 1 < tokenCount && tokens[i + 1] != NULL) {
                if (input_count <
                    1) {  // This ensures that there is only one input
                    *input_file = tokens[i + 1];  // This is the file path
                    input_count++;
                    i++;  // Skip the next token since it is the file path
                } else {
                    fprintf(stderr, "Syntax Error: Multiple Input Redirects\n");
                    // If input direction >= 1, print out error and return -1
                    return -1;
                }
            } else {
                fprintf(stderr, "Syntax Error: No Input File.\n");
                // If the token after the redirection symbol is NULL, no file
                // was inputted
                return -1;
            }
        } else if (strcmp(tokens[i], ">") == 0) {
            if (i + 1 < tokenCount && tokens[i + 1] != NULL) {
                if (output_count <
                    1) {  // This ensures that there is only one output
                    *output_file = tokens[i + 1];  // This is the file path
                    output_count++;
                    i++;  // Skip the next token since it is the file path
                } else {
                    fprintf(stderr,
                            "Syntax Error: Multiple Output Redirects\n");
                    // If output direction >= 1, print out error and return -1
                    return -1;
                }
            } else {
                fprintf(stderr, "Syntax Error: No Output File.\n");
                // If the token after the redirection symbol is NULL, no file
                // was inputted
                return -1;
            }
        } else if (strcmp(tokens[i], ">>") == 0) {
            if (i + 1 < tokenCount && tokens[i + 1] != NULL) {
                if (output_count <
                    1) {  // This ensures that there is only one output
                    *append_mode = 1;              // Set the append flag
                    *output_file = tokens[i + 1];  // This is the file path
                    output_count++;
                    i++;  // Skip the next token since it is the file path
                } else {
                    fprintf(stderr,
                            "Syntax Error: Multiple Output Redirects\n");
                    // If output direction >= 1, print out error and return -1
                    return -1;
                }
            } else {
                fprintf(stderr, "Syntax Error: No Output File.\n");
                // If the token after the redirection symbol is NULL, no file
                // was inputted
                return -1;
            }
        } else {
            argv[*argc] = tokens[i];
            *argc = *argc + 1;
            // Put the token into argc and increment the counter
        }
    }

    argv[*argc] = NULL;  // NULL terminate the argv array

    if (argv[0] == NULL) {  // If by the end of population, the argv[0] is NULL,
                            // no command was inputted
        // fprintf(stderr, "Error: No Command.\n");
        return -1;
    }
    return 0;  // Parse was successful :)
}
/* My synerr function prints a syntax error for the given function
 *
 * *func - a string of the function name
 *
 * returns void
 */
void synerr(char *func) {
    fprintf(stderr, "%s: Syntax Error\n", func);  // Print error function
}
/* My syscalls function handles cd, ln, rm, exit, fg, bg, and any executables
 *
 * *argv - argv array populated by parse
 * **input_file - a string of the input file address (NULL if none)
 * **output_file - a string of the output file address (NULL if none)
 * *append_mode - an int which indicates whether to open the file in append mode
 * *prog_path - a string to the full file path to the executable
 * *argc - an int of the number of arguments in argv
 * *shell_pid - an int which represents the shell pgid
 * *bg_flag - an int which represents if the process was passed with "&"" (0 for
 * foreground, 1 for background)
 * **job_list - a pointer to the job_list
 * *job_num - an int which is the current job number
 *
 * returns 0 for a sucess and -1 for a failture
 */
int syscalls(char *argv[512], char **input_file, char **output_file,
             int *append_mode, char **prog_path, int *argc, pid_t *shell_pgid,
             int *bg_flag, job_list_t **job_list, int *job_num) {
    if (strcmp(argv[0], "cd") == 0) {  // If command is "cd"
        if (*argc == 2) {  // argc has to be of length two or else syntax error
            if (chdir(argv[1]) ==
                -1) {  // Do command, if fails, print error and return -1
                perror("cd");
                return -1;
            } else {
                return 0;  // Command was successful, return success
            }
        } else {
            synerr("cd");  // Print syntax error
            return -1;
        }
    } else if (strcmp(argv[0], "ln") == 0) {  // If command is "ln"
        char *src = argv[1];
        char *dest = argv[2];
        if (*argc == 3) {  // argc must be 3 or else syntax error
            if (link(src, dest) ==
                -1) {  // Do command, if fails, print error and return -1
                perror("ln");
                return -1;
            } else {
                return 0;  // Command was successful, return success
            }
        } else {
            synerr("ln");  // Print syntax error
            return -1;
        }
    } else if (strcmp(argv[0], "rm") == 0 &&
               *prog_path == NULL) {  // If command is rm AND prog_path must be
                                      // NULL or else it can be '/bin/rm' which
                                      // should be handled by the fork
        if (*argc == 2) {             // argc must 2 or else syntax error
            if (unlink(argv[1]) ==
                -1) {  // Do command, if fails, print error and return -1
                perror("rm");
                return -1;
            } else {
                return 0;  // Command was successful, return success
            }
        } else {
            synerr("rm");  // Print syntax error
            return -1;
        }
    } else if (strcmp(argv[0], "exit") == 0) {  // If command is "exit"
        if (*argc == 1) {  // Must just be "exit" or else syntax error
            cleanup_job_list(*job_list);
            exit(0);  // Exit successfully
        } else {
            synerr("exit");  // Print syntax error
            return -1;
        }
    } else if (strcmp(argv[0], "jobs") == 0) {
        if (*argc == 1) {     // Must just be jobs
            jobs(*job_list);  // Print jobs
        } else {
            synerr("jobs");  // Print syntax error
            return -1;
        }
    } else if (strcmp(argv[0], "bg") == 0) {
        if (*argc == 2) {
            // RESUME JOB IN BACKGROUND
            char *jid_string = argv[1];
            if (jid_string[0] != '%' || strrchr(jid_string, '%') == NULL) {
                fprintf(stderr, "Usage: bg %%<jid>\n");  // Print correct usage
                return -1;
            }

            jid_string++;  // Move past the %
            int jid = atoi(jid_string);

            if (update_job_jid(*job_list, jid, RUNNING) ==
                -1) {  // Update the job status to running
                fprintf(stderr, "Error: Job not found\n");
                return -1;
            }

            pid_t job_pid = get_job_pid(*job_list, jid);

            if (kill(-job_pid, SIGCONT) == -1) {  // Singal to job to continue
                perror("kill");
                return -1;
            }

        } else {
            synerr("bg");  // Print a syntax error
            return -1;
        }
    } else if (strcmp(argv[0], "fg") == 0) {
        if (*argc == 2) {
            char *jid_string = argv[1];

            if (jid_string[0] != '%' || strrchr(jid_string, '%') == NULL) {
                fprintf(stderr, "Usage: fg %%<jid>\n");  // Correct usage
                return -1;
            }

            jid_string++;  // Move past the %

            int jid = atoi(jid_string);

            if (update_job_jid(*job_list, jid, RUNNING) ==
                -1) {  // Update the job to be running
                fprintf(stderr, "Error: Job not found\n");
                return -1;
            }

            pid_t job_pid = get_job_pid(*job_list, jid);

            if (kill(-job_pid, SIGCONT) == -1) {  // Signal job to continue
                perror("kill");
                return -1;
            }

            if (tcsetpgrp(STDIN_FILENO, job_pid) ==
                -1) {  // Terminal control to the job
                perror("tcsetpgrp");
                return -1;
            }

            // Use while loop to ensure it waits for the process to finish
            int wstatus;
            pid_t wret;
            while ((wret = waitpid(-job_pid, &wstatus,
                                   WUNTRACED | WCONTINUED)) > 0) {
                if (wret == -1) {
                    // Handle error
                    printf("error");
                    return -1;
                } else if (WIFEXITED(
                               wstatus)) {  // If the process exits normally
                    if (remove_job_pid(*job_list, job_pid) ==
                        -1) {  // Remove the job from the list
                        fprintf(stderr, "Error: Could Not Remove Job\n");
                        return -1;
                    }
                    if (tcsetpgrp(STDIN_FILENO, *shell_pgid) ==
                        -1) {  // Restore control back to shell
                        perror("tcsetpgrp");
                        return -1;
                    }
                    return 0;
                } else if (WIFSIGNALED(wstatus)) {
                    // Process was terminated by a signal
                    int signal = WTERMSIG(wstatus);
                    printf("(%d) terminated by signal %d\n", job_pid, signal);
                    if (remove_job_pid(*job_list, job_pid) ==
                        -1) {  // Remove job from list
                        fprintf(stderr, "Error: Could Not Remove Job\n");
                        return -1;
                    }
                    if (tcsetpgrp(STDIN_FILENO, *shell_pgid) ==
                        -1) {  // Restore control back to shell
                        perror("tcsetpgrp");
                        return -1;
                    }
                    return 0;
                } else if (WIFSTOPPED(wstatus)) {
                    // Process was stopped by a signal
                    int signal = WSTOPSIG(wstatus);  // Update job status again
                    if (update_job_pid(*job_list, job_pid, STOPPED) == -1) {
                        fprintf(stderr, "Error: Could not update job status\n");
                        return -1;
                    }  // Print message
                    printf("[%d] (%d) suspended by signal %d\n",
                           get_job_jid(*job_list, job_pid), job_pid, signal);
                    if (tcsetpgrp(STDIN_FILENO, *shell_pgid) ==
                        -1) {  // Restore control to the shell
                        perror("tcsetpgrp");
                        return -1;
                    }
                    return 0;
                }
            }
        } else {
            synerr("fg");  // Print an error if the arg count is greater than 2
            return -1;
        }
    } else {
        pid_t child_pid;
        if ((child_pid = fork()) == 0) {  // Start child process
            // If error, print a message and exit
            if (setpgid(getpid(), 0) == -1) {
                perror("setpgid");
                exit(1);
            }

            pid_t child_pgid = getpgrp();
            // If error, print a message and exit
            if (child_pgid == -1) {
                perror("getpgrp");
                exit(1);
            }
            // If the process if foreground, switch the terminal control
            if (*bg_flag != 1) {
                if (tcsetpgrp(STDIN_FILENO, child_pgid) == -1) {
                    perror("tcsetpgrp");  // If fail, exit and print error
                    exit(1);
                }
            }
            // Restore signal handlers
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGTTOU, SIG_DFL);

            if (*input_file != NULL) {            // If an input_file exists
                if (close(STDIN_FILENO) == -1) {  // Close the standard input
                    perror("close");              // If close fails, print error
                    exit(1);                      // Exit process unsuccessfully
                }
                if (open(*input_file, O_RDONLY, 0666) ==
                    -1) {            // Open input file READ ONLY
                    perror("open");  // If open fails, print error
                    exit(1);         // Exit process unsuccessfully
                }
            }
            if (*output_file != NULL) {            // If an output_file exists
                if (close(STDOUT_FILENO) == -1) {  // Close the standard input
                    perror("close");  // If close fails, print error
                    exit(1);          // Exit process unsuccessfully
                }
                if (*append_mode == 1) {
                    if (open(*output_file, O_WRONLY | O_CREAT | O_APPEND,
                             0666) ==
                        -1) {  // Open file WRITE ONLY, create if it hasn't, and
                               // adds to the end of it
                        perror("open");  // If open fails, print error
                        exit(1);         // Exit process unsuccessfully
                    }
                } else {
                    if (open(*output_file, O_WRONLY | O_CREAT | O_TRUNC,
                             0666) ==
                        -1) {  // Open file WRITE ONLY, create if it hasn't, and
                               // clears it and write at the top
                        perror("open");  // If open fails, print error
                        exit(1);         // Exit process unsuccessfully
                    }
                }
            }
            execv(
                *prog_path,
                argv);  // Execute the given program with the arguments in argv

            perror("execv");  // If execv fails, print error
            exit(1);          // Exit process unsuccessfully
        }
        if (*bg_flag == 1) {
            *job_num = *job_num + 1;
            // Add the job to the job list; print error if error
            if (add_job(*job_list, *job_num, child_pid, RUNNING, *prog_path) ==
                -1) {
                printf("Error: Could not add job\n");
                return -1;
            }

            printf("[%d] (%d)\n", *job_num, child_pid);
        } else {  // If parent process, wait for child process to
                  // finish
            int wstatus;
            waitpid(child_pid, &wstatus, WUNTRACED | WCONTINUED);
            if (WIFSIGNALED(
                    wstatus)) {  // If the job is terminated by a signal, print
                                 // a message; we do not need to update the job
                                 // list because it is a foreground job
                int signal = WTERMSIG(wstatus);
                printf("(%d) terminated by signal %d\n", child_pid, signal);
            } else if (WIFSTOPPED(
                           wstatus)) {  // If job is stopped, add the job to the
                                        // job list and print a message
                int signal = WSTOPSIG(wstatus);
                *job_num = *job_num + 1;
                if (add_job(*job_list, *job_num, child_pid, STOPPED,
                            *prog_path) == -1) {
                    fprintf(stderr, "Error: Could not add job\n");
                    return -1;
                }
                printf("[%d] (%d) suspended by signal %d\n",
                       get_job_jid(*job_list, child_pid), child_pid, signal);
            }
            if (tcsetpgrp(STDIN_FILENO, *shell_pgid) ==
                -1) {  // Return terminal control; if error print and return
                perror("tcsetpgrp");
                return -1;
            }
        }
        if (child_pid < 0) {
            perror("fork");  // Else fork has failed and print error
            return -1;
        }
    }
    return 0;  // Successful syscall
}
/* My reaper function check for the background job exiting (stopping,
 * termininating, or continuing)
 *
 * **job_list - a pointer to the global job_list
 *
 * returns 0 for a sucess and -1 for a failture
 */
int reaper(job_list_t **job_list) {
    int status;
    pid_t wret;
    while ((wret = waitpid(-1, &status, WUNTRACED | WCONTINUED | WNOHANG)) >
           0) {
        int t_jid = get_job_jid(*job_list, wret);
        if (WIFEXITED(status)) {  // If the BACKGROUND job exits normally, print
                                  // a message and remove the job
            printf("[%d] (%d) terminated with exit status %d\n", t_jid, wret,
                   WEXITSTATUS(status));
            if (remove_job_pid(*job_list, wret) == -1) {
                fprintf(stderr, "Error: Could not remove job\n");
                return -1;
            }
        } else if (WIFSIGNALED(
                       status)) {  // If a job is terminated by a signal, print
                                   // a message and remove the job
            printf("[%d] (%d) terminated by signal %d\n", t_jid, wret,
                   WTERMSIG(status));
            if (remove_job_pid(*job_list, wret) == -1) {
                fprintf(stderr, "Error: Could not remove job\n");
                return -1;
            }
        } else if (WIFSTOPPED(
                       status)) {  // If a job is stopped by a signal, print a
                                   // statement and update its status
            printf("[%d] (%d) suspended by signal %d\n", t_jid, wret,
                   WSTOPSIG(status));
            if (update_job_pid(*job_list, wret, STOPPED) == -1) {
                fprintf(stderr, "Error: Could not update job status\n");
                return -1;
            }
        } else if (WIFCONTINUED(status)) {  // If the job was continued, print a
                                            // statement and update its status
            printf("[%d] (%d) resumed\n", t_jid, wret);
            if (update_job_pid(*job_list, wret, RUNNING) == -1) {
                fprintf(stderr, "Error: Could not update job status\n");
                return -1;
            }
        }
    }
    return 0;
}
/* My main function has the REPL loop which calls each of the helper functions
 * and houses all of the main variables
 *
 * void
 * returns 0 for a sucess and -1 for a failture
 */
int main() {
    int BUFSIZE = 1048;
    job_list_t *job_list = init_job_list();
    int job_num = 0;
    while (1) {
        char buf[BUFSIZE];
        char *tokens[512];
        char *argv[512];
        int append_mode = 0;
        char *input_file = NULL;
        char *output_file = NULL;
        int argc = 0;
        int bg_flag = 0;  // 0 if not bg, 1 if bg
        char *prog_path = NULL;

        pid_t shell_pgid = getpgrp();
        if (shell_pgid ==
            -1) {  // If error, print error and move onto next iteration
            perror("getpgrp");
            continue;
        }

        if (reaper(&job_list) == -1) {  // Call the reaper function, if it
                                        // fails, go onto the next iteration
            continue;
        }

        if (printf("Shell> ") < 0) {
            fprintf(stderr,
                    "Error printing\n");  // Print error if printf failts
        }
        if (fflush(stdout) < 0) {
            perror("fflush");  // Print error since fflush sets errno
        }

        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);

        ssize_t bytes_read =
            read(STDIN_FILENO, buf, sizeof(buf));  // Read in user input

        /* This checks for EOF (ctrl + D)*/
        if (bytes_read == 0) {
            cleanup_job_list(job_list);  // Clean-up the job list when we exit
            exit(1);
        }

        buf[bytes_read] = '\0';  // NULL terminate buffer

        if (parse(buf, tokens, argv, &input_file, &output_file, &append_mode,
                  &prog_path, &argc, &bg_flag) == -1) {
            // If parse fails, skip to the next iteration of the loop (error
            // message printed from parse)
            continue;
        }

        if (syscalls(argv, &input_file, &output_file, &append_mode, &prog_path,
                     &argc, &shell_pgid, &bg_flag, &job_list, &job_num) == -1) {
            // If syscalls fails, skip to the next iteration of the loop (error
            // message printed from syscalls)
            continue;
        }
    }
    cleanup_job_list(
        job_list);  // Clean Up all of the jobs when the shell exits
    return 0;       // Function was successful!
}
