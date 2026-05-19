fifo c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "../../include/protocol.h"
#include "../../include/ipc.h"

//ipc_open_fifo_read
//Creates and opens the FIFO for reading safely.
// Uses a keep-alive writer to prevent read() from returning 0

int ipc_open_fifo_read(int my_id, int *keepalive_fd){
	char fifo_path[256];
	snprintf(fifo_path, sizeof(fifo_path), "%s%d", FIFO_PATH_PREFIX, my_id);
	
	// create the FIFO. If it exists already (EEXIST), proceed normally
	if(mkfifo(fifo_path, 0666)==-1 && errno != EEXIST){
		perror("Error creating FIFO");
		return IPC_ERROR;
	}
	
	// Open the FIFO for reading in non-blocking mode to prevent the process from hanging indefinitely if no writer is currently attached.
	
	int fd_in = open(fifo_path, O_RDONLY | O_NONBLOCK);
	if(fd_in<0){
		perror("Error opening FIFO for read");
		return IPC_ERROR;
	}
	
	// Open a persistent file descriptor in write mode to keep the pipe open.
	// this will make the read() call wont block and wait for new messages
	
	if (keepalive_fd != NULL){
		*keepalive_fd = open(fifo_path, O_WRONLY);
	}
	
	// REmove the O_NONBLOCK flag so that read() works normally (blocking wait)
	int flags = fcntl (fd_in, F_GETFL);
	fcntl(fd_in, F_SETFL, flags & ~O_NONBLOCK);
	
	return fd_in;
	
	
}




message c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "../../include/protocol.h"
#include "../../include/ipc.h"

//ipc_recv_message
//reads a raw string from the FIFO and parses it into the domo_message struct
// format: SENDER_ID|COMMAND|TARGET_ID|PAYLOAD

int ipc_recv_message(int fd_in, domo_message *msg){
	if (fd_in <0 || msg == NULL) return IPC_ERROR;
	
	char buffer[MAX_MSG_LEN];
	memset(buffer, 0, sizeof(buffer));
	
	// REad the incoming string from the FIFO
	ssize_t bytes_read = read(fd_in, buffer, sizeof(buffer)-1);
	if (bytes_read <=0){
		return IPC_ERROR; //error or empty read
	}
	
	
	//ensure null-termination and remove the trailing newline character
	buffer[bytes_read] = '\0';
	buffer[strcspn(buffer,"\n")] = '\0';
	
	//Parse the message using strtok_r (thread safe version of strtok)
	char *saveptr;
	char *sender = strtok_r(buffer, MSG_DELIMITER, &saveptr);
	char *cmd = strtok_r(NULL, MSG_DELIMITER, &saveptr);
	char *target = strtok_r(NULL, MSG_DELIMITER, &saveptr);
	char *payload = strtok_r(NULL, MSG_DELIMITER, &saveptr); //Payload can be NULL
	
	// validate that the mandatory fields are present
	if(!sender || !cmd || !target){
	return IPC_ERROR;
	}
	
	// Populate the domo_message structure safely
	strncpy(msg->sender_id, sender, sizeof(msg->sender_id) -1);
	msg->sender_id[sizeof(msg->sender_id)-1 ] = '\0';
	
	strncpy(msg->command, cmd, sizeof(msg->command) -1);
	msg->command[sizeof(msg->command)-1]= '\0';
	
	msg->target_id = atoi(target);
	
	//If there is a paylaod, copy it; if not, leave it empty
	if (payload != NULL){
		strncpy (msg->payload, payload, sizeof(msg->payload)-1);
		msg->payload[sizeof(msg->payload)-1] = '\0';
	}else {
		msg->payload[0] = '\0';
	}
	
	return OK;
}

//ipc_send_message
//Serializes the domo_message struct into a string and sends it.
//Uses O_NONBLOCK to prevent the process from hanging if the target device crashed and is not reading FIFO.

int ipc_send_message(const domo_message *msg){
	if(msg == NULL) return IPC_ERROR;
	
	
	char fifo_path[256];
	snprintf(fifo_path, sizeof(fifo_path), "%s%d", FIFO_PATH_PREFIX, msg->target_id);
	
	//Open the target FIFO for writing
	//If the reader (target process) does not exist, open() will fail with ENXIO insted of blocking our process forever.
	int fd_out = open(fifo_path, O_WRONLY | O_NONBLOCK);
	if(fd_out< 0){
		if(errno == ENXIO){
			//No reader on other side
			return DEVICE_NOT_FOUND;
		}
		return IPC_ERROR;
	}
	
	//format the message into the required protocol string
	char buffer[MAX_MSG_LEN];
	int len = snprintf(buffer, sizeof(buffer), "%s|%s|%d|%s\n", msg->sender_id, msg->command, msg->target_id, msg->payload);
	
	// check if string is interrupted in the middle
	if(len>=(int)sizeof(buffer)){
		close(fd_out);
		return IPC_ERROR;
	}
	
	//write the formatted string to the FIFO
	ssize_t written = write(fd_out, buffer, len);
	close(fd_out); //Always cose the write end when done
	
	if(written != len){
	return IPC_ERROR;
	}
	
	return OK;
	
}

