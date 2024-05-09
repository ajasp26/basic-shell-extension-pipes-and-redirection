/**
 * Compile via gcc -g -Wall -Werror main.c -o main.o
 * Execute via ./main.o
 *
 * @author Alex Jasper
 * @version 04/22/2024
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <limits.h>

// Function prototypes
int find_pipe_idx(size_t num_args, char *args[]);
void execute_pipe(char *args[], int pipe_idx, size_t num_args);

bool is_valid_redirection(size_t i, size_t num_args, char *args[])
{
    // Returns true if there is a valid argument after the redirection symbol and it's not another redirection symbol.
    return i + 1 < num_args && args[i + 1][0] != '<' && args[i + 1][0] != '>';
}

/**
 * Scans the command arguments for input redirection symbol ('<') and modifies the command arguments to
 * exclude the redirection syntax, while also setting the input redirection file.
 *
 * @param num_args Pointer to the number of arguments.
 * @param args Array of argument strings.
 * @param input_file Pointer to a string pointer, set to the input file name if redirection is found.
 * @return true if input redirection is found and set, false otherwise.
 */
bool redirect_input(size_t *num_args, char *args[], char **input_file)
{
    for (size_t i = 0; i < *num_args; i++)
    {
        if (strcmp(args[i], "<") == 0)
        {
            if (!is_valid_redirection(i, *num_args, args))
            {
                fprintf(stderr, "Syntax error near unexpected token `%s'\n", args[i + 1]);
                return false;
            }
            *input_file = strdup(args[i + 1]);
            for (size_t j = i; j < *num_args - 2; j++)
            {
                args[j] = args[j + 2];
            }
            *num_args -= 2;         // Adjust the number of arguments.
            args[*num_args] = NULL; // Null terminate the modified argument list.
            return true;
        }
    }
    return false;
}

/**
 * Scans the command arguments for output redirection symbol ('>') and modifies the command arguments to
 * exclude the redirection syntax, while also setting the output redirection file.
 *
 * @param num_args Pointer to the number of arguments.
 * @param args Array of argument strings.
 * @param output_file Pointer to a string pointer, set to the output file name if redirection is found.
 * @return true if output redirection is found and set, false otherwise.
 */
bool redirect_output(size_t *num_args, char *args[], char **output_file)
{
    for (size_t i = 0; i < *num_args; i++)
    {
        if (strcmp(args[i], ">") == 0)
        {
            if (!is_valid_redirection(i, *num_args, args))
            {
                fprintf(stderr, "Syntax error near unexpected token `%s'\n", args[i + 1]);
                return false;
            }
            *output_file = strdup(args[i + 1]);
            for (size_t j = i; j < *num_args - 2; j++)
            {
                args[j] = args[j + 2];
            }
            *num_args -= 2;
            args[*num_args] = NULL;
            return true;
        }
    }
    return false;
}

/**
 * Executes the command specified by args array. Handles input and output redirection if specified.
 * Changes the current directory if the command is 'cd'. Uses fork and execvp for other commands.
 *
 * @param num_args Number of arguments in args.
 * @param args Array of arguments.
 */
void execute_cmd(size_t num_args, char *args[])
{
    if (num_args == 0)
        return;

    int pipe_idx = find_pipe_idx(num_args, args);
    if (pipe_idx != -1)
    {
        execute_pipe(args, pipe_idx, num_args);
        return;
    }

    // Handle input and output redirection.
    char *input_file = NULL, *output_file = NULL;
    if (redirect_input(&num_args, args, &input_file))
    {
        if (!freopen(input_file, "r", stdin))
        {
            perror("Failed to redirect input");
            free(input_file);
            return;
        }
        free(input_file);
    }
    if (redirect_output(&num_args, args, &output_file))
    {
        if (!freopen(output_file, "w", stdout))
        {
            perror("Failed to redirect output");
            free(output_file);
            return;
        }
        free(output_file);
    }

    // Special handling for the 'cd' command.
    if (strcmp(args[0], "cd") == 0)
    {
        if (num_args < 2)
        {
            fprintf(stderr, "cd: missing operand\n");
        }
        else if (chdir(args[1]) != 0)
        {
            perror("cd");
        }
        return;
    }

    // For all other commands, use fork and exec.
    pid_t pid = fork();
    if (pid == 0)
    {
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
        // Attempt to restore standard input and output streams.
        if (stdin != freopen("/dev/tty", "r", stdin))
            perror("Restoring stdin failed");
        if (stdout != freopen("/dev/tty", "w", stdout))
            perror("Restoring stdout failed");
    }
    else
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
}

/**
 * Parses the command line input into tokens that are executed by execute_cmd. Allocates memory
 * dynamically for the arguments array which is managed within this function.
 *
 * @param input The command line input string.
 */
