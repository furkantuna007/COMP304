#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <termios.h> // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>
//part2
#include <fcntl.h> // for open()
//PART3 - Roll dice
#include <time.h>

//PART3 - cloc
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fnmatch.h>

//PART3 - helper - custom command
#include <dirent.h>
#include <ctype.h>

//PART3 - cdh
#include <limits.h>
const char *history_file = "cdh_history.txt";


//COMP304 Project1
//Yiğithan Şanlık 64117
//Furkan Tuna 69730

const char *sysname = "mishell";
const char *PATH_ENV_VAR = "PATH";

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t {
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command) {
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n",
		   command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");

	for (i = 0; i < 3; i++) {
		printf("\t\t%d: %s\n", i,
			   command->redirects[i] ? command->redirects[i] : "N/A");
	}

	printf("\tArguments (%d):\n", command->arg_count);

	for (i = 0; i < command->arg_count; ++i) {
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	}

	if (command->next) {
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command) {
	if (command->arg_count) {
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}

	for (int i = 0; i < 3; ++i) {
		if (command->redirects[i])
			free(command->redirects[i]);
	}

	if (command->next) {
		free_command(command->next);
		command->next = NULL;
	}

	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt() {
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command) {
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);

	// trim left whitespace
	while (len > 0 && strchr(splitters, buf[0]) != NULL) {
		buf++;
		len--;
	}

	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL) {
		// trim right whitespace
		buf[--len] = 0;
	}

	// auto-complete
	if (len > 0 && buf[len - 1] == '?') {
		command->auto_complete = true;
	}

	// background
	if (len > 0 && buf[len - 1] == '&') {
		command->background = true;
	}

	char *pch = strtok(buf, splitters);
	if (pch == NULL) {
		command->name = (char *)malloc(1);
		command->name[0] = 0;
	} else {
		command->name = (char *)malloc(strlen(pch) + 1);
		strcpy(command->name, pch);
	}

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1) {
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// trim left whitespace
		while (len > 0 && strchr(splitters, arg[0]) != NULL) {
			arg++;
			len--;
		}

		// trim right whitespace
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL) {
			arg[--len] = 0;
		}

		// empty arg, go for next
		if (len == 0) {
			continue;
		}

		// piping to another command
		if (strcmp(arg, "|") == 0) {
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0) {
			// handled before
			continue;
		}

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<') {
			redirect_index = 0;
		}

		if (arg[0] == '>') {
			if (len > 1 && arg[1] == '>') {
				redirect_index = 2;
				arg++;
				len--;
			} else {
				redirect_index = 1;
			}
		}

		if (redirect_index != -1) {
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 &&
			((arg[0] == '"' && arg[len - 1] == '"') ||
			 (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}

		command->args =
			(char **)realloc(command->args, sizeof(char *) * (arg_index + 1));

		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;

	// increase args size by 2
	command->args = (char **)realloc(
		command->args, sizeof(char *) * (command->arg_count += 2));

	// shift everything forward by 1
	for (int i = command->arg_count - 2; i > 0; --i) {
		command->args[i] = command->args[i - 1];
	}

	// set args[0] as a copy of name
	command->args[0] = strdup(command->name);

	// set args[arg_count-1] (last) to NULL
	command->args[command->arg_count - 1] = NULL;

	return 0;
}

void prompt_backspace() {
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command) {
	size_t index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &=
		~(ICANON |
		  ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	show_prompt();
	buf[0] = 0;

	while (1) {
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		// handle tab
		if (c == 9) {
			buf[index++] = '?'; // autocomplete
			break;
		}

		// handle backspace
		if (c == 127) {
			if (index > 0) {
				prompt_backspace();
				index--;
			}
			continue;
		}

		if (c == 27 || c == 91 || c == 66 || c == 67 || c == 68) {
			continue;
		}

		// up arrow
		if (c == 65) {
			while (index > 0) {
				prompt_backspace();
				index--;
			}

			char tmpbuf[4096];
			printf("%s", oldbuf);
			strcpy(tmpbuf, buf);
			strcpy(buf, oldbuf);
			strcpy(oldbuf, tmpbuf);
			index += strlen(buf);
			continue;
		}

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}

	// trim newline from the end
	if (index > 0 && buf[index - 1] == '\n') {
		index--;
	}

	// null terminate string
	buf[index++] = '\0';

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);

// mymodule.c (psvis.c) part V
static int is_module_loaded() {
    return system("lsmod | grep mymodule > /dev/null") == 0;
}

int psvis(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        printf("Usage: psvis <PID> <output_file>\n");
        return 1;
    }

    if (getuid()) {
        printf("You need to be a superuser to use the psvis command.\n");
        return 1;
    }

    if (!is_module_loaded()) {
        printf("Loading kernel module...\n");
        system("sudo insmod module/mymodule.ko");
    } else {
        printf("Kernel module is already loaded.\n");
    }

    char buff1[128];
    sprintf(buff1, "sudo dmesg -C; sudo rmmod mymodule; sudo insmod module/mymodule.ko pid=%d", atoi(args[1]));
    system(buff1);

    // draw
    FILE *data = fopen("data.txt", "w");
    system("echo \"digraph G {\n\" > data.txt; sudo dmesg | cut -d [ -f2- | cut -d ] -f2- | grep 'Start time' >> data.txt; echo \"}\n\" >> data.txt");
    fclose(data);

    char buff2[128];
    sprintf(buff2, "cat data.txt | dot -Tpng > %s.png", args[2]);
    system(buff2);

    return 0;
}


//header psvis
int psvis(char **args);

//Func prototypes for my custom command(Yiğithan Şanlık)
void openHelper();
void addHelper();
char helper[1024]; //our file name for addHelper / openHelper commands of choice
int main() {

	while (1) {
		struct command_t *command = malloc(sizeof(struct command_t));
		//We need to get path name of the working directory with getcwd for addHelper and showHelper commands.
			getcwd(helper, sizeof(helper));

			strcat(helper, "/helper.txt"); //strcat to concatenate


		// set all bytes to 0
		memset(command, 0, sizeof(struct command_t));

		int code;
		code = prompt(command);
		if (code == EXIT) {
			break;
		}

		code = process_command(command);
		if (code == EXIT) {
			break;
		}

		free_command(command);
	}

	printf("\n");
	return 0;
}
//PART3 We implemented dice_roll_args function as mentioned in our pdf
//With 3 input
int dice_roll_args(char *arg_str, int *number_roll, int * number_side) {

    int i = 0;
    while (arg_str[i] != '\0') {
        if (arg_str[i] == 'd') {
            if (i == 0) {
                *number_roll = 1;
            }
            else {
                // Set number of rolls
                char roll_str[i]; // Corrected length
                strncpy(roll_str, arg_str, i); // Corrected length
                roll_str[i] = '\0';
                *number_roll = atoi(roll_str);
            }
            // Set number of sides
            int side_str_len = strlen(arg_str) - (i+1) + 1;

            if (side_str_len <= 1) {
                return 1;
            }

            char side_str[side_str_len + 1];
            strncpy(side_str, arg_str + i + 1, side_str_len);
            side_str[side_str_len] = '\0';
            *number_side = atoi(side_str);
            }
        i++;
        }

        return 0;
}

//Command of Choice 1 addHelper() and openHelper()
	//Yigithan Sanlik

void addHelper() {
    FILE *file = fopen(helper, "a");
    if (file) {
        char helper[2048];
        printf("Create a helper for our shell");
        fgets(helper, sizeof(helper), stdin);
        fputs(helper, file);
        fclose(file);
    } else {
        perror("Error opening the file");
    }
}

void openHelper() {
    FILE *file = fopen(helper, "r");
    if (file) {
        char ln[500];
        int i = 1;
        while (fgets(ln, sizeof(ln), file)) {
            printf("%d %s", i, ln);
            i++;
        }
        fclose(file);
    } else {
        perror("Error opening the file");
    }
}

//PART3-cdh save the current directory to the history file
void save_cwd_to_history() {
    FILE *file = fopen(history_file, "a");
    if (file) {
        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            fprintf(file, "%s\n", cwd);
        }
        fclose(file);
    } else {
        perror("save_cwd_to_history");
    }
}