ipc common

#include <stdio.h>
#include <string.h>

#include "../../include/protocol.h"
#include "../../include/ipc.h"

//ipc_print_message
//Utility function to print a domo_message to the stdout. useful for debugging

void ipc_print_message(const domo_message *msg){
	if (msg==NULL){
		printf("[IPC DEBUG] Can't print: message is NULL.\n");
		return;
	}
	
	printf("[IPC DEBUG] Sender: %s | Cmd: %s | Target: %d | Payload: %s\n", msg->sender_id, msg->command, msg->target_id, (msg->payload[0] != '\0') ? msg->payload : "NULL");
}


//ipc_create_message
//Helper function to quickly populate a domo_message struct safely. Saves otehr modules from having to write multiple strncpy lines manually.

void ipc_create_message(domo_message *msg, const char *sender, const char *cmd, int target, const char *payload){
	if (msg == NULL) return;
	
	strncpy(msg->sender_id, sender, sizeof(msg->sender_id) -1);
	msg->sender_id[sizeof(msg->sender_id) -1]='\0';
	
	strncpy(msg->command, cmd, sizeof(msg->command) -1);
	msg->command[sizeof(msg->command) -1]='\0';
	
	msg->target_id = target;
	
	if (payload != NULL){
		strncpy(msg->payload, payload, sizeof(msg->payload) -1);
			msg->payload[sizeof(msg->payload) -1]='\0';
	}else{
		msg->payload[0] = '\0';
	}
	
	
}

request reply

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

#include "../../include/protocol.h"
#include "../../include/ipc.h"

//ipc_send_request_and_wait
//sends a request message and waits for a reply on the provided fd_in.
// Uses select() to implement a timeout, preventing the process from blocking if the target device is unresponsive or dead.

int ipc_send_request_and_wait(const domo_message *request, domo_message *response, int fd_in){
	
	if(request == NULL || response == NULL || fd_in <0){
		return IPC_ERROR;
	}
	
	// Send the request message
	int send_status = ipc_send_message(request);
	if(send_status != OK){
		return send_status;		//return DEVICE_NOT_FOUND or IPC_ERROR
	}
	
	// Prepare the file descriptor set for select()
	fd_set read_fds;
	FD_ZERO(&read_fds);
	FD_SET(fd_in, &read_fds);
	
	
	// Set the timeout duration
	struct timeval tv;
	tv.tv_sec = TIMEOUT_DEVICE;
	tv.tv_usec = 0;
	
	//Wait for incoming data or timeout
	//select() return -1 or error, 0 on timeout, >0 if data is ready
	int retval = select(fd_in +1, &read_fds, NULL, NULL, &tv);
	
	if (retval == -1){
		perror("Error in select() during request-reply");
		return IPC_ERROR;
	} else if(retval == 0){
		//timeout exired, no response received
		printf("[IPC] Error: Timeout exired waiting for device %d to reply. \n", request->target_id);
		return IPC_ERROR;
	}
	
	// Data is ready to be read from the FIFO
	return ipc_recv_message(fd_in, response);
	
	
}

routing c

#include <stdio.h>
#include <string.h>

#include "../../include/protocol.h"
#include "../../include/routing.h"

//Global routing table
//declared here but accessed by hierarchy.c via 'extern'
routing_node routing_table[MAX_DEVICES];

//routing_init
//initializes the routing table with all the slots set to empty (-1)

void routing_init(void){
	for (int i=0; i<MAX_DEVICES; i++){
		routing_table[i].id = -1; // -1 =empty slot
		routing_table[i].parent_id = -1;
		routing_table[i].type = DEV_UNKNOWN;
	}
}

//routing_add_node
//Register a new device in the system with its parent as default setted to 0.

int routing_add_node(int id, device_type type){
	if (id<0)return IPC_ERROR;
	
	for (int i = 0; i< MAX_DEVICES; i++){
		if (routing_table[i].id == -1){
			routing_table[i].id = id;
			routing_table[i].parent_id = CONTROLLER_ID; // Always defaults to 0
			routing_table[i].type = type;
			return OK;
		}
	}
	return IPC_ERROR; // Routing table is full
}

//routing_remove_node
//removes a device from the routing table (cascading deletion of children is handled at the IPC/CONTROLLER level.

int routing_remove_node(int id){
	int found = 0;
	
	for (int i =0; i< MAX_DEVICES; i++){
		if (routing_table[i].id == id){
			routing_table[i].id = -1;	//Mark slots as deleted
			routing_table[i].parent_id = -1;
			routing_table[i].type = DEV_UNKNOWN;
			found = 1;
			break;
		}
	}
	
	return found ? OK : DEVICE_NOT_FOUND;
}




