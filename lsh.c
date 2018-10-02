/* 
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
n * If you want to add functions in a separate file 
 * you will need to modify Makefile to compile
 * your additional functions.
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Submit the entire lab1 folder as a tar archive (.tgz).
 * Command to create submission archive: 
      $> tar cvf lab1.tgz lab1/
 *
 * All the best 
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
#include <signal.h>
// for threading
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
// for separating command from arguments
#include <string.h>
// for debugging
#include <errno.h>


/*
 * Function declarations
 */
// RunCommand will return error if forking fails
// It could get the last commands exit-code, later
int RunCommand(Command *);
// Run Pgm will never return, since it transforms into the executed program
void RunPgm(Pgm *, int, int);
void intHandler(int);

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);


/* When non-zero, this global means the user is done using this program. */
int done = 0;
//PID of first child
int cpid = -1;

// Kills first foreground child, if alive, instead of self
void intHandler(int sig)
{
  if (cpid > 0)
    {
      kill(cpid, SIGKILL);
    }
}

/*
 * Name: main
 *
 * Description: Initialises
 *
 */
int main(void)
{
  Command cmd;
  int ret;
  char pwd[1024];

  // set signal handling to internal
  signal(SIGINT, intHandler);

  // handle children
  // initially we didn't handle sigchild and created zombies
  // we found this fix on stackoverflow after swift googling for handling
  /* https://stackoverflow.com/questions/7171722/how-can-i-handle-sigchld */
  signal(SIGCHLD, SIG_IGN);
  
  while (!done) {

    //Declare a char pointer and get the command from the user
    char *line;
    line = readline("# "); //Returns upon newline

    //If empty: go back and scan again.
    if (!line) {
      /* Encountered EOF at top level */
      done = 1;
    }
    //If not: clean the input and check that it isn't empty.
    else {
      //Remove leading and trailing whitespace from the line
      stripwhite(line);
      if(*line) {
        //Check for comments 
	if(line[0] == '#') {}
	//if normal, execute
	else {
	  add_history(line);
	  //Parse
	  ret = parse(line, &cmd);
	  if (ret != 1)
	    {
	      fprintf(stderr, "Parse error\n");
	      return 1;
	    }

	  if(strcmp(line,"exit") == 0) {
	    done=1;
	  }
	  else if(strcmp(cmd.pgm->pgmlist[0],"cd") == 0) {
	    if( cmd.pgm->pgmlist[1] )
	      {
		chdir(cmd.pgm->pgmlist[1]);	
	      }
	  }
	  else if(strcmp(line,"pwd") == 0) {
	    if (getcwd(pwd, sizeof(pwd)) != NULL)
	      {
		printf("%s\n", pwd);
	      }
	    else
	      {
		fprintf(stderr,"Pwd error. Is the path longer than 1024 characters?");
	      }
	  }
	  else
	    {
	  
	      //execute
	      ret = RunCommand(&cmd);
	      //negative numbers and 0 is error, since ret = nr of executed commands is 
	      if (ret < 0 )
		{
		  fprintf(stderr, "Error running command.");
		  return 1;
		}
	    }
      	  //Print debug information.
	  //	  PrintCommand(ret, &cmd);
	}
      }
    }
    //At each new loop/line, clean the input.
    if(line) {
      free(line);
    }
  }
  return 0;
}

/*
 * Name: RunCommand
 *
 * Description: Executes the parsed command structure
 * Returns error if any part of execution fails
 *
 */
int
RunCommand(Command *cmd)
{
  int cret;
  int in = -1;
  int out = -1;
  
  if( cmd->rstdin ){
    if ((in = open(cmd->rstdin, O_RDONLY)) == -1)
      {
	fprintf(stderr, "Cannot open input file\n");
	return -1;
      }
  }
  
  if( cmd->rstdout ){
    // This is where my append would go, if the parser read it!!!
    if ((out = open(cmd->rstdout, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1)
      {
	fprintf(stderr, "Cannot open output file\n");
	return -1;
      }
  }
  
  //Fork the program call
  cpid = fork();
  if (cpid == 0)
    {
      //is child, carry on
      //will never return...
      RunPgm(cmd->pgm, out, in);
    }
  else if(cpid > 0)
    {
      if( !cmd->bakground )
	{
	  //Wait for child, return child's return
	  //This yields the executed commands return code, due to exec
	  waitpid(cpid, &cret, 0);
	  return cret;
	}
      else
	{
	  //forget child and return 0 without waiting for return
	  cpid = -1;
	  return 0;
	}
    }
  else
    {
      //fork error
      fprintf(stderr, "Fork error in parent.");
      return -1;
    }
  return -1; // it should never get here
}


/*
 * Name: RunPgm
 *
 * Description: Runs the pgm list recursively
 * (I wish the parser had been readable and thus feasible to modify)
 *
 */
void
RunPgm (Pgm *p, int out, int in)
{
  int pid;
  int pipes[2];
  
  /* The list is in reversed order so print
   * it reversed to get right
   * (If it wasn't this could easily be done by looping...)
   */
  
  //if there is a next command, set the pipe correctly and run it
  if( p->next )
    {
      if(pipe(pipes))
	{
	  //pipe creation failed
	  fprintf(stderr, "pipe error in child\n");
	  exit(0);
	}
      //After pipe is prepared, fork
      pid = fork();
      if( pid == 0)
	{
	  //is child
	  close(pipes[0]);
	  //run the next command
	  RunPgm(p->next, pipes[1], in);
	  exit(0);
	}
      else if( pid <= 0)
	{
	  //is error, cleanup and fail silently
	  close(pipes[0]);
	  close(pipes[1]);
	  fprintf(stderr, "forking error in child.");  
	  exit(0);
	}
      else
	{
	  //is parent
	  //change stdin to pipe's output
	  close(pipes[1]);
	  dup2(pipes[0], STDIN_FILENO);
	  //run as usual
	}
    }
  else
    {
      //if this is the last program in the line the
      //in flag must be applied
      if (out >= 0)
	{
	  dup2(in, STDIN_FILENO);
	}
    }
  //if parent and pipe done or no threading
  //change output to out, if given
  if (out >= 0)
    {
      dup2(out, STDOUT_FILENO);
    }
  //run command
  execvp(*p->pgmlist, p->pgmlist);
  fprintf(stderr, "Command not found.");
  exit(0);
      
  return;
}



/*
 * Name: PrintCommand
 *
 * Description: Prints a Command structure as returned by parse on stdout.
 *
 */
void
PrintCommand (int n, Command *cmd)
{
  printf("Parse returned %d:\n", n);
  printf("   stdin : %s\n", cmd->rstdin  ? cmd->rstdin  : "<none>" );
  printf("   stdout: %s\n", cmd->rstdout ? cmd->rstdout : "<none>" );
  printf("   bg    : %s\n", cmd->bakground ? "yes" : "no");
  PrintPgm(cmd->pgm);
}

/*
 * Name: PrintPgm
 *
 * Description: Prints a list of Pgm:s
 *
 */
void
PrintPgm (Pgm *p)
{
  if (p == NULL) {
    return;
  }
  else {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    PrintPgm(p->next);
    printf("    [");
    while (*pl) {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/*
 * Name: stripwhite
 *
 * Description: Strip whitespace from the start and end of STRING.
 */
void
stripwhite (char *string)
{
  register int i = 0;

  while (isspace( string[i] )) {
    i++;
  }
  
  if (i) {
    strcpy (string, string + i);
  }

  i = strlen( string ) - 1;
  while (i> 0 && isspace (string[i])) {
    i--;
  }

  string [++i] = '\0';
}
