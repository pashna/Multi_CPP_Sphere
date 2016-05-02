// using Linux

#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/stat.h>

bool ismanaging(char c)
{
    return c == '>' || c == '<' || c == '|' || c == '&' || c == ';';
}

void parse(char *input, char **&cmd)
{
    cmd = (char **) malloc(sizeof(*cmd));
    cmd[0] = (char *) malloc(sizeof(**cmd));

    int cap = 1, pos = 0, posi = 0, n = int(strlen(input)), currcap = 1, qopen = 0;

    for (int i = 0; i < n; i++) {
        if (input[i] == '\'') {
            qopen ^= 1;
            continue;
        }

        if (!qopen && posi > 0 && (isspace(input[i]) || (ismanaging(input[i]) ^ ismanaging(input[i - 1])))) {
            pos++;
            posi = 0;
        }
        
        if (pos + 1 == cap) {
            cmd = (char **) realloc(cmd, sizeof(*cmd) * cap * 2);

            for (int j = cap; j < cap * 2; j++) {
                cmd[j] = NULL;
            }
            cap *= 2;
        }

        if (cmd[pos] == NULL) {
            cmd[pos] = (char *) malloc(sizeof(**cmd));
            cmd[pos][posi] = 0;
            currcap = 1;
        }

        if (posi + 1 == currcap) {
            cmd[pos] = (char *) realloc(cmd[pos], sizeof(**cmd) * currcap * 2);

            for (int j = currcap; j < currcap * 2; j++) {
                cmd[pos][j] = 0;
            }
            currcap *= 2;
        }
        
        if (qopen || !isspace(input[i])) {
            cmd[pos][posi] = input[i];
            posi++;
        }
    }

    if (posi == 0) {
        free(cmd[pos]);
        cmd[pos] = NULL;
    }
}

void process(char **cmd, int start = 0)
{
    char **argv = (char **) malloc(sizeof(*argv));
    argv[0] = NULL;

    int cap = 1, pos = 0, i = 0;
    int fdin = -1, fdout = -1;

    for (i = start; cmd[i] != NULL && strcmp(cmd[i], "&&") != 0 && strcmp(cmd[i], "||") != 0 &&
    strcmp(cmd[i], "|") != 0 && strcmp(cmd[i], ";") != 0; i++) {
        if (strcmp(cmd[i], "<") == 0) {
            fdin = open(cmd[i + 1], O_RDONLY | O_CLOEXEC);
            i++;
        } else if (strcmp(cmd[i], ">") == 0) {
            fdout = open(cmd[i + 1], O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
            i++;
        } else if (strcmp(cmd[i], ">>") == 0) {
            fdout = open(cmd[i + 1], O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
            i++;
        } else {
            if (pos + 1 == cap) {
                argv = (char **) realloc(argv, sizeof(*argv) * cap * 2);
                
                for (int j = cap; j < cap * 2; j++) {
                    argv[j] = NULL;
                }
                cap *= 2;
            }
            argv[pos] = cmd[i];
            pos++;
        }
    }
    bool ispipe = (cmd[i] != NULL && strcmp(cmd[i], "|") == 0);
    int fd[2];

    if (ispipe) {
        assert(pipe(fd) == 0);
    }

    if (!fork()) {
        if (ispipe) {
            close(fd[0]);
            dup2(fd[1], STDOUT_FILENO);
            close(fd[1]);
        }

        if (fdin != -1) {
            dup2(fdin, STDIN_FILENO);
        }

        if (fdout != -1) {
            dup2(fdout, STDOUT_FILENO);
        }
        execvp(argv[0], argv);
        exit(1);
    }

    if (fdin != -1) {
        close(fdin);
    }
    if (fdout != -1) {
        close(fdout);
    }
    int status;

    if (cmd[i] != NULL && (strcmp(cmd[i], "&&") == 0 || strcmp(cmd[i], "||") == 0 || strcmp(cmd[i], ";") == 0)) {
        wait(&status);

        if ((strcmp(cmd[i], "&&") == 0 && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) ||
        (strcmp(cmd[i], "||") == 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
            char *prev = cmd[i];

            while (cmd[i] != NULL && (strcmp(cmd[i], prev) == 0 || (strcmp(cmd[i], "&&") != 0 && strcmp(cmd[i], "||") != 0 &&
            strcmp(cmd[i], "|") != 0 && strcmp(cmd[i], ";") != 0))) {
                i++;
            }
        }
    }

    if (cmd[i] != NULL && !fork()) {
        if (ispipe) {
            close(fd[1]);
            dup2(fd[0], STDIN_FILENO);
            close(fd[0]);
        }
        process(cmd, i + 1);
    }

    if (ispipe) {
        close(fd[0]);
        close(fd[1]);
    }

    while ((wait(&status)) != -1);
    free(argv);
    exit(WEXITSTATUS(status));
}

int main(void)
{
    char input[1 << 16];

    while (fgets(input, sizeof(input), stdin)) {
        char **cmd = NULL;
        parse(input, cmd);
        bool back = 0;

        for (int i = 0; cmd[i] != NULL; i++) {
            if (cmd[i + 1] == NULL && strcmp(cmd[i], "&") == 0) {
                free(cmd[i]);
                cmd[i] = NULL;
                back = 1;
            }
        }

        if (!back) {
            if (!fork()) {
                process(cmd);
            }
            wait(NULL);
        } else {
            if (!fork()) {
                int pid;

                if (!(pid = fork())) {
                    process(cmd);
                }
                fprintf(stderr, "Spawned child process %d\n", pid);
                int status;
                wait(&status);

                fprintf(stderr, "Process %d exited: %d\n", pid, WEXITSTATUS(status));

                for (int i = 0; cmd[i] != NULL; i++) {
                    free(cmd[i]);
                }
                free(cmd);
                exit(0);
            }
        }
    }
    return 0;
}
