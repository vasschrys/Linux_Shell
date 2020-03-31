/* $begin shellmain */
#include <stdlib.h>
#include "csapp.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <errno.h>

#define MAXARGS 128
#define MAXJOBS 128
#define MAXJSUM 1000
#define RD 0
#define WR 1

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);
void c_signalCatcher(int sig);
void zombie_signalCatcher(int sig);
void z_signalCatcher(int sig);
void addJob(char *status, pid_t pid, char *command, int job);
void changeStatus(char *status_, int count);
void pipe0();
void pipe1();
void piping(char *arg, char* argv);
void removeIndex(char *str, int i);

/* Job struct */
typedef struct Job
{
    pid_t jpid;
    int jobNum;
    char *status;
    char *command;

} Job;

/* JSum struct */
typedef struct JSum
{
    pid_t jsumpid;
    double time;
    char *status;
    char *command;
    int minFault;
    int majFault;

} JSum;

/* Global Variables */
pid_t pidfg; //pid 
struct Job *jobList; //list of jobs 
struct JSum *jsumTable; //table of background jobs
int status_; //pid status
clock_t clk; //holds time for jsum
char *command; //hold command line value
int fd[2]; //file descriptor for pipes
struct timespec start, end;
int counter; //counter for Jobs
int jcounter = 0; //counter for Jsum
int flag; //decides which commands are jsum and which arent

#define BILLION 1000000000L


/* Main */
int main()
{
    jobList = malloc(MAXJOBS * sizeof(struct Job));
    jsumTable = malloc(MAXJSUM * sizeof(struct JSum));
    char cmdline[MAXLINE]; /* Command line */
    signal(SIGINT, c_signalCatcher);
    signal(SIGTSTP, z_signalCatcher);

    //doesnt work with ls
    signal(SIGCHLD, zombie_signalCatcher);
    while (1)
    {
        int status1;

        /* Read */
        printf("lsh> ");
        Fgets(cmdline, MAXLINE, stdin);
        if (feof(stdin))
            exit(0);

        /* Evaluate */
        u_int64_t timeDiff;

        clock_gettime(CLOCK_MONOTONIC, &start); //mark start time

        eval(cmdline);

        clock_gettime(CLOCK_MONOTONIC, &end);
        timeDiff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
        timeDiff = timeDiff / 1000000;

        if (flag == 0)
        {
            int finish = waitpid(jsumTable[jcounter].jsumpid, &status1, WNOHANG);
            if (!WIFCONTINUED(status1))
            {
                struct rusage _rusage;
                getrusage(RUSAGE_SELF, &_rusage);
                jsumTable[jcounter].minFault = _rusage.ru_minflt;
                jsumTable[jcounter].majFault = _rusage.ru_majflt;
                jsumTable[jcounter].time = timeDiff;
                jcounter++; 
            }
        }
    }
}
/* $end shellmain */


