/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#include "execute.h"

#include <stdio.h>
#include <fcntl.h> // for open
#include <sys/wait.h>
#include "quash.h"

// Remove this and all expansion calls to it
/**
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME()                                                  \
  fprintf(stderr, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)
  
IMPLEMENT_DEQUE_STRUCT(PidDeque, pid_t);
IMPLEMENT_DEQUE(PidDeque, pid_t);

typedef struct Job {
    int job_id;
    char* cmd;
    PidDeque pidDeque;
} Job;

IMPLEMENT_DEQUE_STRUCT(JobDeque, Job);
IMPLEMENT_DEQUE(JobDeque, Job);

//Declare queue of jobs
static JobDeque jobs;
static bool init = 1;
static int pipes[2][2];
static int job_id = 1;
/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char* get_current_directory(bool* should_free) {
	int size = 256;
	*should_free = true;
	char* pwd = getcwd(NULL, size);
  return pwd;
}

// Returns the value of an environment variable env_var
const char* lookup_env(const char* env_var) {
  // TODO: Lookup environment variables. This is required for parser to be able
  // to interpret variables from the command line and display the prompt
  // correctly
  // HINT: This should be pretty simple

  return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status() {
  // TODO: Check on the statuses of all processes belonging to all background
  // jobs. This function should remove jobs from the jobs queue once all
  // processes belonging to a job have completed.
  int job_count = length_JobDeque(&jobs);
  for(int i = 0; i< job_count; i++)
  {
	  Job job = pop_front_JobDeque(&jobs);
	  pid_t process = peek_front_PidDeque(&job.pidDeque);
	  int pid_count = length_PidDeque(&job.pidDeque);
	  for(int j = 0; j < pid_count; j++)
	  {
		  pid_t pid = pop_front_PidDeque(&job.pidDeque);
		  int status;
		  if(waitpid(pid, &status, WNOHANG) == 0)
		  {
			  push_back_PidDeque(&job.pidDeque, pid);
		  }
	  }
	  
	  if(is_empty_PidDeque(&job.pidDeque))
	  {
		  print_job_bg_complete(job.job_id, process, job.cmd);
		  free(job.cmd);
		  destroy_PidDeque(&job.pidDeque);
	  }
	  else
	  {
		  push_back_JobDeque(&jobs, job);
	  }
  }
}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char* cmd) {
  printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
  fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char* cmd) {
  printf("Background job started: ");
  print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char* cmd) {
  printf("Completed: \t");
  print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd) {
  // Execute a program with a list of arguments. The `args` array is a NULL
  // terminated (last string is always NULL) list of strings. The first element
  // in the array is the executable
  char* exec = cmd.args[0];
  char** args = cmd.args;

  // TODO: Implement run generic
  execvp(exec, args);

  perror("ERROR: Failed to execute program");
}

// Print strings
void run_echo(EchoCommand cmd) {
  // Print an array of strings. The args array is a NULL terminated (last
  // string is always NULL) list of strings.
  char** str = cmd.args;

  // TODO: Implement echo
	for(int i = 0; true; i++)
	{
		if(str[i] == NULL)
			break;
		printf("%s ", str[i]);
	}
	printf("\n");
  // Flush the buffer before returning
  fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd) {
  // Write an environment variable
  const char* env_var = cmd.env_var;
  const char* val = cmd.val;

  setenv(env_var, val, 1);
}

// Changes the current working directory
void run_cd(CDCommand cmd) {
  // Get the directory name
  const char* dir = cmd.dir;

  // Check if the directory is valid
  if (dir == NULL) 
  {
    perror("ERROR: Failed to resolve path");
    return;
  }

  // TODO: Update the PWD environment variable to be the new current working
  // directory and optionally update OLD_PWD environment variable to be the old
  // working directory.
  setenv("OLD_PWD", lookup_env("PWD"), 1);
  	chdir(dir);
  setenv("PWD", dir, 1);
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd) {
  int signal = cmd.sig;
  int job_id = cmd.job;

 for(int i = 0; i < length_JobDeque(&jobs); i++)
 {
	 Job job = pop_front_JobDeque(&jobs);
	 if(job.job_id == job_id)
	 {
		 while(!is_empty_PidDeque(&job.pidDeque))
		 {
			 pid_t pid = pop_front_PidDeque(&job.pidDeque);
			 kill(pid, signal);
			 push_back_PidDeque(&job.pidDeque, pid);
		 }
	 }
	 push_back_JobDeque(&jobs, job);
 }
}


// Prints the current working directory to stdout
void run_pwd() {
  // TODO: Print the current working directory
	printf("%s\n", lookup_env("PWD"));
  // Flush the buffer before returning
	fflush(stdout);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs() {
  // TODO: Print background jobs
  for(int i = 0; i < length_JobDeque(&jobs); i++)
  {
	  Job job = pop_front_JobDeque(&jobs);
	  print_job(job.job_id, peek_front_PidDeque(&job.pidDeque), job.cmd);
	  push_back_JobDeque(&jobs, job);
  }

  // Flush the buffer before returning
  fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case GENERIC:
    run_generic(cmd.generic);
    break;

  case ECHO:
    run_echo(cmd.echo);
    break;

  case PWD:
    run_pwd();
    break;

  case JOBS:
    run_jobs();
    break;

  case EXPORT:
  case CD:
  case KILL:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd) {
  CommandType type = get_command_type(cmd);

  switch (type) {
  case EXPORT:
    run_export(cmd.export);
    break;

  case CD:
    run_cd(cmd.cd);
    break;

  case KILL:
    run_kill(cmd.kill);
    break;

  case GENERIC:
  case ECHO:
  case PWD:
  case JOBS:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */
