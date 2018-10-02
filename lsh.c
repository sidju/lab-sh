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
//#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"
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
void RunPgm(Pgm *, int);

void PrintCommand(int, Command *);
void PrintPgm(Pgm *);
void stripwhite(char *);


/* When non-zero, this global means the user is done using this program. */
int done = 0;

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
        //Check for special commands 
	if(line[0] == '#') {
	  printf("commented\n");
	}
	else if(strcmp(line,"exit") == 0) {
	  done=1;
	}
	else if(strcmp(line,"cd") == 0) {
	  printf("cd\n");
	}
	else if(strcmp(line,"pwd") == 0) {
	  printf("%s\n", getenv("PWD"));
	}
	//if normal, execute
	else {

	  //TODO: add meaningful error codes
	  add_history(line);
	  //Parse
	  ret = parse(line, &cmd);
	  if (ret != 1)
	    {
	      puts("parse error\n");
	      return 1;
	    }
	  //execute
	  ret = RunCommand(&cmd);
	  //negative numbers and 0 is error, since ret = nr of executed commands is 
	  if (ret < 0 )
	    {
	      puts("run error\n");
	      return 1;
	    }
      	  //Print debug information.
	  PrintCommand(ret, &cmd);
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
  int cpid;
  printf("starting execution\n");
  if( cmd->rstdin ){
    printf("there is an rstdin set\n");
  }
  //figure out how to read from specified file
  if( cmd->rstdout ){
    printf("there is an rstdout set\n");
    //if file address starts with / it is global, else appended to $PWD
  }
  //Fork the program call
  cpid = fork();
  if (cpid == 0)
    {
      //is child, carry on
      RunPgm(cmd->pgm, -1);
      exit(0);
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
	  //Return 0 without waiting for return
	  return 0;
	}
    }
  else
    {
      //fork error
      return -1;
    }
}


/*
 * Name: RunPgm
 *
 * Description: Runs the pgm list recursively
 * (I wish the parser had been readable and thus feasible to modify)
 *
 */
void
RunPgm (Pgm *p, int out)
{
  int cpid;
  int pipes[2];
  
  if (p == NULL)
    {
      // should be redundant now
      // I'll delete it later, when sure
      //     return;
    }
  else
    {
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
	      puts("pipe error");
	      return;
	    }
	  //After pipe is prepared, fork
	  cpid = fork();
	  if( cpid == 0)
	    {
	      //is child
	      close(pipes[0]);
	      //run the next command
	      RunPgm(p->next, pipes[1]);
	      exit(0);
	    }
	  else if( cpid <= 0)
	    {
	      //is error, cleanup and fail silently
	      close(pipes[0]);
	      close(pipes[1]);
	      return;
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
      //if parent and pipe done or no threading
      //change output to out, if given
      if (out >= 0)
	{
	  dup2(out, STDOUT_FILENO);
	}
      //run command
      execvp(*p->pgmlist, p->pgmlist);
      puts("Command not found.");
      exit(0);
      }
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
