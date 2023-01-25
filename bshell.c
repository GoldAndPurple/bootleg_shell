#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFSIZE 1024
#define MAXARGNUM 32
#define DELIMITER " \t\r\n\a"

int execute(char **args);
int call_exec(char **args);
int custom_pipe(char *line);
int custom_cd(char **args);
int custom_mkdir(char **args);
char **split_input(char *line);

// lab 1
int file_create(char *filename);
int file_read(char *filename);
int file_delete(char *filename, int squelch);
int file_move(char *from, char *to);
int file_link(char *from, char *to, int soft);
int file_copy(char *from, char *to);
int directory_list(char *path);

int file_create(char *filename) {
  char buf[BUFSIZE];
  int fd;
  if (fd = open(filename, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR) == -1) {
    perror("Overwrite?");
    if (read(STDIN_FILENO, buf, 3) != 2) {
      printf("[y]/[n]?\n");
      while ((read(STDIN_FILENO, buf, 3) != 2) && (buf[0] != 'y') &&
             (buf[0] != 'n')) {
        printf("[y]/[n]?\n");
      }
    }

    if (buf[0] == 'y') {
      if (fd = open(filename, O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR) != -1) {
        printf("File overwritten\n");
      } else {
        perror("Could not create file");
        return 3;
      }
    } else if (buf[0] == 'n') {
      printf("File creation cancelled\n");
    } else {
      perror("This should not happen");
      return 3;
    }
  }
  printf("File created\n");
  return 1;
}

int file_read(char *filename) {
  char buf[BUFSIZE];
  int fd = open(filename, O_RDONLY);
  if (fd != -1) {
    while (read(fd, buf, BUFSIZE) > 0) {
      write(STDOUT_FILENO, buf, BUFSIZE);
    }
    write(STDOUT_FILENO, "\n", 1);
  } else {
    perror("Could not read file");
    return 3;
  }
  return 1;
}

int file_delete(char *filename, int squelch) {
  if (remove(filename) == 0) {
    if (squelch == 0) {
      printf("Deleted successfully\n");
    }
  } else {
    if (squelch == 0) {
      perror("Could not delete file");
      return 3;
    }
  }
  return 1;
}

int file_move(char *from, char *to) {
  if ((rename(from, to) == 0) || (file_delete(from, 1) == 0)) {
    printf("Moved successfully\n");
  } else {
    perror("Could not move file");
    return 3;
  }
  return 1;
}

int file_link(char *from, char *to, int soft) {
  int result;
  if (soft) {
    result = symlink(from, to);
  } else {
    result = link(from, to);
  }
  if (result == 0) {
    printf("Linked successfully\n");
  } else {
    perror("Could not create link");
    return 3;
  }
  return 1;
}

int file_copy(char *from, char *to) { return file_link(from, to, 0); }

int directory_list(char *path) {
  DIR *dp;
  struct dirent *ep;

  dp = opendir(path);
  if (dp != NULL) {
    while (ep = readdir(dp)) {
      printf("%s\n", ep->d_name);
    }
  } else {
    perror("Could not open the directory");
    return 3;
  }

  return 1;
}

// lab 1 end

char *get_input(void) {
  int bufsize = BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char) * bufsize);
  int c;
  if (!buffer) {
    fprintf(stderr, "dynamic memory allocation error\n");
    exit(EXIT_FAILURE);
  }
  while (1) {
    c = getchar();
    if (c == EOF) {
      exit(EXIT_SUCCESS);
    } else if (c == '\n') {
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }
    position++;

    if (position >= bufsize) {
      fprintf(stderr, "buffer size exceeded\n");
      exit(EXIT_FAILURE);
    }
  }
}

char **split_input(char *line) {
  int bufsize = MAXARGNUM, position = 0;
  char **tokens = malloc(bufsize * sizeof(char *));
  char *token, **tokens_backup;

  if (!tokens) {
    fprintf(stderr, "dynamic memory allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, DELIMITER);
  while (token != NULL) {
    if (token[0] == '*' || token[strlen(token) - 1] == '*') {
      DIR *d;
      struct dirent *dir;
      d = opendir(".");
      if (d) {
        while ((dir = readdir(d)) != NULL) {
          if (dir->d_name[0] != '.') {
            if (strlen(token) == 1 ||
                (token[0] == '*' &&
                 (!strncmp(
                     token + 1,
                     (dir->d_name + strlen(dir->d_name) - strlen(token) + 1),
                     strlen(token) - 1))) ||
                (token[strlen(token) - 1] == '*' &&
                 (!strncmp(token, dir->d_name, strlen(token) - 1)))) {
              tokens[position] = dir->d_name;
              position++;
            }
          }
        }
        closedir(d);
      }
    } else {
      tokens[position] = token;
      position++;
    }

    if (position >= bufsize) {
      bufsize += MAXARGNUM;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char *));
      if (!tokens) {
        free(tokens_backup);
        fprintf(stderr, "dynamic memory allocation error\n");
        exit(EXIT_FAILURE);
      }
    }

    token = strtok(NULL, DELIMITER);
  }
  tokens[position] = NULL;
  return tokens;
}

