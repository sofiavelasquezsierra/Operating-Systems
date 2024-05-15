#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> 
#include <sys/stat.h>
#include "shellmemory.h"
#include "shell.h"
//#include <sys/types.h>

//used to be 3 -> changed to 7 for set
int MAX_ARGS_SIZE = 7;

int badcommand(){
	printf("%s\n", "Unknown Command");
	return 1;
}

// For run command only
int badcommandFileDoesNotExist(){
	printf("%s\n", "Bad command: File not found");
	return 3;
}

int help();
int quit();
int set(char* var, char* value);
int print(char* var);
int run(char* script);
int echo(char* token);
int badcommandFileDoesNotExist();
int my_ls();
int my_mkdir(char* dirname);
int my_touch(char* filename);
int my_cd(char* dirname);
int my_cat(char* filename);

// Interpret commands and their arguments
int interpreter(char* command_args[], int args_size){
	int i;


    // Check if the first argument is empty or contains only whitespace
    if (args_size < 1 || command_args[0] == NULL || command_args[0][0] == '\0' || strspn(command_args[0], " \t\r\n") == strlen(command_args[0])) {
        return badcommand();
    }

	//modify here for set

	if (strcmp(command_args[0], "set")==0) {
		//set
		if (args_size <= 2 || args_size > 7){
			printf("%s\n", "Bad command: set");
			return 1;
		} 	

		char value[1000] = "";
		for (i = 2; i < args_size; i++) {
			strcat(value, command_args[i]);
            if (i < args_size - 1) {
                strcat(value, " "); // Add space between tokens
            }
        }

		return set(command_args[1], value);
	}




	if ( args_size < 1 || args_size > MAX_ARGS_SIZE){
		return badcommand();
	}

	for ( i=0; i<args_size; i++){ //strip spaces new line etc
		command_args[i][strcspn(command_args[i], "\r\n")] = 0;
	}

	if (strcmp(command_args[0], "help")==0){
	    //help
	    if (args_size != 1) return badcommand();
	    return help();
	
	} else if (strcmp(command_args[0], "quit")==0) {
		//quit
		if (args_size != 1) return badcommand();
		return quit();

	} else if (strcmp(command_args[0], "set")==0) {
		//set
		if (args_size != 3) return badcommand();	
		return set(command_args[1], command_args[2]);
	
	} else if (strcmp(command_args[0], "print")==0) {
		if (args_size != 2) return badcommand();
		return print(command_args[1]);
	
	} else if (strcmp(command_args[0], "run")==0) {
		if (args_size != 2) return badcommand();
		return run(command_args[1]);
	
	} else if (strcmp(command_args[0], "echo")==0) {
		if (args_size != 2) return badcommand();
		return echo(command_args[1]);
	
	} else if (strcmp(command_args[0], "my_ls")==0) {
		if (args_size != 1) return badcommand();
		return my_ls();
	
	} else if (strcmp(command_args[0], "my_mkdir")==0) {
		if (args_size != 2) return badcommand();
		return my_mkdir(command_args[1]);
	
	} else if (strcmp(command_args[0], "my_touch") == 0) {
    	if (args_size != 2) return badcommand();
	    return my_touch(command_args[1]);

	} else if (strcmp(command_args[0], "my_cd") == 0) {
    	if (args_size != 2) return badcommand();
	    return my_cd(command_args[1]);
		
	} else if (strcmp(command_args[0], "my_cat") == 0) {
    	if (args_size != 2) return badcommand();
	    return my_cat(command_args[1]);
		
	} else return badcommand();
}

int help(){

	char help_string[] = "COMMAND			DESCRIPTION\n \
help			Displays all the commands\n \
quit			Exits / terminates the shell with “Bye!”\n \
set VAR STRING		Assigns a value to shell memory\n \
print VAR		Displays the STRING assigned to VAR\n \
run SCRIPT.TXT		Executes the file SCRIPT.TXT\n ";
	printf("%s\n", help_string);
	return 0;
}

int quit(){
	printf("%s\n", "Bye!");
	exit(0);
}






int set(char* var, char* value){

	char *tokens[5];
	int i=0;


	//checks if more than 5 tokens and adds all the tokens in array tokens
	char* token = strtok(value, " ");
	while (token != NULL){
		tokens[i++] = token;
		token = strtok(NULL, " ");
	}

	//char *link = "=";

	char buffer[1000];
	//reinitialize the buffer when set command is accepted
	 buffer[0] = '\0';
	 
	//strcpy(buffer, var);
	//strcat(buffer, link);


	for (int j=0; j<i; j++){
		strcat(buffer, tokens[j]);
		if (j<i-1){
			strcat(buffer, " "); //add space between tokens
		}
	}



	mem_set_value(var, buffer);

	return 0;

}

int print(char* var){
	printf("%s\n", mem_get_value(var)); 
	return 0;
}

int run(char* script){
	int errCode = 0;
	char line[1000];
	FILE *p = fopen(script,"rt");  // the program is in a file

	if(p == NULL){
		return badcommandFileDoesNotExist();
	}

	fgets(line,999,p);
	while(1){
		errCode = parseInput(line);	// which calls interpreter()
		memset(line, 0, sizeof(line));

		if(feof(p)){
			break;
		}
		fgets(line,999,p);
	}

    fclose(p);

	return errCode;
}

int echo(char* token) { //Should I add return statements like return 1 when it fails etc ?
	if (token[0] == '$') {
		char* var = token + 1; //to get only the name of the token
		char* value = mem_get_value(var);
		if (value != NULL) {
            printf("%s\n", value);
        } else {
            printf("\n"); 
        }
    } else {
        printf("%s\n", token);
    }
    return 0;
}


int my_ls() {
    // system function as pointed out
    system("ls");
    return 0;
}


int my_mkdir(char* dirname){

    mkdir(dirname, 0777);
}

int my_touch(char* filename) {
	 FILE *file = fopen(filename, "w");
	 fclose(file);
    return 0;
}

int my_cd(char* dirname) {
	if (chdir(dirname) != 0) {
        printf("Bad command: my_cd\n");
        return 1;
    }
}

int my_cat(char* filename) {
	FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Bad command: my_cat\n");
        return 1;
    }

	char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        printf("%s", buffer);
    }

	fclose(file);
    return 0;
}
