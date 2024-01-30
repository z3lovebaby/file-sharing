#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <dirent.h>
#include <map>
#include <string>
#include "utils.h"
#include "commands.h"
#include <cerrno>
#include <time.h>
#include <vector>
#include <iostream>
using namespace std;
std::map<std::string, int> expected_argc;
//to check the correctness of commands from stdin

std::map<std::string, cl_filestat> localfiles; // local files
std::map<int, int> sv_to_cl; // maparea fd_server -> fd_client
std::map<int, std::string> openfiles; // the files being transferred

char current_user[MAX_STR_LENGTH] = "none"; // the current user
char prompt[MAX_STR_LENGTH] = "$ "; // the current prompt
char path[4 * MAX_STR_LENGTH]; // cwd
int out = 1;
int pid; // client process id

bool safe_to_quit = false;

FILE *log_file;


/**
 * Actualizeaza prompt-ul.
 */
void update_prompt(char *user) {
    if (!strcmp(user, "none"))
        strcpy(prompt, "$ ");
    else {
        strcpy(prompt, user);
        strcat(prompt, "> ");
    }
}
void log_message(const char *message, const char *prompt) {
    time_t t;
    struct tm *tm_info;

    time(&t);
    tm_info = localtime(&t);

    char time_str[30];  // Adjust the size based on your formatting needs
    strftime(time_str, sizeof(time_str), "[%Y-%m-%d %H:%M:%S] ", tm_info);

    fprintf(log_file, "%s:%s%s\n%s",current_user, time_str, message, prompt);
    fflush(log_file);
}
/**
 * Initializations that include local files, pid, path
 */
void processDirectory(const std::string& directory_path) {
    DIR *dir;
    dirent *ent;
    char path_buffer[BUFLEN];

    dir = opendir(directory_path.c_str());
    if (dir != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            // Ignore "." and ".."
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
                continue;
            }

            strcpy(path_buffer, directory_path.c_str());
            strcat(path_buffer, "/");
            strcat(path_buffer, ent->d_name);

            // If the entry is a directory, process it recursively
            if (ent->d_type == DT_DIR) {
                processDirectory(path_buffer);
            } else if (ent->d_type == DT_REG) {
                // Only add to localfiles if it's a regular file
                cl_filestat newfile;
                newfile.fd = -1;
                newfile.is_open = false;
                strcpy(newfile.full_path, path_buffer);
                localfiles[std::string(ent->d_name)] = newfile;
            }
        }

        closedir(dir);
    } else {
        std::cerr << "Error opening directory: " << directory_path << "\n";
    }
}
void getAllFilesInDirectory(const std::string& folder_path, std::vector<std::string>& file_list) {
    DIR *dir;
    struct dirent *ent;

    if ((dir = opendir(folder_path.c_str())) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_REG) {
                // Regular file, add its path to the list
                file_list.push_back(folder_path + "/" + ent->d_name);
            } else if (ent->d_type == DT_DIR && strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                // Directory (excluding "." and ".."), recurse into it
                getAllFilesInDirectory(folder_path + "/" + ent->d_name, file_list);
            }
        }
        closedir(dir);
    } else {
        std::cerr << "Error opening directory: " << folder_path << std::endl;
    }
}
std::string getFNameFromPath(const std::string& pathh) {
    size_t pos = pathh.find_last_of('/');
    if (pos != std::string::npos && pos + 1 < pathh.length()) {
        return pathh.substr(pos + 1);
    }
    return pathh;
}
void cl_init() {
    expected_argc[std::string("signup")] = 3;
    expected_argc[std::string("login")] = 2;
    expected_argc[std::string("logout")] = 0;
    expected_argc[std::string("getuserlist")] = 0;
    expected_argc[std::string("getfilelist")] = 1;
    expected_argc[std::string("upload")] = 2;
    expected_argc[std::string("rename")] = 2;
    expected_argc[std::string("move")] = 2;
    expected_argc[std::string("copy")] = 2;
    expected_argc[std::string("uploadFolder")] = 2;
    expected_argc[std::string("downloadFolder")] = 2;
    expected_argc[std::string("download")] = 3;
    expected_argc[std::string("share")] = 1;
    expected_argc[std::string("unshare")] = 1;
    expected_argc[std::string("delete")] = 1;
    expected_argc[std::string("quit")] = 0;

    // DIR *dir;
    // dirent *ent;
    // char path_buffer[BUFLEN];

    getcwd(path, sizeof(path));

    // dir = opendir(path);
    // while ((ent = readdir(dir)) != NULL) {
    //     strcpy(path_buffer, path);
    //     strcat(path_buffer, "/");
    //     strcat(path_buffer, ent->d_name);

    //     cl_filestat newfile;

    //     newfile.fd = -1;
    //     newfile.is_open = false;
    //     strcpy(newfile.full_path, path_buffer);

    //     localfiles[std::string(ent->d_name)] = newfile;

    // }
    processDirectory(path);
    // std::cout << "Full paths of files in the current directory and its subdirectories:\n";
    // for (const auto& file : localfiles) {
    //     std::cout << file.second.full_path << "\n";
    // }
    pid = getpid();

    char log_buf[BUFLEN];
    sprintf(log_buf, "client-%d.log", pid);

    log_file = fopen(log_buf, "w");

    fflush(stdout);
    fprintf(log_file, "%s", prompt);
    fflush(log_file);
}


