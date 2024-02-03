#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

pid_t pgid;

struct sigaction sigint_ign_action, sigint_default_action;
struct sigaction sigtstp_ign_action, sigtstp_default_action;



int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_pwd(struct tokens* tokens);
int cmd_cd(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
  cmd_fun_t* fun;
  char* cmd;
  char* doc;
} fun_desc_t;

/* Override the handler of SIGINT. 
 * Kill the foreground process, but not the shell. */
void sigint_handler(int sig) {
  if (shell_is_interactive) {
    printf("\n");
    fflush(stdout);
  }
}

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_pwd, "pwd", "print the current working directory to standard output"},
    {cmd_cd, "cd", "change the current working directory to the specified directory"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
  return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Prints the current working directory to standard output */
int cmd_pwd(unused struct tokens* tokens) {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    fprintf(stdout, "%s\n", cwd);
  } else {
    perror("getcwd() error");
  }
  return 1;
}

/* Changes the current working directory to the specified directory */
int cmd_cd(struct tokens* tokens) {
  if (tokens_get_length(tokens) != 2) {
    fprintf(stderr, "cd: missing argument\n");
    return 0;
  }
  if (chdir(tokens_get_token(tokens, 1)) != 0) {
    perror("chdir() error");
  }
  return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
  for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
    if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
      return i;
  return -1;
}

/* Intialization procedures for this shell */
void init_shell() {
  /* Our shell is connected to standard input. */
  shell_terminal = STDIN_FILENO;

  signal(SIGINT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  // signal(SIGTTIN, SIG_IGN);
  // sigint_ign_action.sa_handler = SIG_IGN;
  // sigemptyset(&sigint_ign_action.sa_mask);
  // sigint_ign_action.sa_flags = 0;
  // sigaction(SIGINT, &sigint_ign_action, &sigint_default_action);

  // sigtstp_ign_action.sa_handler = SIG_IGN;
  // sigemptyset(&sigtstp_ign_action.sa_mask);
  // sigtstp_ign_action.sa_flags = 0;
  // sigaction(SIGTSTP, &sigtstp_ign_action, &sigtstp_default_action);

  /* Check if we are running interactively */
  shell_is_interactive = isatty(shell_terminal);

  if (shell_is_interactive) {
    /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
     * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
     * foreground, we'll receive a SIGCONT. */
    while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
      kill(-shell_pgid, SIGTTIN);

    /* Saves the shell's process id */
    shell_pgid = getpid();

    /* Take control of the terminal */
    tcsetpgrp(shell_terminal, shell_pgid);

    /* Save the current termios to a variable, so it can be restored later. */
    tcgetattr(shell_terminal, &shell_tmodes);
  }
}

int file_exists(char *filename) {
  return access(filename, F_OK) != -1;
}

void execute_command(char *cmd, char *args[]) {
  char *path = getenv("PATH");

  // set up redirection
  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], ">") == 0) {
      int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd == -1) {
        perror("open() error");
        exit(1);
      }
      dup2(fd, STDOUT_FILENO);
      close(fd);
      args[i] = NULL;
    } else if (strcmp(args[i], "<") == 0) {
      int fd = open(args[i + 1], O_RDONLY);
      if (fd == -1) {
        perror("open() error");
        exit(1);
      }
      dup2(fd, STDIN_FILENO);
      close(fd);
      args[i] = NULL;
    }
  }
  
  if (cmd[0] == '/') {
    execv(cmd, args);
    perror("execv() error");
    exit(1);
  } else {
    char full_path[1024];
    char *token, *saveptr;
    for (token = strtok_r(path, ":", &saveptr); token != NULL; token = strtok_r(NULL, ":", &saveptr)) {
      snprintf(full_path, sizeof(full_path), "%s/%s", token, cmd);
      if (file_exists(full_path)) {
        execv(full_path, args);
        perror("execv() error");
        exit(1);
      }
    }
  }

  fprintf(stderr, "Command not found: %s\n", cmd);

}

int main(unused int argc, unused char* argv[]) {
  init_shell();

  static char line[4096];
  int line_num = 0;

  /* Please only print shell prompts when standard input is not a tty */
  if (shell_is_interactive)
    fprintf(stdout, "%d: ", line_num);

  while (fgets(line, 4096, stdin)) {
    /* Split our line into words. */
    struct tokens* tokens = tokenize(line);

    /* Find which built-in function to run. */
    int fundex = lookup(tokens_get_token(tokens, 0));

    if (fundex >= 0) {
      cmd_table[fundex].fun(tokens);
    } else {
      // support pipes
      int pipe_num = 0;
      for (size_t i = 0; i < tokens_get_length(tokens); i++) {
        if (strcmp(tokens_get_token(tokens, i), "|") == 0) {
          pipe_num++;
        }
      }

      int pipes[pipe_num + 1][2];
      for (int i = 0; i < pipe_num; i++) {
        if (pipe(pipes[i]) == -1) {
          perror("pipe() error");
          exit(1);
        }
      }

      char *token, *saveptr;
      int i = 0;

      for (token = strtok_r(line, "|", &saveptr); token != NULL; token = strtok_r(NULL, "|", &saveptr)) {
        pid_t cpid;
        struct tokens* sub_tokens = tokenize(token);
        char *cmd = tokens_get_token(sub_tokens, 0);
        char *sub_args[tokens_get_length(sub_tokens) + 1];
        for (size_t j = 0; j < tokens_get_length(sub_tokens); j++) {
          sub_args[j] = tokens_get_token(sub_tokens, j);
        }
        sub_args[tokens_get_length(sub_tokens)] = NULL;

        cpid = fork();

        if (cpid == 0) {
          signal(SIGINT, SIG_DFL);
          signal(SIGTSTP, SIG_DFL);
          signal(SIGTTOU, SIG_DFL);
          // signal(SIGTTIN, SIG_DFL);
          // sigaction(SIGINT, &sigint_default_action, NULL);
          // sigaction(SIGTSTP, &sigtstp_default_action, NULL);

          if (i == 0) {
            pgid = getpid();
            setpgid(0, 0);
          } else {
            setpgid(0, pgid);
          }

          if (i > 0) {
            dup2(pipes[i - 1][0], STDIN_FILENO);
          }

          if (i < pipe_num) {
            dup2(pipes[i][1], STDOUT_FILENO);
          }          
          
          for (int j = 0; j < pipe_num; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
          }

          // tokens_destroy(sub_tokens);
          
          execute_command(cmd, sub_args);
          exit(1);
        }
        
        if (i == 0) {
          setpgid(cpid, cpid);
          pgid = cpid;
        } else {
          setpgid(cpid, pgid);
        }

        i++;
      }

      for (int i = 0; i < pipe_num; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
      }

      tcsetpgrp(STDIN_FILENO, pgid);

      // for (int i = 0; i < pipe_num + 1; i++) {
      //   waitpid();
      // }
      waitpid(-pgid, NULL, WUNTRACED);

      tcsetpgrp(STDIN_FILENO, shell_pgid);

    }

    if (shell_is_interactive)
      /* Please only print shell prompts when standard input is not a tty */
      fprintf(stdout, "%d: ", ++line_num);

    /* Clean up memory */
    tokens_destroy(tokens);
  }

  return 0;
}