void cdh() {
    // Save the current directory to the history file
    save_cwd_to_history();

    // Read and display recent directories
    char *directories[10] = {NULL};
    int dir_count = 0;

    FILE *file = fopen(history_file, "r");
    if (file) {
        char line[PATH_MAX];
        while (fgets(line, sizeof(line), file) && dir_count < 10) {
            // Remove newline character
            line[strcspn(line, "\n")] = 0;

            // Check if the directory is already in the list
            int duplicate = 0;
            for (int i = 0; i < dir_count; i++) {
                if (strcmp(directories[i], line) == 0) {
                    duplicate = 1;
                    break;
                }
            }

            // If not a duplicate, add to the list
            if (!duplicate) {
                directories[dir_count] = strdup(line);
                dir_count++;
            }
        }
        fclose(file);
    } else {
        perror("cdh");
    }

    // Print the list of recent directories
    printf("Recent directories:\n");
    for (int i = 0; i < dir_count; i++) {
        printf("%d(%c) %s\n", i + 1, 'a' + i, directories[i]);
    }

    // Get the user's choice and change to the chosen directory
    printf("Choose a directory (number or letter): ");
    char choice[10];
    fgets(choice, sizeof(choice), stdin);

    int index = -1;
    if (sscanf(choice, "%d", &index) == 1) {
        index -= 1;
    } else {
        char letter;
        if (sscanf(choice, "%c", &letter) == 1) {
            index = letter - 'a';
        }
    }

    if (index >= 0 && index < dir_count) {
        chdir(directories[index]);
        printf("Changed directory to %s\n", directories[index]);
    } else {
        printf("Invalid choice\n");
    }

    // Free the memory allocated for the directory list
    for (int i = 0; i < dir_count; i++) {
        free(directories[i]);
    }
}