void create_process(CommandHolder holder, int i, PidDeque* pidDeque) {
  // Read the flags field from the parser
  bool p_in  = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in  = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true

  // TODO: Setup pipes, redirects, and new process
  pid_t pid;
  int write = i % 2;
  int read = (i-1)%2;
  
  if(p_out)
	pipe(pipes[write]);

  pid = fork();
  if(pid == 0) //child
  {
	  if (r_in)
    {
        FILE* file = fopen(holder.redirect_in, "r");
        dup2(fileno(file), STDIN_FILENO);
    }

    if (r_out)
    {
		if(r_app)
		{
		FILE* file = fopen(holder.redirect_out , "a") ;
        dup2(fileno(file), STDOUT_FILENO);
		}
		else
		{
        FILE* file = fopen(holder.redirect_out , "w") ;
        dup2(fileno(file), STDOUT_FILENO);
		}
    }
	
	  if(p_in)
	  {
	 dup2(pipes[read][0], STDIN_FILENO);
	 close(pipes[read][0]);
	  }
	  if(p_out)
	  {
		dup2(pipes[write][1], STDOUT_FILENO);
		close(pipes[write][1]);
	  }
	child_run_command(holder.cmd); // This should be done in the child branch of a fork
  exit(0);
  }
  else                              // a fork
  {
	if(p_out)
	{
		close(pipes[write][1]);
	}
	push_back_PidDeque(pidDeque, pid);
	parent_run_command(holder.cmd); 
	}
}

// Run a list of commands
void run_script(CommandHolder* holders) {
  if (holders == NULL)
    return;

if(init){
	jobs = new_JobDeque(1);
	init=false;
}

  check_jobs_bg_status();

  if (get_command_holder_type(holders[0]) == EXIT &&
      get_command_holder_type(holders[1]) == EOC) {
    end_main_loop();
    return;
  }

  	Job new_job;
	new_job.job_id =job_id;
	new_job.pidDeque = new_PidDeque(1);
	new_job.cmd = get_command_string();
  
  CommandType type;

  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
    create_process(holders[i], i, &new_job.pidDeque);

  if (!(holders[0].flags & BACKGROUND)) {
    // Not a background Job
    // TODO: Wait for all processes under the job to complete
	while(!is_empty_PidDeque(&new_job.pidDeque))
	{
		pid_t pid = pop_front_PidDeque(&new_job.pidDeque);
		int status;
		waitpid(pid, &status, 0);
	}
	  destroy_PidDeque(&new_job.pidDeque);
	  free(new_job.cmd);
  }
  else {
	  job_id++;
    // A background job.
    // TODO: Push the new job to the job queue
	push_back_JobDeque(&jobs, new_job);
    // TODO: Once jobs are implemented, uncomment and fill the following line
    print_job_bg_start(new_job.job_id, peek_back_PidDeque(&new_job.pidDeque), new_job.cmd);
	
  }
}
