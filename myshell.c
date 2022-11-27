#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int check_if_need_pipe(int count, char **arglist);
int exec_command_in_background(char **arglist);
int exec_command(char **arglist);
int redirect_output(int count, char **arglist);
int piping_processes(char **arglist1, char **arglist2);
void check_fork(int pid, int fd[]);
void failed_exec(void);


int prepare(void){
    /* ERAN'S TRICK */
    signal(SIGCHLD, SA_RESTART); /*check if should be this or IGN*/
    signal(SIGINT, SIG_IGN);

    return 0;
}

int process_arglist(int count, char **arglist){
    int i = 0;

    /* if there is | sign in the command we need to run both of the commands concurrently, piping the first process output to the output of the second */
    if ((i = check_if_need_pipe(count, arglist)) > 0){
        arglist[i] = NULL;
        piping_processes(arglist, &arglist[i+1]);
        return 1;
    }

    /* if there is > sign the output of the process should be output to the file */
    else if (count >= 3){
        if (strcmp(arglist[count-2], ">") == 0){
            redirect_output(count, arglist);
            return 1;
        }
    }

    /* if we need to run the process in the background the parent should not wait for an answer */
    if ((count >= 2) & (strcmp(arglist[count-1],"&") == 0)){
        arglist[count-1] = NULL;
        exec_command_in_background(arglist);
        return 1;
    }

    /* exec the command according to flag */
    exec_command(arglist);
    return 1;
}

int finalize(void){
    signal(SIGINT, SIG_IGN);

    return 0;
}


/*check if the sign | is in the arglist, if it is return its index else return -1*/
int check_if_need_pipe(int count, char **arglist){
    int i;
    for (i=0; i<count; i++){
        if (strcmp(arglist[i], "|") == 0){
            return i;
        }
    }
    return -1;
}

int exec_command(char **arglist){
    int pid = fork();
    check_fork(pid, NULL);

    /* if the process is the child, exec the command instead of him */
    if (!pid){
        signal(SIGINT, SIG_DFL);  /*foreground child process should terminate upon SIGINT*/
        execvp(arglist[0], arglist);
        failed_exec();
    }

    waitpid(pid, NULL, 0);
    return 0;
}

int exec_command_in_background(char **arglist){
    int pid = fork();
    check_fork(pid, NULL);

    /* if the process is the child, exec the command instead of him */
    if (!pid){
        execvp(arglist[0], arglist);
        failed_exec();
    }

    return 0;
}

/* with help from reference: https://stackoverflow.com/questions/11515399/implementing-shell-in-c-and-need-help-handling-input-output-redirection */
int redirect_output(int count, char **arglist){
    int fd, curr_out, pid;

    fd = open(arglist[count-1], O_WRONLY |O_CREAT, 0600); /*---- change flags!!!!*/
    if (fd < 0){
        fprintf(stderr, "ERROR: Can't open output file!\n");
        exit(1);
    }

    /* fix arglist before running the command*/
    arglist[count-2] = NULL;

    pid = fork();
    check_fork(pid, NULL);

    /* if the process is the child, exec the command instead of him */
    if (!pid){
        signal(SIGINT, SIG_DFL);
        curr_out = dup2(fd, STDOUT_FILENO);
        execvp(arglist[0], arglist);
        close(curr_out);
        failed_exec();
    }

    /* if the process is the parent and flag == 0, wait for child to finish */
    waitpid(pid, NULL, 0);
    close(fd);
    return 0;
}

/* with help from reference: https://www.youtube.com/watch?v=pO1wuN3hJZ4 */
int piping_processes(char **arglist1, char **arglist2){
    int pipefd[2], pid1, pid2;
    if (pipe(pipefd) < 0){
        fprintf(stderr, "ERROR: pipe creation failed\n");
        exit(1);
    }

    pid1 = fork();
    check_fork(pid1, pipefd);

    if (!pid1){
        signal(SIGINT, SIG_DFL);
        close(pipefd[0]); /*close read end*/
        dup2(pipefd[1], STDOUT_FILENO);
        execvp(arglist1[0], arglist1);
        failed_exec();
    } else{
        pid2 = fork();
        check_fork(pid2, pipefd);

        if (!pid2){
            signal(SIGINT, SIG_DFL);
            close(pipefd[1]); /*close write end*/
            dup2(pipefd[0], STDIN_FILENO);
            execvp(arglist2[0], arglist2);
            failed_exec();
        }
    }
    close(pipefd[0]);
    close(pipefd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
    return 0;
}

void check_fork(int pid, int fd[]){
    if (pid < 0){
        if (fd != NULL){
            close(fd[0]);
            close(fd[1]);
        }
        fprintf(stderr, "ERROR: fork Failed!\n");
        exit(1);
    }
}

void failed_exec(void){
    fprintf(stderr, "ERROR: Execution Failed\n");
    exit(1);
}