void parse_cmd(char *input)
{
    char **args = NULL;
    size_t num_args = 0, capacity = 10;
    args = malloc(capacity * sizeof(char *));
    if (!args)
    {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    char *token = strtok(input, " \n");
    while (token != NULL)
    {
        if (num_args >= capacity) // Reallocate memory if capacity is exceeded.
        {
            capacity *= 2;
            args = realloc(args, capacity * sizeof(char *));
            if (!args)
            {
                perror("realloc failed");
                exit(EXIT_FAILURE);
            }
        }
        args[num_args++] = strdup(token); // Store the token and increase the count.
        token = strtok(NULL, " \n");
    }
    args[num_args] = NULL; // Null terminate the list of arguments.
    execute_cmd(num_args, args);

    // Clean up: free memory allocated for arguments.
    for (size_t i = 0; i < num_args; i++)
    {
        free(args[i]);
    }
    free(args);
}

/**
 * Displays a help message listing built-in commands and supported features.
 */
void execute_help(void)
{
    printf("Help:\n"
           "Type program names and arguments, and hit enter.\n"
           "The following are built-in:\n"
           "  * cd <dir> - change the directory to <dir>\n"
           "  * help - display this help message\n"
           "  * quit - exit the shell\n"
           "Supported features: piping (|), redirection (>)\n");
}

/**
 * Finds the index of the pipe symbol ('|') in the command arguments, which indicates that
 * the command should be split into two subprocesses with the output of the first connected to
 * the input of the second.
 *
 * @param num_args The number of arguments in the args array.
 * @param args The array of arguments.
 * @return The index of the pipe symbol if found, or -1 if not found.
 */
int find_pipe_idx(const size_t num_args, char *args[])
{
    for (size_t i = 0; i < num_args; i++)
    {
        if (strcmp(args[i], "|") == 0)
        {
            return (int)i; // Return the index of the pipe symbol.
        }
    }
    return -1; // Return -1 if no pipe symbol is found.
}

/**
 * Executes two piped commands. The command before the pipe symbol ('|') is executed with its output
 * redirected to the input of the command following the pipe. This function handles the creation and
 * management of the pipe, forks processes for each command segment, and connects the two via the pipe.
 *
 * @param args The complete array of command arguments including the pipe symbol.
 * @param pipe_idx The index of the pipe symbol in the args array.
 * @param num_args The total number of arguments in the args array.
 */
void execute_pipe(char *args[], int pipe_idx, size_t num_args)
{

    char *lhs[pipe_idx + 1];        // Command before the pipe.
    char *rhs[num_args - pipe_idx]; // Command after the pipe.
    for (int i = 0; i < pipe_idx; i++)
    {
        lhs[i] = args[i];
    }
    lhs[pipe_idx] = NULL;
    for (size_t i = 0; i < num_args - pipe_idx - 1; i++)
    {
        rhs[i] = args[i + pipe_idx + 1];
    }
    rhs[num_args - pipe_idx - 1] = NULL;

    int fd[2];
    if (pipe(fd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    pid_t pid1 = fork();
    if (pid1 == 0) // Child process: will handle the command before the pipe.
    {
        close(fd[0]);               // Close the read end of the pipe.
        dup2(fd[1], STDOUT_FILENO); // Redirect stdout to the write end of the pipe.
        close(fd[1]);               // Close the original write end.
        execvp(lhs[0], lhs);        // Execute the command.
        perror("execvp lhs");
        exit(EXIT_FAILURE);
    }
    pid_t pid2 = fork(); // Fork the second process.
    if (pid2 == 0)       // Child process: will handle the command after the pipe.
    {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        execvp(rhs[0], rhs);
        perror("execvp rhs");
        exit(EXIT_FAILURE);
    }

    // Parent process: closes both ends of the pipe and waits for both children to finish.
    close(fd[0]);
    close(fd[1]);
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}

/*
 * Main function will implement an infinite loop that reads user input until "quit" is entered.
 * getline() is used and it will handle the inputs up to LINE_MAX characters. No functionality for
 * not also quiting with Ctrl+C. Not sure if that is necessary but figured I would mention that. Doesn't
 * seem necessary right now, but am happy to implement.
 * Now also displays a welcome message while handling current working directory and user input.
 */
int main(void)
{
    char *input = NULL;
    size_t bufsize = 0;

    printf("Welcome to Alex's Shell.\n"
           "Enter a shell command(e.g., cd, ls, ...).\n"
           "Piping and redirection are supported. Version 1.0\n");

    while (1)
    {
        char cwd[PATH_MAX]; // Buffer to hold the current working directory.
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s$ ", cwd);
        }
        else
        {
            perror("getcwd");
            exit(EXIT_FAILURE);
        }

        getline(&input, &bufsize, stdin);

        if (strncmp(input, "quit\n", 5) == 0)
        {
            break;
        }
        else if (strncmp(input, "help\n", 5) == 0)
        {
            execute_help();
            continue;
        }

        parse_cmd(input); // Parse and execute the command.
    }

    free(input); // Free the input buffer
    return 0;
}