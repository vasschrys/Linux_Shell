**README**
**liajackShell**

Our program lsh.c, is a linux shell that reads commands that are typed into the console 
and executes them. Our shell includes a multitude of functions. You can find a list of 
them below. 

**COMPILING AND RUNNING**

To run our program extract the provided zip file and compile using:

	gcc -Wall -o shellex -pthread lsh.c csapp.c

After compiling, run using :

	./lsh

**USAGE**

Our shell behaves like a Linux terminal, performing many commands. You can run the following built in commands:
	
	bg - Run jobs in the background. 
		 If the user wants to run a stopped process in the background.
		 By typing in 'bg %<JID>' the shell runs the process with the user given job number. 
	
	fg - Run jobs in the foreground. If the user wants to run a stopped process in the 
	     foreground. By typing in 'fg %<JID>' the shell will bring the process with the 
	     specified job number to the forground of the terminal until it completes or the 
	     user enters ina  termination command. 
	
	%  - Reference and retrieve a job, (ex: %1) retrieves the first job. % indicates that 
		 the number following it respresents a Job Id number. 
	
	$  - Print the value of an environment variable, (ex: $PATH). 
	
	&  - When entered after a process in the command line, the process runs in the background, 
		 (ex: sleep 10 &).
	
	jobs - Lists all background and suspended jobs. Detailed with the PID number, 
		   status, and command of each job. 

	jsum - Lists all processes that are not built in commnands (ex. ls). Detailed in 
		   this list are the PID, status, command, major page falts, minor page faults, 
		   and time taken to complete the non-built in commands. 
	