hierarchy

#include <stdio.h>
#include <string.h>

#include "../../include/protocol.h"
#include "../../include/routing.h"

// Access the global table defined and initialized in routing.cex
extern routing_node routing_table[MAX_DEVICES];

//is_control_device
//VAlidates if a device type is allowed to have children

bool is_control_device(devcice_type type){
	return (type == DEV_CONTROLLER || type == DEV_HUB || type == DEV_TIMER);
}

//get_node_index (Private Helper)
//finds the array index of a device by its logical ID.

static int get_node_index(int id){
	for (int i=0; i<MAX_DEVICES; i++){
		if (routing_table[i].id == id){
			return i;
		}
	}
	return -1; // DEvice not found in the array
}

//routing_link_devices
//Tries to logically link child_id to parent_id.
//Enforces tree structure by checking for DEVICE_TYPE_MISMATCH and CYCLE_DETECTED (preventing infinite loops)

int routing_link_devices(int child_id, int parent_id){
	// you can't link a device to itself
	if (child_id == parent_id){
		return LINK_FAILED;
	}
	
	int child_idx = get_node_index(child_id);
	int parent_idx = get_node_index(parent_id);
	
	// Ensure both devices actually exist in the registry
	if (child_idx == -1 || parent_idx == -1){
		return DEVICE_NOT_FOUND;
	}
	
	// Validate Parent type (only Controller, Hub and Timer can be parents)
	if (!is_control_device(routing_table[parent_idx].type)){
		return DEVICE_TYPE_MISMATCH;
	}
	
	// Cycle Detection Algorithm
	// We traverse upwards from the PROPOSED NEW parent up to the Controller
	// If we encounter the child_id during this operation, it means that the new parent is a descendant of the child.
	// linking them would create an infinite loop, making the IPC crash
	
	int current_ancestor_id = parent_id;
	
	while (current_ancestor_id != CONTROLLER_ID){
		
		if(current_ancestor_id == child_id) {
			return CYCLE_DETECTED;
		}
		
		int ancestor_idx = get_node_index(current_ancestor_id);
		if (ancestor_idx == -1){
			break; //safety fallback: broken chain
		}
		
		// move up one level in the logical tree
		current_ancestor_id = routing_table[ancestor_idx].parent_id;
		
	}
	
	// All checks passed. Apply the logical link safely.
	routing_table[child_idx].parent_id = parent_id;
	return OK;

}



serialization

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/protocol.h"
#include "../../include/ipc.h"

// serialize_message
// converts a domo_message struct into a formatted protocol string.
// format: SENDER_ID|COMMAND|TARGET_ID|PAYLOAD
int serialize_message(const domo_message *msg, char *buffer, size_t max_len){
	if (msg == NULL || buffer == NULL) return IPC_ERROR;
	
	// Format the message into the required protocol string
	int len = snprintf(buffer, max_len), "%s|%s|%d|%s\n", msg->sender_id, msg->command, msg->target_id, msg->payload);
	
	// check if string is interrupted in the middle
	if(len<0 || (size_t)len >=max_len){
		return IPC_ERROR;
	}
	
	return OK;
}

// deserialize_message
//Parses a raw string buffer into a domo_message struct safely.

int deserialize_message(char *buffer, domo_message *msg){
	if (buffer == NULL || msg == NULL) return IPC_ERROR;
	
	//ensure no trailing newline interferes with parsing
	buffer[strcspn(buffer, "\n")] = '\0';
	
	// Parse the message using strtok_r (thread-safe version of strtok
	char *saveptr;
	char *sender = strtok_r(buffer, MSG_DELIMITER, &saveptr);
	char *cmd = strtok_r(NULL, MSG_DELIMITER, &saveptr);
	char *target = strtok_r(NULL, MSG_DELIMITER, &saveptr);
	char *payload = strtok_r(NULL, MSG_DELIMITER, &saveptr); //Payload can be NULL
	
	// validate that the mandatory fields are present
	if(!sender || !cmd || !target){
	return IPC_ERROR;
	}
	
	// Populate the domo_message structure safely
	strncpy(msg->sender_id, sender, sizeof(msg->sender_id) -1);
	msg->sender_id[sizeof(msg->sender_id)-1 ] = '\0';
	
	strncpy(msg->command, cmd, sizeof(msg->command) -1);
	msg->command[sizeof(msg->command)-1]= '\0';
	
	msg->target_id = atoi(target);
	
	//If there is a paylaod, copy it; if not, leave it empty
	if (payload != NULL){
		strncpy (msg->payload, payload, sizeof(msg->payload)-1);
		msg->payload[sizeof(msg->payload)-1] = '\0';
	}else {
		msg->payload[0] = '\0';
	}
	
	return OK;
}