//Part3 - cloc
void count_lines(const char *filename, int *blank_lines, int *comment_lines, int *code_lines) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        return;
    }

    char line[2048];
    int in_multiline_comment = 0;
    while (fgets(line, sizeof(line), file)) {
        char *trimmed = strtok(line, " \t");
        if (!trimmed || strlen(trimmed) == 0) {
            (*blank_lines)++;
        } else if (trimmed[0] == '#' || (trimmed[0] == '/' && trimmed[1] == '/') ||
                   (in_multiline_comment || (trimmed[0] == '/' && trimmed[1] == '*'))) {
            (*comment_lines)++;
            if (trimmed[0] == '/' && trimmed[1] == '*') {
                in_multiline_comment = 1;
            }
            if (strstr(trimmed, "*/")) {
                in_multiline_comment = 0;
            }
        } else {
            (*code_lines)++;
        }
    }

    fclose(file);
}

void cloc(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("cloc");
        return;
    }

    int total_files = 0, processed_files = 0;
    int c_files = 0, cpp_files = 0, py_files = 0, txt_files = 0;
    int c_blank_lines = 0, cpp_blank_lines = 0, py_blank_lines = 0, txt_blank_lines = 0;
    int c_comment_lines = 0, cpp_comment_lines = 0, py_comment_lines = 0, txt_comment_lines = 0;
    int c_code_lines = 0, cpp_code_lines = 0, py_code_lines = 0, txt_code_lines = 0;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        total_files++;

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        struct stat sb;
        if (stat(full_path, &sb) == -1) {
            perror("cloc");
            continue;
        }

        if (S_ISDIR(sb.st_mode)) {
            cloc(full_path);
        } else if (S_ISREG(sb.st_mode)) {
            processed_files++;

            int blank_lines = 0, comment_lines = 0, code_lines = 0;
            count_lines(full_path, &blank_lines, &comment_lines, &code_lines);

            if (fnmatch("*.c", entry->d_name, 0) == 0) {
                c_files++;
                c_blank_lines += blank_lines;
                c_comment_lines += comment_lines;
                c_code_lines += code_lines;
            } else if (fnmatch("*.cpp", entry->d_name, 0) == 0 || fnmatch("*.hpp", entry->d_name, 0) == 0) {
                cpp_files++;
                cpp_blank_lines += blank_lines;
                cpp_comment_lines += comment_lines;
                cpp_code_lines += code_lines;
            } else if (fnmatch("*.py", entry->d_name, 0) == 0) {
                            py_files++;
                            py_blank_lines += blank_lines;
                            py_comment_lines += comment_lines;
                            py_code_lines += code_lines;
            } else if (fnmatch("*.txt", entry->d_name, 0) == 0) {
                            txt_files++;
                            txt_blank_lines += blank_lines;
                            txt_comment_lines += comment_lines;
                            txt_code_lines += code_lines;
             }
          }
        }

                printf("Total files: %d\n", total_files);
                printf("Processed files: %d\n", processed_files);
                printf(".c    Files: %d, Blank lines: %d, Comment lines: %d, Code lines: %d\n", c_files, c_blank_lines, c_comment_lines, c_code_lines);
                printf(".cpp  Files: %d, Blank lines: %d, Comment lines: %d, Code lines: %d\n", cpp_files, cpp_blank_lines, cpp_comment_lines, cpp_code_lines);
                printf(".py   Files: %d, Blank lines: %d, Comment lines: %d, Code lines: %d\n", py_files, py_blank_lines, py_comment_lines, py_code_lines);
                printf(".txt  Files: %d, Blank lines: %d, Comment lines: %d, Code lines: %d\n", txt_files, txt_blank_lines, txt_comment_lines, txt_code_lines);

                closedir(dir);
            }