int custom_cd(char **args) {
  if (args[1] == NULL) {
    if (chdir(getenv("HOME")) != 0) {
      perror("change directory failed");
      return 3;
    }
  } else {
    if (chdir(args[1]) != 0) {
      perror("change directory failed");
      return 3;
    }
  }
  return 1;
}

int custom_pipe(char *line) {
  int i, commandc = 0, numpipes = 0, status;
  pid_t pid;
  char **args;
  for (i = 0; line[i] != '\0'; i++) {
    if (i > 0) {
      if (line[i] == '|' && line[i + 1] != '|' && line[i - 1] != '|') {
        numpipes++;
      }
    }
  }
  int *pipefds = (int *)malloc((2 * numpipes) * sizeof(int));
  char *token = (char *)malloc((128) * sizeof(char));
  token = strtok_r(line, "|", &line);
  for (i = 0; i < numpipes; i++) {
    if (pipe(pipefds + i * 2) < 0) {
      perror("pipe creation failed");
      return 3;
    }
  }
  do {
    pid = fork();
    if (pid == 0) { // child process
      if (commandc != 0) {
        if (dup2(pipefds[(commandc - 1) * 2], 0) < 0) {
          perror("child couldnt get input");
          exit(1);
        }
      }
      if (commandc != numpipes) {
        if (dup2(pipefds[commandc * 2 + 1], 1) < 0) {
          perror("child couldnt output");
          exit(1);
        }
      }
      for (i = 0; i < 2 * numpipes; i++) {
        close(pipefds[i]);
      }
      args = split_input(token);
      execvp(args[0], args);
      perror("exec failed");
      exit(1);
    } else if (pid < 0) {
      perror("fork() failed");
      return 3;
    } // fork error
    commandc++; // parent process
  } while (commandc < numpipes + 1 && (token = strtok_r(NULL, "|", &line)));
  for (i = 0; i < 2 * numpipes; i++) {
    close(pipefds[i]);
  }
  free(pipefds);
  return 1;
}

int custom_mkdir(char **args) {
  int c, argc = 0;
  mode_t mode = 0777;
  char path[128];
  getcwd(path, sizeof(path));
  while (args[argc] != NULL) {
    argc++;
  }
  if (*args) {
    strcat(path, "/");
    strcat(path, *args);
    if (mkdir(path, mode) == -1) {
      perror("mkdir failed ");
      return 3;
    }
  } else {
    fprintf(stderr, "expected directory name argument for \"mkdir\"\n");
    return 3;
  }
  return 1;
}

int call_exec(char **args) {
  pid_t pid;
  int status;
  pid = fork();
  if (pid == 0) { // child process
    if (execvp(args[0], args) == -1) {
      perror("child process: execution error");
      return 3;
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("forking error");
    return 3;
  } else { // parent process
    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
  if (status == 0)
    return 1;
  return 3;
}

int execute(char **args) {
  if (!strcmp(args[0], "cd")) {
    return custom_cd(args);
  } else if (!strcmp(args[0], "mkdir")) {
    return custom_mkdir(args);
  } else if (!strcmp(args[0], "exit")) {
    exit(0);
  }
  // lab 1 commands
  else if (!strcmp(args[0], "create")) {
    return file_create(args[1]);
  } else if (!strcmp(args[0], "read")) {
    return file_read(args[1]);
  } else if (!strcmp(args[0], "copy")) {
    return file_copy(args[1], args[2]);
  } else if (!strcmp(args[0], "delete")) {
    return file_delete(args[1], 1);
  } else if (!strcmp(args[0], "move")) {
    return file_move(args[1], args[2]);
  } else if (!strcmp(args[0], "link")) {
    int linktype;
    if (!strcmp(args[3], "soft")) {
      linktype = 1;
    } else if (!strcmp(args[3], "hard")) {
      linktype = 0;
    }
    return file_link(args[1], args[2], linktype);
  } else if (!strcmp(args[0], "list")) {
    return directory_list(args[1]);
  }
  
  return call_exec(args);
}

int main(int argc, char **argv) {
  int i, j = 0, status = 1;
  char *line, *oneline, *tmp, **args, cwd_name[256];

  // loop while no errors are around
  do {
    j = 0;
    getcwd(cwd_name, sizeof(cwd_name));
    printf("%s$ ", cwd_name);
    line = get_input();

    // check multiline inputs
    for (i = 0; line[i] != '\0'; i++) {
      if (line[i] == '\\') {
        j++;
      } else if (
          i > 0 && line[i] == '|' && line[i - 1] != '|' && line[i + 1] != '|') {
        custom_pipe(line);
        goto LOOP_END;
      }
    }
    tmp = (char *)malloc((strlen(line) + 1) * sizeof(char));
    strcpy(tmp, "\\");
    strcat(tmp, line);
    oneline = strtok_r(tmp, "\\", &tmp);
    // attempt to execute the separated command string
    do {
      if (oneline != NULL) {
        args = split_input(oneline);
        status = execute(args);
      }
      oneline = strtok_r(NULL, "\\", &tmp);
      j--;
    } while (j > -1 && status);

  LOOP_END:
    free(line);

  } while (status);
  return 0;
}