/**
 * 
The number of files in transfer (for quit).
 */
int files_in_transfer() {
    int ret = 0;
    std::map<std::string, cl_filestat>::iterator it;
    for (it = localfiles.begin(); it != localfiles.end(); it++)
        if (it->second.is_open)
            ret++;
    return ret;
}


int main(int argc, char *argv[]) {

    if (argc < 3) {
        error("Usage: ./client <IP_server> <port_server>\n");
    }

    cl_init();

    int sockfd; // the socket on which I communicate with the server
    uint16_t fdmax, i;
    int byte_count;

    sockaddr_in serv_addr;

    fd_set read_fds;
    fd_set temp_fds;

    FD_ZERO(&read_fds);
    FD_ZERO(&temp_fds);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
        {error("ERROR connecting");
        exit(EXIT_FAILURE);}
    FD_SET(sockfd, &read_fds);
    FD_SET(0, &read_fds);
    fdmax = sockfd;
    // cout<<"SOCK: "<<fdmax<<endl;
    printf("Connecting successful.\n");
    printf("Would you like to see instructions for using the system? <yes or no>: ");

    char choice[10];
    scanf("%[^\n]%*c",choice);
    if(strcasecmp(choice,"yes")==0){
        printf("Guide:\n");
        printf("1.Signup: signup <username> <pw> <confirmPw>\n");
        printf("2.Login: login <username> <pw>\n");
        printf("3.Logout: logout\n");
        printf("4.Upload a file: upload <filepath> <destination>\n");
        printf("5.Download a file: download <username> <filepath> <destination>\n");
        printf("6.Upload a folder: uploadFolder <folder> <destination>\n");
        printf("7.Download a folder: downloadFolder <user> <folder>\n");
        printf("8.Get list user in system: getuserlist\n");
        printf("9.Get list file of user: getfilelist <user>\n");
        printf("10.Set status file share/unshare: share/unshare <filepath>\n");
        printf("11.Coppy a file: copy <src> <destination>\n");
        printf("12.Change directory of file: move <src> <destination>\n");
        printf("13.Rename file: rename <filepath> <newname>\n");
        printf("14.Delete a file: delete <filepath>\n");
        printf("Enter to continue...\n");
    }
    else{
        printf("Ok, wellcome you comback my app.\n$ ");
    }
    fflush(stdout);
    unsigned char buffer[BUFLEN];
    command cmd_from_input;
    
    while (1) {
        temp_fds = read_fds;
        if (select(fdmax + 1, &temp_fds, NULL, NULL, NULL) == -1)
            error("ERROR in select");

        if (safe_to_quit && files_in_transfer() == 0) {
            close(sockfd);
            break;
        }
        // looking for data to read
        for (i = 0; i <= fdmax; ++i) {
            // cout<<"i: "<<i<<endl;
            if (FD_ISSET(i, &temp_fds)) {
                if (i == 0) { // data from stdin
                    // should be a command
                    memset(buffer, 0, BUFLEN);
                    fgets((char*)buffer, BUFLEN - 1, stdin);

                    buffer[strlen((char*)buffer) - 1] = '\0';

                    // printf("am citit: `%s`\n", buffer);

                    fprintf(log_file, "%s\n%s", buffer, prompt);

                    cmd_from_input = create_cmd(buffer, current_user);

                    if (!strcmp(cmd_from_input.name, "WHITESPACE")) {
                            printf("%s", prompt);
                            fflush(stdout);
                            fprintf(log_file, "%s", prompt);
                            fflush(log_file);
                        continue;
                    } else if (!strcmp(cmd_from_input.name, "ERROR")) {
                        printf("Command doesnt exist\n%s", prompt);
                        fflush(stdout);
                        log_message("Command doesnt exist\n", prompt);
                        continue;
                    }else if(!strcmp(cmd_from_input.name, "signup") && strcmp(current_user, "none")){
                        printf("-1.1 You logined...Let logout if want create new account.\n%s", prompt);
                        fflush(stdout);
                        // fprintf(log_file, "-1 The client is not authenticated\n%s", prompt);
                        // fflush(log_file);
                        log_message("-1.1 You logined...Let logout if want create new account.", prompt);
                        continue;
                    }else if (!strcmp(cmd_from_input.name, "signup")&&cmd_from_input.argc < expected_argc[std::string(cmd_from_input.name)]) {
                        printf("Too few argument\n%s", prompt);
                        fflush(stdout);
                        log_message("Too few argument", prompt);
                        continue;
                    }else if (!strcmp(cmd_from_input.name, "signup")&&cmd_from_input.argc > expected_argc[std::string(cmd_from_input.name)]) {
                        printf("Too many arguments\n%s", prompt);
                        fflush(stdout);
                        log_message("Too many arguments", prompt);
                        continue;
                    }else if (strcmp(cmd_from_input.name, "signup")&&strcmp(cmd_from_input.name, "login") && !strcmp(current_user, "none")) {
                        printf("-1 The client is not authenticated\n%s", prompt);
                        fflush(stdout);
                        // fprintf(log_file, "-1 The client is not authenticated\n%s", prompt);
                        // fflush(log_file);
                        log_message("-1 The client is not authenticated", prompt);
                        continue;
                    }else if (cmd_from_input.argc < expected_argc[std::string(cmd_from_input.name)]) {
                        printf("Too few argument\n%s", prompt);
                        fflush(stdout);
                        log_message("Too few argument", prompt);
                        continue;
                    }else if (cmd_from_input.argc > expected_argc[std::string(cmd_from_input.name)]) {
                        printf("Check: %d %d %s",cmd_from_input.argc,expected_argc[std::string(cmd_from_input.name)],cmd_from_input.name);
                        printf("Too many arguments\n%s", prompt);
                        fflush(stdout);
                        log_message("Too many arguments", prompt);
                        continue;
                    }else if (!strcmp(cmd_from_input.name, "login") && strcmp(current_user, "none")) {
                        printf("-2 Session already open\n%s", prompt);
                        fflush(stdout);
                        log_message("-2 Session already open", prompt);
                        continue;
                    }else if (!strcmp(cmd_from_input.name, "logout") && !strcmp(current_user, "none")) {
                        printf("-1 The client is not authenticated\n%s", prompt);
                        fflush(stdout);
                        log_message("-1 The client is not authenticated", prompt);
                        continue;
                    }else if (!strcmp(cmd_from_input.name, "upload") && localfiles.find(getFNameFromPath(std::string(cmd_from_input.args[0]))) == localfiles.end()) {
                        printf("-4 File not exist\n%s", prompt);
                        fflush(stdout);
                        // fprintf(log_file, "-4 File not exist\n%s", prompt);
                        // fflush(log_file);
                        log_message("-4 File not exist", prompt);
                        continue;
                    }// TODO: logging and pretty prompt

                    // creez mesajul
                    message msg_to_send;
                    msg_to_send.type = CMD_CLIENT;
                    msg_to_send.length = strlen((char*)buffer) + 1;
                    // printf("msg_to_send.length = %d\n", msg_to_send.length);
                    memcpy(msg_to_send.payload, buffer, msg_to_send.length);
                    byte_count = mySend(sockfd, msg_to_send);
                    if (byte_count < 0)
                        error("ERROR writing to socket");

                } else if (i == sockfd) { // something from the server
                    message msg_to_receive;
                    if ((byte_count = myRecv(sockfd, &msg_to_receive)) <= 0) {
                        if (byte_count == 0) { // server hang up
                            printf("client: server(%d) hang up", i);
                        } else
                            error("ERROR in recv");
                        close(i);
                        FD_CLR(i, &read_fds);
                    } else {
                        if (msg_to_receive.type == CMD_ERROR) { // error
                            char errnor[MAX_STR_LENGTH];
                            sscanf((char*)msg_to_receive.payload, "%s", errnor);

                            printf("%s\n%s", msg_to_receive.payload, prompt);
                            fflush(stdout);
                            // fprintf(log_file, "%s\n%s", msg_to_receive.payload, prompt);
                            // fflush(log_file);
                            log_message(reinterpret_cast<const char*>(msg_to_receive.payload), prompt);
                            if (!strcmp(errnor, "-8")) // BF
                                exit(-8);
                        } else if (msg_to_receive.type == CMD_SUCCESS) {
                            for (int j = 0; j < msg_to_receive.length; ++j)
                                buffer[j] = msg_to_receive.payload[j];

                            command cmd = create_cmd(buffer, current_user);
                            if(!strcmp(cmd.name, "signup")){
                                printf("Signup successfull !\nLet signup to use my app.\n");
                                printf("%s", prompt);
                                fflush(stdout);
                                log_message("One account was created!", "");
                            } else if (!strcmp(cmd.name, "login")) {
                                strcpy(current_user, cmd.args[0]);
                                update_prompt(current_user);
                                printf("Login successfull !\nEnter to continue...");
                                fflush(stdout);
                                log_message("Login successfull !", "");

                            } else if (!strcmp(cmd.name, "logout")) {
                                strcpy(current_user, "none");
                                update_prompt(current_user);
                                printf("%s - Logout successfull !\n",current_user);
                                printf("%s", prompt);
                                fflush(stdout);
                                log_message("Logout successfull !", "");
                            } else if (!strcmp(cmd.name, "quit")) {
                                printf("Quitting...\n%s", prompt);
                                fprintf(log_file, "Quitting...\n%s", prompt);
                                fflush(log_file);
                                fflush(stdout);
                                safe_to_quit = true;

                            } else {
                                printf("\n%s\n%s", msg_to_receive.payload, prompt);
                                fflush(stdout);
                                // log_message("\nResult: ", "");
                                // fprintf(log_file, "\n%s\n%s", buffer, prompt);
                                // fflush(log_file);
                            }
                        } else if (msg_to_receive.type == INIT_UPLOAD) {
                            int sv_fd;
                            char filename[MAX_STR_LENGTH];
                            sscanf((char*)msg_to_receive.payload, "%d %s", &sv_fd, filename);

                            int cl_fd = open(localfiles[std::string(filename)].full_path, O_RDONLY);
                            if (cl_fd < 0)
                                perror("ERROR on open");
                            sv_to_cl[sv_fd] = cl_fd;
                            openfiles[cl_fd] = std::string(filename);
                            localfiles[std::string(filename)].is_open = true;
                            localfiles[std::string(filename)].fd = cl_fd;

                            FD_SET(cl_fd, &read_fds);
                            if (cl_fd > fdmax)
                                fdmax = cl_fd;

                            message msg_to_send;
                            msg_to_send.type = INIT_UPLOAD2;
                            sprintf((char*)msg_to_send.payload, "%d %d", cl_fd, sv_fd);
                            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

                            int byte_count = mySend(sockfd, msg_to_send);
                            if (byte_count < 0)
                                perror("init_upload2: ERROR on send");

                        } else if (msg_to_receive.type == INIT_FOLDER_UPLOAD) {
                            int sv_fd;
                            char folder_path[MAX_STR_LENGTH];
                            char des_path[MAX_STR_LENGTH];
                            sscanf((char *)msg_to_receive.payload, "%d %s %s", &sv_fd, folder_path, des_path);
                            cout<<"aaaa: "<<msg_to_receive.payload<<endl;
                            // Step 1: Get the list of files in the folder
                            std::vector<std::string> file_list;

                            getAllFilesInDirectory(folder_path, file_list);

                            std::cout << "Danh sách file trong vector:\n";
                            for (const auto& file : file_list) {
                                std::cout << file << "\n";
                            }
                            for (const auto &filename : file_list)
                            {
                                std::string full_path = filename;
                                // full_path += "/";
                                // full_path += filename;
                                char buffer1[BUFLEN];
                                int bytes_written = snprintf(buffer1, BUFLEN, "upload %s %s", full_path.c_str(), des_path);
                                // cout<<"cmd: "<<buffer1<<endl;
                                if (bytes_written >= 0 && bytes_written < BUFLEN){
                                    message msg_to_send;
                                    msg_to_send.type = CMD_CLIENT;
                                    msg_to_send.length = strlen((char *)buffer1) + 1;
                                    // printf("msg_to_send.length = %d\n", msg_to_send.length);
                                    memcpy(msg_to_send.payload, buffer1, msg_to_send.length);
                                    // printmsg(msg_to_send);
                                    // trimit comanda serverului
                                    byte_count = mySend(sockfd, msg_to_send);
                                    if (byte_count < 0)
                                        error("ERROR writing to socket");
                                }else{
                                    cout<<"Error formatting the upload\n";
                                }
                            }
                        } else if (msg_to_receive.type == INIT_FOLDER_DOWNLOAD) {
                            int sv_fd;
                            char requser_str[MAX_STR_LENGTH];
                            char pathListFile[BUFLEN];
                            char folderName[MAX_STR_LENGTH];
                            sscanf((char *)msg_to_receive.payload, "%d %s %s %s", &sv_fd, requser_str, pathListFile,folderName);
                            // Step 1: Get the list of files in the folder
                            std::vector<std::string> received_file_paths;

                            char* token = strtok((char*)pathListFile, ";");
                            while (token != NULL) {
                                received_file_paths.push_back(token);
                                token = strtok(NULL, ";");
                            }
                            for (const auto &filename : received_file_paths)
                            {
                                string remaining_path = "";
                                size_t user_pos = filename.find((string)requser_str + "/");

                                if (user_pos != std::string::npos) 
                                    remaining_path = filename.substr(user_pos + strlen(requser_str) + 1);
                                // full_path += "/";
                                // full_path += filename;
                                char buffer1[BUFLEN];
                                int bytes_written = snprintf(buffer1, BUFLEN, "download %s %s %s", requser_str,remaining_path.c_str(), folderName);
                                // cout<<"cmd: "<<buffer1<<endl;
                                if (bytes_written >= 0 && bytes_written < BUFLEN){
                                    message msg_to_send;
                                    msg_to_send.type = CMD_CLIENT;
                                    msg_to_send.length = strlen((char *)buffer1) + 1;
                                    // printf("msg_to_send.length = %d\n", msg_to_send.length);
                                    memcpy(msg_to_send.payload, buffer1, msg_to_send.length);
                                    // printmsg(msg_to_send);
                                    // trimit comanda serverului
                                    byte_count = mySend(sockfd, msg_to_send);
                                    if (byte_count < 0)
                                        error("ERROR writing to socket");
                                }else{
                                    cout<<"Error formatting the upload\n";
                                }
                            }
                        } else if (msg_to_receive.type == INIT_DOWNLOAD) {
                            int sv_fd;
                            char filename[MAX_STR_LENGTH];
                            char des[MAX_STR_LENGTH];
                            sscanf((char*)msg_to_receive.payload, "%d %s %s", &sv_fd, filename,des);

                            char path_buffer[BUFLEN], pid_buf[MAX_STR_LENGTH];
                            sprintf(pid_buf, "%d_%s", pid, filename);
                            strcpy(path_buffer, path);
                            strcat(path_buffer, "/");
                            if(!strcmp(des,"@")){
                                strcat(path_buffer, pid_buf);
                            }
                            else{
                                std::string current_path = "";
                                size_t last_slash = 0;

                                while ((last_slash = std::string(path_buffer).find_first_of("/", last_slash + 1)) != std::string::npos)
                                {
                                    current_path = std::string(path_buffer).substr(0, last_slash);
                                    mkdir(current_path.c_str(), 0777);
                                }

                                // Tạo thư mục cuối cùng (bbb trong trường hợp này)
                                mkdir(des, 0777);
                                strcat(path_buffer, des);
                                strcat(path_buffer, "/");
                                strcat(path_buffer, pid_buf);
                            }

                            int cl_fd = open(path_buffer, O_WRONLY | O_CREAT);
                            if (cl_fd < 0)
                                perror("init_download: ERROR in open");

                            sv_to_cl[sv_fd] = cl_fd;
                            openfiles[cl_fd] = std::string(pid_buf);
                            localfiles[std::string(pid_buf)].is_open = true;
                            localfiles[std::string(pid_buf)].fd = cl_fd;
                            strcpy(localfiles[std::string(pid_buf)].full_path, path_buffer);

                            message msg_to_send;
                            msg_to_send.type = INIT_DOWNLOAD2;
                            sprintf((char*)msg_to_send.payload, "%d %d", cl_fd, sv_fd);
                            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

                            if (mySend(sockfd, msg_to_send) < 0)
                                perror("init_download2: ERROR on send");

                        } else if (msg_to_receive.type == FILE_TRANSFER) {
                            uint16_t cl_fd, sv_fd;
                            memcpy(&sv_fd, msg_to_receive.payload, 2);
                            cl_fd = sv_to_cl[sv_fd];

                            write(cl_fd, msg_to_receive.payload + 2, msg_to_receive.length - 2);
                            printf("\nTransfer successfull..\n");
                            fflush(stdout);

                        } else if (msg_to_receive.type == CLOSE_TRANSFER) {
                            uint16_t cl_fd, sv_fd;
                            memcpy(&sv_fd, msg_to_receive.payload, 2);
                            cl_fd = sv_to_cl[sv_fd];

                            close(cl_fd);
                            localfiles[openfiles[cl_fd]].is_open = false;
                            localfiles[openfiles[cl_fd]].fd = -1;

                            struct stat st = {0};
                            stat(localfiles[openfiles[cl_fd]].full_path, &st);

                            printf("Download finished: %s -- %ld bytes\n%s", openfiles[cl_fd].c_str(), st.st_size, prompt);
                            fflush(stdout);
                            fprintf(log_file, "Download finished: %s -- %ld bytes\n%s", openfiles[cl_fd].c_str(), st.st_size, prompt);
                            fflush(log_file);
                        }

                    }

                } else { // 
                    int byte_count = read(i, buffer, 4090);

                    if (byte_count < 0)
                        perror("Read from file: ERROR on read");

                    if (byte_count > 0) { // 
                        message msg_to_send;
                        msg_to_send.type = FILE_TRANSFER;
                        memcpy(msg_to_send.payload, &i, 2);
                        memcpy(msg_to_send.payload + 2, buffer, byte_count);
                        msg_to_send.length = byte_count + 2;

                        if (mySend(sockfd, msg_to_send) < 0)
                            perror("File_transfer: ERROR on send");
                    } else { // 
                        message msg_to_send;
                        msg_to_send.type = CLOSE_TRANSFER;
                        memcpy(msg_to_send.payload, &i, 2);
                        msg_to_send.length = 2;

                        FD_CLR(i, &read_fds);
                        close(i);
                        localfiles[openfiles[i]].is_open = false;
                        localfiles[openfiles[i]].fd = -1;

                        if (mySend(sockfd, msg_to_send) < 0)
                            perror("Close_transfer: ERROR on send");
                    }
                }
            }
        }
    }
    fflush(stdout);
    fprintf(log_file, "The client has logged out");
    fflush(log_file);
    fclose(log_file);
    return 0;
}