/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    flag = 0;            //set flag to 0
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */

    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return; /* Ignore empty lines */

    if (!builtin_command(argv))
    {

        command = (char *)malloc(strlen(cmdline) + 1);
        strcpy(command, cmdline);

        //if string containts the pipe char '|'
        char *strsearch = strchr(cmdline, '|'); //strseach holds index of |
        if (strsearch != NULL)
        {

            piping(argv[0], argv[2]);

            return;
        }

        else //if not piping
        {

            //FORK
            if ((pid = Fork()) == 0)
            { /* Child runs user job */

                // set PGID to parent PID
                setpgid(0, 0);

                //$ 
                char **ptr = argv;
                for (int i = 0; argv[i] != '\0'; i++)
                {
                    char *name = calloc(20, sizeof(char));

                    if (ptr[i][0] == '$')
                    {

                        for (int j = 1; ptr[i][j] != '\0'; j++)
                        {
                            char *cat[1] = {ptr[i][j]};
                            strcat(name, cat);
                        }

                        argv[i] = getenv(name);
                    }
                }

                //EXEC IS HERE
                if (execvp(argv[0], argv) < 0)
                {

                    printf("%s: Command not found.\n", argv[0]);
                    exit(0);
                }
            }
        }

        /* Parent waits for foreground job to terminate */
        if (!bg) //fg
        {
            flag = 0;
            pidfg = pid; //set fg to pid

            if (waitpid(pid, &status_, WUNTRACED) > 0)
            {

                //block signal handler sigchild from running
                if (WIFSTOPPED(status_))
                {
                    if (counter == 0)
                    {
                        counter++;
                        addJob("Stopped", pid, command, counter);
                        //change status for jsum to abort
                        jsumTable[jcounter].status = "abort";
                        jsumTable[jcounter].command = command;
                        jsumTable[jcounter].jsumpid = pid;
                    }
                    else
                    {
                        counter++;
                        addJob("Stopped", pid, command, counter);
                        //change status for jsum to abort
                        jsumTable[jcounter].status = "abort";
                        jsumTable[jcounter].command = command;
                        jsumTable[jcounter].jsumpid = pid;
                    }
                }
                //unblock signal handler sigchild
            }
            else
            { //signal not stopped
                jsumTable[jcounter].jsumpid = pid;
                jsumTable[jcounter].status = "ok";
                jsumTable[jcounter].command = command;
            }
        }

        else
        { //bg

            counter++;
            //get pid for jsum

            int temp = counter;

            for (int i = 1; i < temp + 1; i++)
            {
                if (pid == jobList[i].jpid)
                {
                    break;
                }
                if (pid != jobList[i].jpid && i == temp)
                {
                    addJob("Running", pid, command, counter);
                }
            }
        }
    }

    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    flag = 1; //process is a built_in command

    if (!strcmp(argv[0], "quit")) /* quit command */
        exit(0);

    if (!strcmp(argv[0], "&")) /* Ignore singleton & */
        return 1;

    if (!strcmp(argv[0], "bg"))
    { /* Send it to the bg */
        char *num = argv[0][4]; 
        char* newstr = calloc(20, sizeof(char)); 
        int strct = 0; 
        
        for(int i =0; i<strlen(argv[1]); i++){
            if(argv[1][i] == '%'){
            }
            else if(argv[1][i] == '\0'){
                break;
            }else{
                newstr[strct] = argv[1][i]; 
                strct++; 
            }
                
        }     
        int check = atoi(newstr);

        if (isdigit(num))
        { //if the next argument is PID
            for (int i = 1; i < counter + 1; i++)
            {
                if (check == jobList[i].jobNum)
                {
                    kill(jobList[i].jpid, SIGCONT);
                    changeStatus("Running", i); //change status
                    return 1;
                }
            }
        }
        
        else{
            printf("No such job\n");
        }
        return 1;
    }

    if (!strcmp(argv[0], "fg")){ /* Send it to the fg */
    
       //char num = argv[0][4]; 
        char* newstr = calloc(20, sizeof(char)); 
        int strct = 0; 
        
        for(int i =0; i<strlen(argv[1]); i++){
            if(argv[1][i] == '%'){
            }
            else if(argv[1][i] == '\0'){
                break;
            }else{
                newstr[strct] = argv[1][i]; 
                strct++; 
            }
                
        }     
        
        int check = atoi(newstr);

        int flag = 0; 

        //if the next argument is PID
        for (int i = 1; i < counter + 1; i++)
        {
            if (check == jobList[i].jobNum)
            {
                kill(-jobList[i].jpid, SIGCONT);
                waitpid(jobList[i].jpid, &status_, WUNTRACED);
                if (WIFEXITED(status_) || WIFSIGNALED(status_)){
                    changeStatus("Exited\t", i);
                    jsumTable[jcounter].status = "Ok";
                }
                else{
                    jsumTable[jcounter].status = "error";
                }
               
                break;
            } else{
                
             }
            flag = 1; 
        }
        
        if(flag == 1) {
            printf("No Such Job\n");
        } 
        
        
        return 1;
    }

    //jsum
    if (!strcmp(argv[0], "jsum")){ /* Send it to the fg */

        if (jsumTable[0].status == NULL)
        {
            printf("No jobs in JSUM\n");
        }
        else
        {

            printf("PID\t");
            printf("Status\t");
            printf("Elapsed Time\t");
            printf("MinFaults\t");
            printf("MajFaults\t");
            printf("Command\n");
            if(jcounter == 0){
                printf("%d\t", jsumTable[0].jsumpid);
                printf("%s\t", jsumTable[0].status);
                printf("%f\t", jsumTable[0].time);
                printf("%d\t", jsumTable[0].minFault);
                printf("\t%d\t", jsumTable[0].majFault);
                printf("\t%s\n", jsumTable[0].command);
            }else{

            
            for (int i = 0; i < jcounter; i++)
            {
                printf("%d\t", jsumTable[i].jsumpid);
                printf("%s\t", jsumTable[i].status);
                printf("%f\t", jsumTable[i].time);
                printf("%d\t", jsumTable[i].minFault);
                printf("\t%d\t", jsumTable[i].majFault);
                printf("\t%s\n", jsumTable[i].command);
            }
            }
        }

        return 1;
    }

    // %
    if (argv[0][0] == '%')
    {
        //if only % sign
        if (argv[0][1] == '\0' || argv[0][1] == NULL)
        {
            return 1;
        }

       
        //char num = argv[0][4]; 
        char* newstr = calloc(20, sizeof(char)); 
        int strct = 0; 
        
        for(int i =0; i<strlen(argv[1]); i++){
            if(argv[1][i] == '%'){
            }
            else if(argv[1][i] == '\0'){
                break;
            }else{
                newstr[strct] = argv[1][i]; 
                strct++; 
            }
                
        }     
        int check = atoi(newstr);

        //if job number not created yet reuturn
        if (check > counter)
        {
            printf("No Job with that JID\n");
            return 1;
        }

        //otherwise print the job at specified index
        printf("[%d]  ", check);
        printf("  %d\t", jobList[check].jpid);
        printf("%s\t", jobList[check].status);
        printf("%s\n", jobList[check].command);

        return 1;
    }

    //jobs
    if (!strcmp(argv[0], "jobs"))
    {
        if (jobList[1].status == NULL)
        {
            printf("No jobs\n");
        }
        else
        {

            for (int i = 1; i < counter + 1; i++)
            {
                printf("[%d]  ", i);
                printf("  %d\t", jobList[i].jpid);
                printf(" %s\t", jobList[i].status);
                printf(" %s", jobList[i].command);
            }
        }

        return 1;
    }

    //for environmental variables
    if (strchr(argv[0], '='))
    {
        char *name;
        char *value;
        int len = strlen(argv[0]);
        if (argv[0][len - 1] == '=')
        {
            name = strtok(argv[0], "=");
            unsetenv(name);
            return 1;
        }

        else
        {
            name = strtok(argv[0], "=");
            value = strtok(NULL, "=");
            setenv(name, value, 1);
            return 1;
        }
    }
    return 0; /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim; /* Points to first space delimiter */
    int argc;    /* Number of args */
    int bg;      /* Background job? */

    buf[strlen(buf) - 1] = ' ';   /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' ')))
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0) /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */

