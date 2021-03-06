#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <dirent.h>
#include <string>

#include "case.h"
#include "server.h"
#include "client.h" /* Just for the port number... */
#include "cli.h"
#include "constants.h" 

using namespace std;

extern int server_running;

/* Communicate with a client */
void *talker(void *p) {
    int connected = 1;
    int the_socket = *(int *)p;
    char in_buffer[BUFFER_SIZE] = { '\0' };
    char out_buffer[BUFFER_SIZE] = { '\0' };
    int n = 0;
    Case current_case = { NULL, NULL, { '\0' }, 0, the_socket, NULL };

    while(server_running && connected) {
        
        /* Listen for requests */
        n = read(the_socket, in_buffer, BUFFER_SIZE);
        if(!server_running) {
            break;
        }
        in_buffer[n] = '\0';
        if( n > 0) { /* We have recieved a command */
            current_case = parse(current_case, in_buffer, out_buffer);
            current_case.local = 0;
            current_case.socket = the_socket;

            /* Write the command output to the socket */
            write(the_socket, out_buffer, strnlen(out_buffer, BUFFER_SIZE));
            /* Give a new prompt */
            sprintf(out_buffer, "\nremote > ");
            write(the_socket, out_buffer, strnlen(out_buffer, BUFFER_SIZE));

            out_buffer[0] = '\0'; /* We are now done with that string */
        }

    }

    sprintf(out_buffer, "__Exit__");
    write(the_socket, out_buffer, 8);

    pthread_exit(EXIT_SUCCESS);
}

/* Listen on a port for connection requests */
void *listener(void *p) {
    
    char buffer[BUFFER_SIZE] = { '\0' };

    pthread_t thread[MAX_THREADS]; /* Kinda dirty.. Will eventually expire */
    int i = 0;

    int the_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  
    int reuse_on = 1;
    setsockopt(the_socket, SOL_SOCKET, SO_REUSEADDR, &reuse_on, sizeof(reuse_on));

    int the_new_socket = 0;

    struct sockaddr_in client_address;
    struct sockaddr_in server_address;

    server_address.sin_family = AF_INET; /* based on Linuxhowtos.org tutorial */
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(PORT_INT);

    /* Bind the socket to the port */
    if(bind(the_socket, (struct sockaddr *)&server_address, sizeof(server_address))) {
        fprintf(stderr, "Error: Could not bind address.\n");
        server_running = 0;
    }
   
    listen(the_socket, 5);

    socklen_t client_address_length = sizeof(client_address);
    
    while(server_running) {
        the_new_socket = accept(the_socket, (struct sockaddr *)&client_address, &client_address_length);
        if(server_running && the_new_socket != -1) {
            pthread_create( &(thread[i++]), NULL, talker, (void *)&the_new_socket);
        }
        sleep(1);
    }

    close(the_socket);


    pthread_exit(EXIT_SUCCESS);

}


void get_input(Case current_case, char *input) {
    int n = 0;
    if(current_case.local) {
        /* We want to read a line from the local terminal */
        fgets(input, BUFFER_SIZE, stdin);
        input[strlen(input)-1] = '\0';
    }
    else {
        /* We want to read a line from the remote client */
        do {
            n = read(current_case.socket, input, BUFFER_SIZE);
        } while(!n);
        input[n] = '\0';
    }
}

/*
 * Wrapper for get_input using c++ strings
 */
string get_input_string(Case current_case) {
	char buffer[BUFFER_SIZE] = {'\0'};
	
	get_input(current_case, buffer);
	
	return string(buffer);
}

void put_output(Case current_case, const char *output) {
    
    if(current_case.local) {
        /* We want to write a line to the local terminal */
        printf("%s", output);
        fflush(stdout);
    }
    else {
        /*We want to write a line to the remote client */
        write(current_case.socket, output, strnlen(output, BUFFER_SIZE));
    }
}

void put_output_and_log(Case current_case, const char *output, bool timestamp) {
	put_output(current_case, output);
	if (timestamp) {
		log_text(current_case, output);
	} else {
		log_text_without_timestamp(current_case, output);
	}
}

/*
 * Wrapper for put_output for C++ strings
 */
void put_output_string(Case current_case, string output) {
	put_output(current_case, output.c_str());
}
