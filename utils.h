#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <map>
#include <stdint.h>

#define MAX_STR_LENGTH 50 // nume useri, fisiere etc.
#define MAX_CLIENTS 100
#define BUFLEN 4096 // buffere

#define CMD_ERROR 0 // the command generated an error
#define CMD_SUCCESS 1 // command executed successfully
#define CMD_CLIENT 2 // package with the order from the client
#define INIT_UPLOAD 3 // package that initiates the upload (sv -> cl)
#define INIT_UPLOAD2 4 // package that initiates the upload (cl -> sv)
#define INIT_DOWNLOAD 5 // package that initiates the download (sv -> cl)
#define INIT_DOWNLOAD2 6 // package that initiates the download (cl -> sv)
#define FILE_TRANSFER 7 // package with part of a file (sv <-> cl)
#define CLOSE_TRANSFER 8 // package that announces the end of the transfer (sv <-> cl)
#define INIT_FOLDER_UPLOAD 9
#define INIT_FOLDER_DOWNLOAD 10

struct disk_file {
    std::string filename;
    char share_status[MAX_STR_LENGTH]; // "SHARED" / "PRIVATE"
    char full_path[4 * MAX_STR_LENGTH];
};

// structura pentru user
struct user {
    std::string name;
    std::string pass;
    std::map<std::string, disk_file> files; // list of files owned by the user
    bool connected;
    int socket;
};

// structure for messages
struct message {
    uint16_t type; // types are those defined above(0 - 10)
    uint16_t length;
    unsigned char payload[BUFLEN - 4];
};

// structure for clients
struct client {
    std::string connected_user;
    int failed_login_attempts;
};

// the structure for the files in transfer on the server
struct sv_filestat {
    std::string filename;
    int cl_socket;
    char full_path[4 * MAX_STR_LENGTH];
    bool is_in_upload;
    bool is_in_download;
};

// the structure for the files being transferred to the client
struct cl_filestat {
    int fd;
    char full_path[4 * MAX_STR_LENGTH];
    bool is_open;
};

// ----------------------------------------------------------------------------


void error(const char* msg);

int mySend(int socket, message msg);

int myRecv(int socket, message* msg);

void printmsg(message msg);

#endif