/*Control C*/
void c_signalCatcher(int sig)
{
    kill(-pidfg, SIGINT); //negative sign  to kill all group
}

/*Control Z*/
void z_signalCatcher(int sig)
{
    kill(-pidfg, SIGTSTP);
    changeStatus("Stopped", counter);
    printf("\n");
}

/*Handles 'zombie' processes*/
void zombie_signalCatcher(int sig)
{
    pid_t p;

    int saved_errno = errno;

    while ((p = waitpid(-1, &status_, WNOHANG)) > 0)
    {
        if (WIFEXITED(status_))
        {
            for (int i = 0; i < counter + 1; i++)
            {
                if (jobList[i].jpid == p && jobList[i].status == "Running")
                { 
                    break;
                }
            }

            if (WIFSTOPPED(status_))
            {
                if (counter == 0)
                {
                    counter++;
                    addJob("Stopped", p, command, counter);
                    
                    //change status for jsum to abort
                    jsumTable[jcounter].status = "abort";
                    jsumTable[jcounter].command = command;
                    jsumTable[jcounter].jsumpid = p;
                }
                else
                {
                    counter++;
                    addJob("Stopped", p, command, counter);

                    //change status for jsum to abort
                    jsumTable[jcounter].status = "abort";
                    jsumTable[jcounter].command = command;
                    jsumTable[jcounter].jsumpid = p;
                }
            }
        }
    }
    errno = saved_errno;
    return;
}

/*Add the job to the jobList*/
void addJob(char *status, pid_t pid, char *command, int job)
{
    jobList[counter].status = status;
    jobList[counter].jpid = pid;
    jobList[counter].command = command;
    jobList[counter].jobNum = job;
}

/*Change the status of a job*/
void changeStatus(char *status_, int count)
{
    jobList[count].status = status_;
}

/*Pipe*/
void piping(char *arg, char *argv)
{
    int status2;
    pid_t pid1, pid2;

    //0 is Read 1 is write
    if (pipe(fd) < 0)
    {
        fprintf(stderr, "Pipe Failed");
        return;
    }

    //fork first child
    pid1 = Fork();
    

    if (pid1 < 0) ///child process terminate
    {
        fprintf(stderr, "Fork Failed");
        return;
    }

    if (pid1 == 0) //child one process
    {
        //only write
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);

        execvp(arg[0], arg);
    }

    else
    { //parent process

        pid2 = Fork();
        

        if (pid2 < 0) //child process terminate
        {
            fprintf(stderr, "Pipe Failed");
            return;
        }

        //child 2
        if (pid2 == 0) //child 2 process run
        {
            //only read
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
            execvp(argv[1], argv);
        }

        else //wait for child 2 to finsih
        {

            wait(&status2); 
            return; 
        }
    }

    return;
}