int process_command(struct command_t *command) {
	int r;

	if (strcmp(command->name, "") == 0) {
		return SUCCESS;
	}

	if (strcmp(command->name, "exit") == 0) {
		return EXIT;
	}
	//changed arg_count to 1
	if (strcmp(command->name, "cd") == 0) {
		if (command->arg_count > 0) {
			r = chdir(command->args[1]);
			if (r == -1) {
				printf("-%s: %s: %s\n", sysname, command->name,
					   strerror(errno));
			}

			return SUCCESS;
		}
	}
	// Roll Dice Command Implementation
	if (strcmp(command->name, "roll") == 0) {
			int number_roll = 1;
			int number_sides;

			if (dice_roll_args(command->args[1], &number_roll, &number_sides) == 1) {
				printf("Your input is wrong. Re-try with new inputs\n");
			}

			int sum = 0;
			int rolls[number_roll];
			srand(time(NULL));
			for (int i = 0; i < number_roll; i++) {
				rolls[i] = rand() % number_sides + 1;
				sum += rolls[i];
			}

			printf("Rolled %d", sum);
			if (number_roll == 1) {
				printf("\n");
				return SUCCESS;
			}

			printf(" (");
			for (int c = 0; c < number_roll; c++) {
				printf(c < number_roll - 1 ? "%d + " : "%d)\n", rolls[c]);
			}
			return SUCCESS;
		}

	 // Add these conditions for addHelper and openHelper commands
	if (strcmp(command->name, "addHelper") == 0) {
	    addHelper();
	    return SUCCESS;
	}

	if (strcmp(command->name, "openHelper") == 0) {
	    openHelper();
	    return SUCCESS;
	}

	//cdh
	if (strcmp(command->name, "cdh") == 0) {
	    cdh();
	    return SUCCESS;
	}

	//cloc
	if (strcmp(command->name, "cloc") == 0) {
	    if (command->arg_count > 0) {
	        cloc(command->args[1]);
	        return SUCCESS;
	    } else {
	        printf("-%s: %s: missing directory argument\n", sysname, command->name);
	        return UNKNOWN;

	    }
	}
	//psvis
	if (strcmp(command->name, "psvis") == 0) {
	    if (command->arg_count > 1) {
	        psvis(command->args);
	        return SUCCESS;
	    } else {
	        printf("-%s: %s: missing arguments\n", sysname, command->name);
	        return UNKNOWN;
	    }
	}




	pid_t pid = fork();
	// child
	if (pid == 0) {

		//PART2 Implementation
		// I/O Redirection
		int input_fd = -1, output_fd = -1, append_fd = -1;

		//Operator < :The input of the program on the left-hand side is read from a file
				if (command->redirects[0]) {
				    input_fd = open(command->redirects[0], O_RDONLY);
				    if (input_fd == -1) {
				        printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
				        exit(EXIT_FAILURE);
				    }
				    dup2(input_fd, STDIN_FILENO);
				}
				//Operator > :The file is created if it does not exist, and truncated otherwise.
				if (command->redirects[1]) {
						    output_fd = open(command->redirects[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
						    if (output_fd == -1) {
						        printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
						        exit(EXIT_FAILURE);
						    }
						    dup2(output_fd, STDOUT_FILENO);
						}
		//Operator >> :The file is created if it does not exist,, but the output is appended if the file already exists.
		if (command->redirects[2]) {
		    append_fd = open(command->redirects[2], O_WRONLY | O_CREAT | O_APPEND, 0644);
		    if (append_fd == -1) {
		        printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
		        exit(EXIT_FAILURE);
		    }
		    dup2(append_fd, STDOUT_FILENO);
		}

		/// This shows how to do exec with environ (but is not available on MacOs)
		// extern char** environ; // environment variables
		// execvpe(command->name, command->args, environ); // exec+args+path+environ

		/// This shows how to do exec with auto-path resolve
		// add a NULL argument to the end of args, and the name to the beginning
		// as required by exec
		// TODO: do your own exec with path resolving using execv()
		// do so by replacing the execvp call below
		//PART1
		// Path resolution
		        char *path_var = getenv(PATH_ENV_VAR);
		        if (path_var) {
		            char *path = strtok(path_var, ":");
		            char exec_path[1024];
		            while (path != NULL) {
		                snprintf(exec_path, sizeof(exec_path), "%s/%s", path, command->name);
		                execv(exec_path, command->args);
		                path = strtok(NULL, ":");
		            }
		        }

		        // If no valid path is found, print an error message
		                printf("-%s: %s: command not found\n", sysname, command->name);
		                exit(EXIT_FAILURE);
		//execvp(command->name, command->args); // exec+args+path
		exit(0);



	} else {
		// TODO: implement background processes here
		// Background process implementation
		        if (!command->background) {
		            wait(0); // wait for child process to finish
		        }
		return SUCCESS;
	}

	// TODO: your implementation here

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}
