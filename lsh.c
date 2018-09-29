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
int RunCommand(Command *);
int RunPgm(Pgm *);

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
	  if (ret != 1){ return 1; }
	  //execute
	  ret = RunCommand(&cmd);
	  if (ret != 0){ return 1; }
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
  int cpid;
  int cret;
  printf("starting execution\n");
  if( cmd->rstdin ){
    printf("there is an rstdin set\n");
  }
  //figure out how to read from specified file
  if( cmd->rstdout ){
    printf("there is an rstdout set\n");
    //if file address starts with / it is global, else appended to $PWD
    puts("test");
  }
  //figure out how to write to specified file
  if( cmd->bakground ){
    printf("it should be run in background\n");
  }
  //write the branching
  cpid = fork();
  if (cpid == 0) {
    //this thread is child
    //run the program
    RunPgm(cmd->pgm);
    //Close the child process
    exit(0);
  }
  else if (cpid > 0) {
    //this thread is parent
    //wait for child if background isn't set
    waitpid(cpid, &cret, 0);
    //TODO: add meaningful error codes
    if(cret != 0){ return 1; }
  }
  else {
    //error has occured
    printf("errno is %d", errno);
  }
  return cret;
}


/*
 * Name: RunPgm
 *
 * Description: Runs the pgm list recursively
 * (I wish the parser had been readable and thus feasible to modify)
 *
 */
int
RunPgm (Pgm *p)
{
  int ret;
  if (p == NULL) {
    return 0;
  }
  else {
    char **pl = p->pgmlist;
    /* The list is in reversed order so print
     * it reversed to get right
     * (If it wasn't this could easily be done by looping...)
     */
    ret = RunPgm(p->next);
    //if previous command worked, run next
    if (ret == 0){
      execvp(*pl, pl);
    }
    return ret;
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
