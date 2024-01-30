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
#include <iostream>
#include <fstream>
#include <vector>
using namespace std;
int listen_port; // the port on which to listen for connections

std::map<std::string, user> users; // users in the system
std::map<int, client> clients;

char path[4 * MAX_STR_LENGTH]; // the full path of cwd
char ucf_filename[MAX_STR_LENGTH];
char sscf_filename[MAX_STR_LENGTH];
fd_set read_fds;
fd_set temp_fds;


std::map<int, sv_filestat> files_to_transfer; // the files that are or were
// transferate.
std::map<int, std::map<int, int> > cl_to_sv;
// ^ mapping between the file-descriptors for each client and those on the server
// every file opened for transfer in any client will have tabs
// the file descriptor opened here on the server

/**
 * Check if a file is being uploaded (to avoid its download
 * delete it)
 */
bool is_uploading(char *full_path) {
    std::map<int, sv_filestat>::iterator it;

    for (it = files_to_transfer.begin(); it != files_to_transfer.end(); it++) {
        if (!strcmp(it->second.full_path, full_path) && it->second.is_in_upload)
            return true;
    }
    return false;
}

/**
 * Check if a file is being downloaded (to avoid deletion)
 */
bool is_downloading(char *full_path) {
    std::map<int, sv_filestat>::iterator it;

    for (it = files_to_transfer.begin(); it != files_to_transfer.end(); it++) {
        if (!strcmp(it->second.full_path, full_path) && it->second.is_in_download)
            return true;
    }
    return false;
}
bool isFile(const char* path) {
    struct stat fileInfo;
    if (stat(path, &fileInfo) != 0) {
        // Error occurred, handle accordingly
        return false;
    }

    return S_ISREG(fileInfo.st_mode);
}
int isFolder(char* path) {
    struct stat s;
    if (stat(path, &s) == 0) {
        if (S_ISDIR(s.st_mode)) {
            return 1;  // It's a directory
        } else {
            return 0;  // It's not a directory
        }
    } else {
        return 0;  // Error, assume it's not a directory
    }
}
// Hàm lấy tên của thư mục từ đường dẫn
std::string getFolderNameFromPath(const std::string& pathh) {
    size_t pos = pathh.find_last_of('/');
    if (pos != std::string::npos && pos + 1 < pathh.length()) {
        return pathh.substr(pos + 1);
    }
    return pathh;
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
bool directoryExists(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (dir) {
        closedir(dir);
        return true;
    }
    return false;
}
/**
 * Function called in sv_init(), parses the user file and creates it
 * the corresponding entries in users.
 * Creates the user's directory, if it does not exist.
 * Goes through the files in the user's directory and puts them in the file list.
 */
void renameFileOrDirectory(std::string& source_path, const std::string& new_name) {
    // Tìm vị trí của dấu gạch chéo cuối cùng để xác định thư mục cha
    size_t last_slash_pos = source_path.find_last_of("/");

    if (last_slash_pos != std::string::npos) {
        // Trích xuất đường dẫn thư mục cha
        std::string parent_path = source_path.substr(0, last_slash_pos + 1);

        // Trích xuất tên tệp hoặc thư mục
        std::string file_or_directory_name = source_path.substr(last_slash_pos + 1);

        // Tạo đường dẫn mới bằng cách kết hợp thư mục cha và tên mới
        std::string new_path = parent_path + new_name;

        // Hiển thị thông báo để kiểm tra
        // std::cout << "Old Path: " << source_path << std::endl;
        // std::cout << "New Path: " << new_path << std::endl;

        // Thực hiện thay đổi tên bằng cách sử dụng hàm rename hoặc tương tự
        // (Ở đây chúng tôi chỉ hiển thị thông báo vì không sử dụng filesystem)
        std::cout << "Rename operation should be performed here." << std::endl;

        // Thực hiện thay đổi tên tệp tin trên hệ thống tệp tin
        if (rename(source_path.c_str(), new_path.c_str()) != 0) {
            // Handle the error if needed
            std::perror("Error during rename: ");
        } else {
            // Update the source_path to the new path
            source_path = new_path;
        }
    } else {
        // Nếu không có dấu gạch chéo, đây là tên tệp tin trực tiếp
        // Tạo đường dẫn mới bằng cách thay đổi toàn bộ tên tệp tin
        std::string new_path = new_name;

        // Hiển thị thông báo để kiểm tra
        // std::cout << "Old Path: " << source_path << std::endl;
        // std::cout << "New Path: " << new_path << std::endl;

        // Thực hiện thay đổi tên bằng cách sử dụng hàm rename hoặc tương tự
        // (Ở đây chúng tôi chỉ hiển thị thông báo vì không sử dụng filesystem)
        std::cout << "Rename operation should be performed here." << std::endl;

        // Thực hiện thay đổi tên tệp tin trên hệ thống tệp tin
        if (rename(source_path.c_str(), new_path.c_str()) != 0) {
            // Handle the error if needed
            std::perror("Error during rename: ");
        } else {
            // Update the source_path to the new path
            source_path = new_path;
        }
    }
}

void processDirectory(const char *path, const char *buf1, const char *root) {
    DIR *dir;
    struct dirent *ent;
    struct stat st={0};

    char path_buffer[MAX_STR_LENGTH];

    strcpy(path_buffer, path);
    strcat(path_buffer, "/");
    strcat(path_buffer, buf1);

    if (stat(path_buffer, &st) == -1)
        mkdir(path_buffer, 0700);

    dir = opendir(path_buffer);
    if (dir != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type == DT_DIR) {
                // Đệ quy nếu là thư mục
                if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
                    processDirectory(path_buffer, ent->d_name, root);
                }
            } else if (ent->d_type == DT_REG) {
                // Xử lý tệp tin ở đây
                disk_file newfile;

                char path_buffer2[BUFLEN];
                strcpy(path_buffer2, path_buffer);
                strcat(path_buffer2, "/");
                strcat(path_buffer2, ent->d_name);

                newfile.filename = ent->d_name;
                strcpy(newfile.full_path, path_buffer2);
                strcpy(newfile.share_status, "PRIVATE");

                // std::cout << newfile.full_path << std::endl;

                users[std::string(root)].files[std::string(newfile.filename)] = newfile;
            }
        }
        closedir(dir);
    } else {
        perror("Could not open directory");
    }
}

void parse_ucf(char *filename) {
    FILE *ucf = fopen(filename, "r");
    // struct stat st = {0};
    char buf1[MAX_STR_LENGTH], buf2[MAX_STR_LENGTH];
    char path_buffer[BUFLEN];
    int n;
    fscanf(ucf, "%d", &n);
    for (int i = 0; i < n; ++i) {
        char root[4 * MAX_STR_LENGTH]; // the full path of cwd
        memset(root, 0, sizeof(root));
        fscanf(ucf, "%s %s", buf1, buf2);
        user newUser;
        newUser.name = std::string(buf1);
        newUser.pass = std::string(buf2);
        newUser.connected = false;
        newUser.socket = -1;
        // cout<<"|"<<buf1<<"|\n|"<<buf2<<"|"<<endl;
        users[std::string(buf1)] = newUser;

        strcpy(path_buffer, path);
        strcat(path_buffer, "/");
        strcat(path_buffer, buf1);
        strcat(root, buf1);

        processDirectory(path, buf1,root);
    }
    fclose(ucf);
}


/**
 * Function called in init(), parses the file with shared files and
 * modify the status, if necessary, or display the errors in the file on the console.
 */
void parse_sscf(char *filename) {
    FILE *sscf = fopen(filename, "r");
    int n;

    fscanf(sscf, "%d\n", &n);
    char buf[MAX_STR_LENGTH * 2], userbuf[MAX_STR_LENGTH], filebuf[MAX_STR_LENGTH];
    for (int i = 0; i < n; ++i) {
        fgets(buf, 2 * MAX_STR_LENGTH, sscf);
        char *pch = strtok(buf, ":");
        strcpy(userbuf, pch);
        pch = strtok(NULL, ":\n");
        strcpy(filebuf, pch);
        std::string user_str = std::string(userbuf);
        std::string file_str = std::string(filebuf);
        // auto it = users[user_str].files.find("file1.txt");

        // if (it != users[user_str].files.end())
        // {
        //     printf("true\n");
        // }
        // else
        // {
        //     printf("False\n");
        // }
        if (users.find(user_str) == users.end()) // no user
            printf("%s: Line %d: user `%s` does not exist\n", filename, i + 2, userbuf);
        else if (users[user_str].files.find(file_str) == users[user_str].files.end()){
            printf("%s: Line %d: `%s` in %s's homedir does not exist\n", filename, i + 2, filename, userbuf);
        }
            // printf("%s: Line %d: `%s` in %s's homedir does not exist\n", filename, i + 2, filename, userbuf);
        else
            strcpy(users[user_str].files[file_str].share_status, "SHARED");
    }

    fclose(sscf);
}

/**
 * parses the arguments and sets the path variable.
 */
void sv_init(char **args) {

    getcwd(path, sizeof(path));

    sscanf(args[1], "%d", &listen_port);
    sscanf(args[2], "%s", ucf_filename);
    sscanf(args[3], "%s", sscf_filename);

    // printf("OK1\n");
    parse_ucf(ucf_filename);
    // printf("OK2\n");
    parse_sscf(sscf_filename);

    printf("Awaiting connections...\n");
}

/**
 * Execute the login command, verifying the correctness of the arguments and send
 * the answer back to the client. Update all relevant fields.
 *
 * cmd is the command extracted from the message from the client
 * socket is the socket on which the server communicates with the initiating client
 * command
 */
int exec_signup(command cmd, int socket){
    std::string username = std::string(cmd.args[0]);
    std::string pass = std::string(cmd.args[1]);
    std::string cfpass = std::string(cmd.args[2]);
    // std::cout << "pass: [" << pass <<"]"<< std::endl;
    // std::cout << "cfpass: [" << cfpass <<"]"<< std::endl;
    std::map<std::string, user>::iterator it;
    it = users.find(username);
    if(it != users.end()){
        message msg_to_send;
        msg_to_send.type = CMD_ERROR; // error
        strcpy((char *)msg_to_send.payload, "-12  Username have existed. Please choose another.");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_signup: duplicate credentials: ERROR in send");

        return -12;
    }else if(pass != cfpass){
        message msg_to_send;
        msg_to_send.type = CMD_ERROR; // error
        strcpy((char *)msg_to_send.payload, "-13  Password and confirm password must be match.");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_signup: password and cfpassword not match: ERROR in send");

        return -13;
    }else{
        user newUser;
        newUser.name = username;
        newUser.pass = pass;
        newUser.connected = false;
        newUser.socket = -1;
        users[username] = newUser;
        // sscanf(args[2], "%s", ucf_filename);
        // sscanf(args[3], "%s", sscf_filename);
        FILE *ucf = fopen(ucf_filename, "r+");
        int n;
        fscanf(ucf, "%d", &n);
        // Move the file pointer to the beginning of the file
        fseek(ucf, 0, SEEK_SET);

        // Write the new value of n to the file
        fprintf(ucf, "%d", n+1);
        // Move the file pointer to the end of the file
        fseek(ucf, 0, SEEK_END);

        // Add a new line at the end of the file
        fprintf(ucf, "\n");

        // Now, you can write additional data at the end of the file
        fprintf(ucf, "%s %s", username.c_str(), pass.c_str());
        fclose(ucf);
        parse_ucf(ucf_filename);
        parse_sscf(sscf_filename);

        message msg_to_send;
        msg_to_send.type = CMD_SUCCESS; // success
        cmd_to_char(cmd, (char*)msg_to_send.payload);
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_signup: success: ERROR in send");
    }
    return 0;

}
int exec_login(command cmd, int socket) {

    std::string username = std::string(cmd.args[0]);
    std::string pass = std::string(cmd.args[1]);

    std::map<std::string, user>::iterator it;
    it = users.find(username);
    if (it == users.end()) { // 
        clients[socket].failed_login_attempts++;

        if (clients[socket].failed_login_attempts == 3) {
            message msg_to_send;
            msg_to_send.type = CMD_ERROR; // error
            strcpy((char*)msg_to_send.payload, "-8 Brute-force detected");
            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
            cout<<"Wrong over 3 times..Brute-force detected\n";
            int bytecount = mySend(socket, msg_to_send);
            if (bytecount < 0)
                error("exec_login: bruteforce: ERROR in send");
            FD_CLR(socket, &read_fds);
            close(socket);
            printf("bruteforce attempt: closed connection on socket %d\n", socket);
            return -8;
        }

        message msg_to_send;
        msg_to_send.type = CMD_ERROR; 
        strcpy((char *)msg_to_send.payload, "-3  Wrong user/password\n");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<"Wrong user or password\n";
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_login: wrong credentials: ERROR in send");

        return -3;
    } else if (users[username].pass != pass) { // the password is different
        clients[socket].failed_login_attempts++;

        if (clients[socket].failed_login_attempts == 3) {
            message msg_to_send;
            msg_to_send.type = CMD_ERROR; 
            strcpy((char*)msg_to_send.payload, "-8  Brute-force detected\n");
            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
            cout<<"Wrong over 3 times..Brute-force detected\n";
            int bytecount = mySend(socket, msg_to_send);
            if (bytecount < 0)
                error("exec_login: bruteforce: ERROR in send");
            FD_CLR(socket, &read_fds);
            close(socket);
            printf("bruteforce attempt: closed connection on socket %d\n", socket);
            return -8;
        }

        message msg_to_send;
        msg_to_send.type = CMD_ERROR; 
        strcpy((char *)msg_to_send.payload, "-3  Wrong user/password\n");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<"Wrong user or password\n";
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_login: wrong credentials: ERROR in send");

        return -3;
    } else { // correct user password
        users[username].connected = true;
        users[username].socket = socket;
        clients[socket].connected_user = username;
        clients[socket].failed_login_attempts = 0;

        message msg_to_send;
        msg_to_send.type = CMD_SUCCESS; // success
        cmd_to_char(cmd, (char*)msg_to_send.payload);
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<"Account `"<<username<<"` login successfully.\n";
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_login: success: ERROR in send");
    }

    return 0;
}

/**
 * Execute the logout command, updating all relevant fields and send
 * the response back to the client. (CMD_SUCCESS only)
 */
int exec_logout(command cmd, int socket) {

    std::string username = clients[socket].connected_user;
    users[username].socket = -1;
    users[username].connected = false;
    clients[socket].connected_user = std::string("none");

    message msg_to_send;
    msg_to_send.type = CMD_SUCCESS;
    cmd_to_char(cmd, (char*)msg_to_send.payload);
    msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
    cout<<"Account `"<<username<<"` logout successfully.\n";
    int bytecount = mySend(socket, msg_to_send);
    if (bytecount < 0)
        error("exec_logout: success: ERROR in send");
    return 0;
}

/**
 * Execute the getuserlist command, create and send back the response.
 */
int exec_getuserlist(command cmd, int socket) {
    message msg_to_send;
    msg_to_send.type = CMD_SUCCESS;
    strcpy((char*)msg_to_send.payload, "LIST USER:\n");
    for (std::map<std::string, user>::iterator iter = users.begin(); iter != users.end(); iter++) {
        strcat((char*)msg_to_send.payload, iter->first.c_str());
        strcat((char*)msg_to_send.payload, "\n");
    }
    msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

    int bytecount = mySend(socket, msg_to_send);
    if (bytecount < 0)
        error("exec_getuserlist: success: ERROR in send");
    return 0;
}

/**
 * Execute the getfilelist command, check the correctness of the arguments, create and
 * send back the answer.
 */
int exec_getfilelist(command cmd, int socket) {

    std::string user_str = std::string(cmd.args[0]);
    if (users.find(user_str) == users.end()) {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-11 Non-existent user");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<"Non-existent user\n";
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_getfilelist: wrong username: ERROR in send");
    } else {
        message msg_to_send;
        msg_to_send.type = CMD_SUCCESS;
        string temp = "LIST FILE OF USER "+user_str+" :\n";
        strcpy((char*)msg_to_send.payload, temp.c_str());

        struct stat st = {0};
        std::map<std::string, disk_file>::iterator it;
        for (it = users[user_str].files.begin(); it != users[user_str].files.end(); it++) {
            strcat((char*)msg_to_send.payload, it->second.filename.c_str());
            stat(it->second.full_path, &st);
            char buf[BUFLEN];
            sprintf(buf, "\t%ld bytes\t", st.st_size);
            strcat((char*)msg_to_send.payload, buf);
            strcat((char*)msg_to_send.payload, it->second.share_status);
            strcat((char*)msg_to_send.payload, "\n");
        }
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_getfilelist: success: ERROR in send");
    }

    return 0;
}

/**
 * Execute the upload command. (rather initiates it)
 * If there are no errors in the arguments, create the file in which the data will be written
 * and sends back to the client the file descriptor from the server.
 * Enter the newly created file in all relevant fields.
 */
int exec_upload(command cmd, int socket) {

    std::string user_str = clients[socket].connected_user;
    std::string filesrc_str = cmd.args[0];
    string des_str = cmd.args[1];

    std::string file_str = getFolderNameFromPath(filesrc_str);
    string des_path;
    if(des_str != "")
    des_path = std::string(path) + "/" + user_str + "/" + des_str;
    else
    des_path = std::string(path) + "/" + user_str;
    if(!directoryExists(des_path)){
        message msg_to_send;

        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-9  Folder destination not exist.\n");
        msg_to_send.length = strlen((char*)msg_to_send.payload);
        cout<<"Folder destination not exist.\n";
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_upload: check folder destination: ERROR in send");
    }
    else if (users[user_str].files.find(file_str) != users[user_str].files.end()) {
        message msg_to_send;

        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-9  File existed on server");
        msg_to_send.length = strlen((char*)msg_to_send.payload);
        cout<<"File existed on server\n";
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_upload: file overwrite: ERROR in send");
    } else {

        char path_buffer[BUFLEN];
        strcpy(path_buffer, des_path.c_str());
        // strcat(path_buffer, "/");
        // strcat(path_buffer, user_str.c_str());
        strcat(path_buffer, "/");
        strcat(path_buffer, file_str.c_str());
        cout<<"Path: "<<path_buffer<<endl;
        
        int fd = open(path_buffer, O_WRONLY | O_CREAT);
        if (fd < 0)
            error("exec_upload: ERROR in open");
        // printf("FILE ON SERVER: %d\n", fd);

        sv_filestat newfs;
        newfs.filename = file_str;
        newfs.is_in_upload = true;
        newfs.is_in_download = false;
        newfs.cl_socket = socket;
        strcpy(newfs.full_path, path_buffer);
        files_to_transfer[fd] = newfs;

        disk_file newfile;
        newfile.filename = file_str;
        strcpy(newfile.full_path, newfs.full_path);
        strcpy(newfile.share_status, "PRIVATE");
        users[user_str].files[file_str] = newfile;

        message msg_to_send;
        msg_to_send.type = INIT_UPLOAD;
        sprintf((char*)msg_to_send.payload, "%d %s", fd, file_str.c_str());
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_upload: file ok: ERROR in send");
    }
    return 0;
}

/**
 * Execute the download command. (rather initiates it)
 * If there are no errors in the arguments and the download is allowed, open
 * the file requested for reading and sends back to the file client
 * its descriptor.
 */
int exec_download(command cmd, int socket) {
    std::string user_str = clients[socket].connected_user;
    std::string requser_str = std::string(cmd.args[0]);
    std::string file_path = std::string(cmd.args[1]);
    string des_path = std::string(cmd.args[2]);

    string full_path = (string)path + "/" + requser_str + "/" + file_path;
    if(!isFile(full_path.c_str())){
        message msg_to_send;

        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-4.1  File not exist or wrong path.\n");
        msg_to_send.length = strlen((char*)msg_to_send.payload);
        cout<<full_path<<endl;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_download: check file exist: ERROR in send");
        return -9;
    }
    string file_str = getFolderNameFromPath(file_path);
    // printf("user_str=`%s`; requser_str=`%s`\n", user_str.c_str(), requser_str.c_str());

    std::map<std::string, disk_file>::iterator it;
    it = users[requser_str].files.find(file_str);
    if (it == users[requser_str].files.end()) {
        message msg_to_send;

        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-4  Non-existent file");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        if (mySend(socket, msg_to_send) < 0)
            error("exec_download: wrong filename: ERROR in send");
    } else if ((!strcmp(it->second.share_status, "PRIVATE")) && (user_str != requser_str)) {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-5  Prohibited download");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        if (mySend(socket, msg_to_send) < 0)
            error("exec_download: not shared: ERROR in send");
    } else {
        char path_buffer[BUFLEN];
        strcpy(path_buffer, it->second.full_path);
        // strcat(path_buffer, "/");
        // strcat(path_buffer, requser_str.c_str());
        // strcat(path_buffer, "/");
        // strcat(path_buffer, file_str.c_str());
        // ^ am creat calea absoluta a fisierului de deschis

        if (is_uploading(path_buffer)) {
            message msg_to_send;

            msg_to_send.type = CMD_ERROR;
            strcpy((char*)msg_to_send.payload, "-10 File in transfer");
            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
            cout<<"File in transfer\n";
            if (mySend(socket, msg_to_send) < 0)
                error("exec_download: is_uploading: ERROR in send");
            return -10;
        }

        int fd = open(path_buffer, O_RDONLY);
        if (fd < 0)
            error("exec_upload: ERROR in open");
        // printf("FILE ON SERVER: %d\n", fd);

        sv_filestat newfs;
        newfs.filename = file_str;
        newfs.is_in_upload = false;
        newfs.is_in_download = true;
        newfs.cl_socket = socket;
        strcpy(newfs.full_path, path_buffer);
        files_to_transfer[fd] = newfs;
        
        message msg_to_send;
        msg_to_send.type = INIT_DOWNLOAD;
        sprintf((char*)msg_to_send.payload, "%d %s %s", fd, file_str.c_str(),des_path.c_str());
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        if (mySend(socket, msg_to_send) < 0)
            error("exec_upload: file ok: ERROR in send");
    }
    return 0;
}
int exec_downloadFolder(command cmd, int socket) {
    std::string user_str = clients[socket].connected_user;
    std::string requser_str = std::string(cmd.args[0]);
    std::string folder_path = std::string(cmd.args[1]);

    string full_path = (string)path + "/" + requser_str + "/" + folder_path;
    string folderName = getFolderNameFromPath(folder_path);
    if(!isFolder(const_cast<char*>(full_path.c_str()))){
        message msg_to_send;

        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-4.1  Folder not exist or wrong path.\n");
        msg_to_send.length = strlen((char*)msg_to_send.payload);
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_download: check file exist: ERROR in send");
        return -9;
    }else{
    // printf("user_str=`%s`; requser_str=`%s`\n", user_str.c_str(), requser_str.c_str());

        int fd = open(full_path.c_str(), O_RDONLY);
        if (fd < 0)
            error("exec_upload: ERROR in open");
        // printf("FILE ON SERVER: %d\n", fd);
        vector<std::string> file_list;
        folder_path = requser_str + "/" + folder_path;
        getAllFilesInDirectory(folder_path,file_list);
        string pathListFile = "";
        for (const auto& file : file_list) {
                                pathListFile = pathListFile+file+";";
                            }
        message msg_to_send;
        cout<<"Path-file: "<<pathListFile;
        msg_to_send.type = INIT_FOLDER_DOWNLOAD;
        sprintf((char*)msg_to_send.payload, "%d %s %s %s", fd,requser_str.c_str(), pathListFile.c_str(),folderName.c_str());
        cout<<"Init download folder successfull at server !\n";
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        if (mySend(socket, msg_to_send) < 0)
            error("exec_upload: file ok: ERROR in send");
    }
    return 0;
}
int exec_uploadFolder(command cmd, int socket){
    std::string user_str = clients[socket].connected_user;
    std::string folder_str = cmd.args[0];
    std::string des_str = cmd.args[1];

    std::string folder_name = getFolderNameFromPath(folder_str);
    cout<<"foldername: "<<folder_name<<endl;
    string source_path;
    if(des_str!="")
    source_path = std::string(path) + "/" + user_str + "/" + des_str;
    else 
    source_path = std::string(path) + "/" + user_str;


    if(directoryExists(source_path)){
        source_path = source_path +"/"+ folder_name;
        if(!directoryExists(source_path)){
            if (mkdir(source_path.c_str(), 0700) == -1) {
                perror("exec_folder_upload: ERROR on mkdir");
    // Xử lý lỗi nếu cần thiết
                }
            int fd = open(source_path.c_str(), O_RDONLY);
            if (fd < 0){error("exec_upload: ERROR in open");}
            message msg_to_send;
            msg_to_send.type = INIT_FOLDER_UPLOAD;
            if(des_str =="") des_str += folder_name;
            else des_str = des_str + "/"+folder_name;
            sprintf((char*)msg_to_send.payload, "%d %s %s", fd, folder_str.c_str(),des_str.c_str());
            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
            cout<<"abc: "<<msg_to_send.payload<<endl;
            int bytecount = mySend(socket, msg_to_send);
            if (bytecount < 0)
                error("exec_folder_upload: folder ok: ERROR in send");
        }else{
            message msg_to_send;
            msg_to_send.type = CMD_ERROR;
            strcpy((char*)msg_to_send.payload, "-4.2  Folder's name exist. Need another name or another destination.");
            msg_to_send.length = strlen((char*)msg_to_send.payload);
            int bytecount = mySend(socket, msg_to_send);
            if (bytecount < 0)
                error("exec_upload: Name of folder want to upload existed: ERROR in send");    
        }
    } else {
    message msg_to_send;
    msg_to_send.type = CMD_ERROR;
    strcpy((char*)msg_to_send.payload, "-4.1  Folder destination not existed.. Please check again!");
    msg_to_send.length = strlen((char*)msg_to_send.payload);
    cout<<"Folder destination not existed.. Please check again!\n";
    int bytecount = mySend(socket, msg_to_send);
    if (bytecount < 0)
        error("exec_upload: file folder not exist: ERROR in send");    
    }
    return 0;
}
/**
 * Execute the share command, update the relevant structures, if possible,
 * and send back a corresponding answer.
 */
int exec_share(command cmd, int socket) {

    std::string user_str = clients[socket].connected_user;
    std::string file_str = std::string(cmd.args[0]);

    if (users[user_str].files.find(file_str) == users[user_str].files.end()) {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-4  Non-existent file");
        cout<<msg_to_send.payload<<endl;
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_share: wrong filename: ERROR in send");
    } else if (!strcmp(users[user_str].files[file_str].share_status, "SHARED")) {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-6 The file is already shared");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_share: already shared: ERROR in send");
    } else {
        message msg_to_send;
        msg_to_send.type = CMD_SUCCESS;
        sprintf((char*)msg_to_send.payload, "200 %s file has been shared", cmd.args[0]);
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        
        FILE *ucf = fopen(sscf_filename, "r+");
        int n;
        fscanf(ucf, "%d", &n);
        // Move the file pointer to the beginning of the file
        fseek(ucf, 0, SEEK_SET);
        // Write the new value of n to the file
        fprintf(ucf, "%d", n+1);
        // Move the file pointer to the end of the file
        fseek(ucf, 0, SEEK_END);

        // Add a new line at the end of the file
        fprintf(ucf, "\n");

        // Now, you can write additional data at the end of the file
        fprintf(ucf, "%s:%s:", user_str.c_str(), file_str.c_str());
        fclose(ucf);
        
        cout<<msg_to_send.payload<<endl;
        strcpy(users[user_str].files[file_str].share_status, "SHARED");

        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_share: success: ERROR in send");
    }
    return 0;
}
void moveFile(const std::string& source_path, const std::string& destination_folder) {
    // Ensure that the destination folder ends with a slash
    std::string destination_folder_with_slash = destination_folder;
    if (!destination_folder_with_slash.empty() && destination_folder_with_slash.back() != '/') {
        destination_folder_with_slash += '/';
    }

    // Extract the file name from the source path
    size_t last_slash_pos = source_path.find_last_of('/');
    std::string file_name = (last_slash_pos != std::string::npos) ? source_path.substr(last_slash_pos + 1) : source_path;

    // Construct the new destination path
    std::string new_destination_path = destination_folder_with_slash + file_name;

    // Perform the move operation
    if (rename(source_path.c_str(), new_destination_path.c_str()) != 0) {
        // Handle the error if needed
        std::perror("Error during move: ");
    } else {
        std::cout << "Move operation successful." << std::endl;
    }
}
int exec_move(command cmd, int socket){
    std::string user_str = clients[socket].connected_user;
    std::string source_path = std::string(cmd.args[0]);
    string destination_folder = std::string(cmd.args[1]);
    source_path = string(path) + "/" +user_str+"/"+ source_path;
    if(destination_folder=="") destination_folder = string(path)+"/"+user_str;
    else destination_folder = string(path)+"/"+user_str+"/"+destination_folder;
    cout<<"S: "<<source_path<<" "<<isFile(source_path.c_str())<<" "<<isFolder(const_cast<char*>(destination_folder.c_str()))<<endl;
    cout<<"SS: "<<destination_folder<<endl;
    if(!isFile(source_path.c_str())|| !isFolder(const_cast<char*>(destination_folder.c_str()))){
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char *)msg_to_send.payload, "-4  File source or folder destination not exist\n");
        msg_to_send.length = strlen((char *)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_share: file/folder not exist: ERROR in send");
    }else{
    
    moveFile(source_path, destination_folder);

    // Optionally, send a success message to the client
    message msg_to_send;
    msg_to_send.type = CMD_SUCCESS;
    strcpy((char*)msg_to_send.payload, "200 Move operation successful.\n");
    msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
    cout<<msg_to_send.payload<<endl;
    int bytecount = mySend(socket, msg_to_send);
    if (bytecount < 0) {
        error("exec_move: success: ERROR in send");
        return -1;
    }
    }
    return 0;
}
void copyFile(const std::string& source_path, const std::string& destination_folder) {
    // Ensure that the destination folder ends with a slash
    std::string destination_folder_with_slash = destination_folder;
    if (!destination_folder_with_slash.empty() && destination_folder_with_slash.back() != '/') {
        destination_folder_with_slash += '/';
    }

    // Extract the file name from the source path
    size_t last_slash_pos = source_path.find_last_of('/');
    std::string file_name = (last_slash_pos != std::string::npos) ? source_path.substr(last_slash_pos + 1) : source_path;

    // Construct the new destination path
    std::string new_destination_path = destination_folder_with_slash + file_name;

    std::ifstream source(source_path, std::ios::binary);
    std::ofstream destination(new_destination_path, std::ios::binary);

    if (!source || !destination) {
        // Handle the case where the source or destination file can't be opened
        std::cerr << "Unable to open source or destination file." << std::endl;
        // You might want to send an error message to the client if needed
        return;
    }

    // Copy the content from source to destination
    destination << source.rdbuf();

    // Close the file streams
    source.close();
    destination.close();

    if (!source || !destination) {
        // Handle the case where there is an issue closing the file streams
        std::cerr << "Error closing source or destination file." << std::endl;
        // You might want to send an error message to the client if needed
        return;
    }

    std::cout << "Copy operation successful." << std::endl;
}
int exec_copy(command cmd, int socket){
    std::string user_str = clients[socket].connected_user;
    std::string source_path = std::string(cmd.args[0]);
    string destination_folder = std::string(cmd.args[1]);
    string checkfiledes = getFolderNameFromPath(source_path);

    source_path = string(path) + "/" +user_str+"/"+ source_path;
    if(destination_folder=="") destination_folder = string(path)+"/"+user_str;
    else destination_folder = string(path)+"/"+user_str+"/"+destination_folder;

    checkfiledes = destination_folder +"/"+checkfiledes;
    // cout<<"S: "<<source_path<<" "<<isFile(source_path.c_str())<<" "<<isFolder(const_cast<char*>(destination_folder.c_str()))<<endl;
    // cout<<"SS: "<<destination_folder<<endl;
    // cout<<"SSS: "<<checkfiledes<<endl;
    if(!isFile(source_path.c_str())|| !isFolder(const_cast<char*>(destination_folder.c_str()))){
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char *)msg_to_send.payload, "-4  File source or folder destination not exist\n");
        msg_to_send.length = strlen((char *)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_share: file/folder not exist: ERROR in send");
    }else if(isFile(checkfiledes.c_str())){
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char *)msg_to_send.payload, "-4  File existed in Folder destination\n");
        msg_to_send.length = strlen((char *)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_copy: file exist: ERROR in send");
    }
        else{
    
    copyFile(source_path, destination_folder);

    // Optionally, send a success message to the client
    message msg_to_send;
    msg_to_send.type = CMD_SUCCESS;
    strcpy((char*)msg_to_send.payload, "200 Move operation successful.\n");
    msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
    cout<<msg_to_send.payload<<endl;
    int bytecount = mySend(socket, msg_to_send);
    if (bytecount < 0) {
        error("exec_move: success: ERROR in send");
        return -1;
    }
    }
    return 0;
}
int exec_rename(command cmd, int socket) {

    std::string user_str = clients[socket].connected_user;
    std::string source_str = std::string(cmd.args[0]);
    std::string newname = std::string(cmd.args[1]);
    cout << "check 2\n";
    string file_str = getFolderNameFromPath(source_str);
    string source_path = string(path) + "/" + user_str + "/" + source_str;
    cout << source_path << " "
         << "\n";
    if (!isFile(source_path.c_str()))
    {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char *)msg_to_send.payload, "-4  File not exist");
        msg_to_send.length = strlen((char *)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_share: wrong filename: ERROR in send");
    }
    else
    {
        auto it = users[user_str].files.find(file_str);
        if (it != users[user_str].files.end())
        {
            // Create a new entry with the new key
            users[user_str].files[newname] = it->second;

            // Update the filename in the new entry
            users[user_str].files[newname].filename = newname;
            renameFileOrDirectory(source_path, newname);
            strcpy(users[user_str].files[newname].full_path, source_path.c_str());
            // Erase the old entry
            users[user_str].files.erase(it);
            message msg_to_send;
            msg_to_send.type = CMD_SUCCESS;
            sprintf((char *)msg_to_send.payload, "200 file %s change name successfully.\n", file_str.c_str());
            msg_to_send.length = strlen((char *)msg_to_send.payload) + 1;

            if (!strcmp(users[user_str].files[newname].share_status,"SHARED"))
            {
                FILE *ucf = fopen(sscf_filename, "r+");
                int n;
                fscanf(ucf, "%d", &n);
                // Move the file pointer to the beginning of the file
                fseek(ucf, 0, SEEK_SET);
                // Write the new value of n to the file
                fprintf(ucf, "%d", n + 1);
                // Move the file pointer to the end of the file
                fseek(ucf, 0, SEEK_END);

                // Add a new line at the end of the file
                fprintf(ucf, "\n");

                // Now, you can write additional data at the end of the file
                fprintf(ucf, "%s:%s:", user_str.c_str(), newname.c_str());
                fclose(ucf);
            }

            cout<<msg_to_send.payload<<endl;
            int bytecount = mySend(socket, msg_to_send);
            if (bytecount < 0)
                error("exec_share: success: ERROR in send");
        }
        else
        {
            message msg_to_send;
            msg_to_send.type = CMD_ERROR;
            strcpy((char *)msg_to_send.payload, "400 Error in change file name\n");
            msg_to_send.length = strlen((char *)msg_to_send.payload) + 1;
            cout<<msg_to_send.payload<<endl;
            int bytecount = mySend(socket, msg_to_send);
            if (bytecount < 0)
                error("exec_share: error change file name: ERROR in send");
        }
    }
    return 0;
}
/**
 * Executes the unshare command, updates the relevant structures, if any
 * possible, and send back a corresponding answer.
 */
void removeFromShared(const char *filename, const char *user_str, const char *file_str) {
    FILE *sscf = fopen(filename, "r");
    if (sscf == NULL) {
        perror("Khong the mo file");
        exit(EXIT_FAILURE);
    }

    int n;
    fscanf(sscf, "%d\n", &n);

    // Mảng để lưu trữ tên tệp và tên người dùng từ mỗi dòng
    char buffer[MAX_STR_LENGTH * 2], userbuf[MAX_STR_LENGTH], filebuf[MAX_STR_LENGTH];
    
    // Mở tệp để ghi lại
    FILE *tempFile = fopen("temp.txt", "w");
    if (tempFile == NULL) {
        perror("Khong the mo temporary file");
        fclose(sscf);
        exit(EXIT_FAILURE);
    }

    // Giảm giá trị của n và ghi lại vào đầu tệp
    fprintf(tempFile, "%d\n", n - 1);

    for (int i = 0; i < n; i++) {
        fgets(buffer, 2 * MAX_STR_LENGTH, sscf);

        // Tạo một bản sao của buffer để sử dụng trong strtok
        char buffer_copy[MAX_STR_LENGTH * 2];
        strcpy(buffer_copy, buffer);

        char *pch = strtok(buffer_copy, ":");
        strcpy(userbuf, pch);
        pch = strtok(NULL, ":\n");
        strcpy(filebuf, pch);

        if (strcmp(userbuf, user_str) == 0 && strcmp(filebuf, file_str) == 0) {
            // Bỏ qua dòng có user_str và file_str
            continue;
        }

        // Ghi lại từng dòng (loại bỏ dòng cần xóa)
        if (i == n - 2) {
            buffer[strlen(buffer) - 1] = '\0';
        }
        fprintf(tempFile, "%s", buffer);
    }
    // Đóng tệp
    fclose(sscf);
    fclose(tempFile);

    // Xóa file gốc và đổi tên file tạm thành tên file gốc
    remove(filename);
    rename("temp.txt", filename);
}
int exec_unshare(command cmd, int socket) {

    std::string user_str = clients[socket].connected_user;
    std::string file_str = std::string(cmd.args[0]);

    if (users[user_str].files.find(file_str) == users[user_str].files.end()) {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-4  Non-existent file");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_unshare: wrong filename: ERROR in send");
    } else if (!strcmp(users[user_str].files[file_str].share_status, "PRIVATE")) {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-7  Already private file");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;
        cout<<msg_to_send.payload<<endl;
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_unshare: already shared: ERROR in send");
    } else {
        message msg_to_send;
        msg_to_send.type = CMD_SUCCESS;
        sprintf((char*)msg_to_send.payload, "200 %s file has been set as PRIVATE", cmd.args[0]);
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        strcpy(users[user_str].files[file_str].share_status, "PRIVATE");
        removeFromShared(sscf_filename,user_str.c_str(),file_str.c_str());
        int bytecount = mySend(socket, msg_to_send);
        if (bytecount < 0)
            error("exec_unshare: success: ERROR in send");
    }
    return 0;
}

/**
 * Execute the delete command, delete the file with unlink(), if this is the case
 * possible (exists and is not in transfers). Send back a reply
 * relevant.
 */
int exec_delete(command cmd, int socket) {
    std::string file_path = cmd.args[0];
    std::string user_str = clients[socket].connected_user;

    string file_str = getFolderNameFromPath(file_path);
    std::map<std::string, disk_file>::iterator it;
    it = users[user_str].files.find(file_str);
    if (it == users[user_str].files.end()) {
        message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-4 Non-existent file");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        if (mySend(socket, msg_to_send) < 0)
            error("exec_delete: missing file: ERROR in send");
        return -4;
    } else {
        char path_buffer[BUFLEN];
        strcpy(path_buffer, path);
        strcat(path_buffer, "/");
        strcat(path_buffer, user_str.c_str());
        strcat(path_buffer, "/");
        strcat(path_buffer, file_path.c_str());
        printf("Path: %s\n",path_buffer);
        if(!isFile(path_buffer)){
            message msg_to_send;
        msg_to_send.type = CMD_ERROR;
        strcpy((char*)msg_to_send.payload, "-4 Non-existent file");
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        if (mySend(socket, msg_to_send) < 0)
            error("exec_delete: missing file: ERROR in send");
        return -4;
        }

        if (is_uploading(path_buffer) || is_downloading(path_buffer)) {
            message msg_to_send;
            msg_to_send.type = CMD_ERROR;
            strcpy((char*)msg_to_send.payload, "-10 file in transfer");
            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

            if (mySend(socket, msg_to_send) < 0)
                error("exec_delete: file in use: ERROR in send");

            return -10;
        }
        if (!strcmp(users[user_str].files[file_str].share_status,"SHARED")){
            removeFromShared(sscf_filename,user_str.c_str(),file_str.c_str());
        }
        users[user_str].files.erase(file_str);
        unlink(path_buffer);
        remove(path_buffer);
        message msg_to_send;
        msg_to_send.type = CMD_SUCCESS;
        sprintf((char*)msg_to_send.payload, "200 The file %s has been deleted", cmd.args[0]);
        msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

        if (mySend(socket, msg_to_send) < 0)
            error("exec_delete: success: ERROR in send");
    }

    return 0;
}

/**
 * Nothing to execute, basically. Quit is done at the customer's place.
 */
int exec_quit(command cmd, int socket) {

    message msg_to_send;
    msg_to_send.type = CMD_SUCCESS;
    cmd_to_char(cmd, (char*)msg_to_send.payload);
    msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

    if (mySend(socket, msg_to_send) < 0)
        error("exec_quit: ERROR in send");

    return 0;
}
/**
 * Calls the corresponding functions, depending on the order received.
 */
int execute(command cmd, int socket) {
    int result = 0;
    if(!strcmp(cmd.name, "signup")){
        result = exec_signup(cmd, socket);
    }
    else if (!strcmp(cmd.name, "login")) {
        result = exec_login(cmd, socket);
    } else if (!strcmp(cmd.name, "logout")) {
        result = exec_logout(cmd, socket);
    } else if (!strcmp(cmd.name, "rename")) {
        result = exec_rename(cmd, socket);
    } else if (!strcmp(cmd.name, "move")) {
        result = exec_move(cmd, socket);
    }else if (!strcmp(cmd.name, "copy")) {
        result = exec_copy(cmd, socket);
    }else if (!strcmp(cmd.name, "getuserlist")) {
        result = exec_getuserlist(cmd, socket);
    } else if (!strcmp(cmd.name, "getfilelist")) {
        result = exec_getfilelist(cmd, socket);
    } else if (!strcmp(cmd.name, "upload")) {
        result = exec_upload(cmd, socket);
    } else if (!strcmp(cmd.name, "uploadFolder")) {
        result = exec_uploadFolder(cmd, socket);
    } else if (!strcmp(cmd.name, "downloadFolder")) {
        result = exec_downloadFolder(cmd, socket);
    } else if (!strcmp(cmd.name, "download")) {
        result = exec_download(cmd, socket);
    } else if (!strcmp(cmd.name, "share")) {
        result = exec_share(cmd, socket);
    } else if (!strcmp(cmd.name, "unshare")) {
        result = exec_unshare(cmd, socket);
    } else if (!strcmp(cmd.name, "delete")) {
        result = exec_delete(cmd, socket);
    } else if (!strcmp(cmd.name, "quit")) {
        result = exec_quit(cmd, socket);
    }

    // TODO: logging + errorcodes
    return result;
}

int main(int argc, char **argv)
{
    if (argc != 4)
        error("Usage: ./server <port_server> <users_config_file> "
              "<static_shares_config_file>\n");

    sv_init(argv);
    
    int listen_sock, newsockfd;
    sockaddr_in serv_addr, cli_addr;

    int fdmax;
    socklen_t clilen;

    int i; 

    FD_ZERO(&read_fds);
    FD_ZERO(&temp_fds);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0)
        error("ERORR opening socket");

    memset((char*) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(listen_port);

    int b = bind(listen_sock, (struct sockaddr *) &serv_addr,sizeof(struct sockaddr));
    if (b < 0)
        error("ERROR on binding");

    listen(listen_sock, MAX_CLIENTS);

    FD_SET(listen_sock, &read_fds);
    fdmax = listen_sock;

    int bytes_read; // number of bits read from any socket or file descriptor
    message received_message;
    command cmd;
    while (1) { // main loop, no stop condition
        temp_fds = read_fds;
        if (select(fdmax + 1, &temp_fds, NULL, NULL, NULL) == -1)
            error("ERROR in select");

        // looking for data to read
        for (i = 0; i <= fdmax; ++i) {
            if (FD_ISSET(i, &temp_fds)) {
                if (i == listen_sock) { // new connection
                    clilen = sizeof(cli_addr);
                    if ((newsockfd = accept(listen_sock, (struct sockaddr *)&cli_addr, &clilen)) == -1)
                        error("ERROR in accept");
                    else {
                        FD_SET(newsockfd, &read_fds);
                        if (newsockfd > fdmax)
                            fdmax = newsockfd;
                        printf("%s: new connection from %s on socket %d\n", argv[0], inet_ntoa(cli_addr.sin_addr), newsockfd);
                        client newclient;
                        newclient.connected_user = "";
                        newclient.failed_login_attempts = 0;
                        clients[newsockfd] = newclient;
                    }
                } else if (clients.find(i) != clients.end()) { // something from a client
                    if ((bytes_read = myRecv(i, &received_message)) <= 0) {
                        // error or connection closed by client
                        if (bytes_read == 0) { // connection closed
                            printf("%s: socket %d hung up\n", argv[0], i);
                            clients[i].connected_user = std::string("none");
                        } else
                            error("ERROR in recv");
                        close(i);
                        FD_CLR(i, &read_fds);
                    } else { // data from a client
                        if (received_message.type == CMD_CLIENT) { // command
                            // printf("%s: creating command from `%s`\n", argv[0], received_message.payload);
                            // printmsg(received_message);
                            cmd = create_cmd(received_message.payload, (char*)clients[i].connected_user.c_str());
                            printf("\nRequest: `%s`\n", received_message.payload);
                            execute(cmd, i);

                        } else if (received_message.type == INIT_UPLOAD2) {
                            // printmsg(received_message);

                            int cl_fd, sv_fd;
                            sscanf((char*) received_message.payload, "%d %d", &cl_fd, &sv_fd);

                            cl_to_sv[i][cl_fd] = sv_fd;
                            // ^ I set the mapping to know which file on sv it corresponds to
                            // the one from the client

                        } else if (received_message.type == INIT_DOWNLOAD2) {
                            // printmsg(received_message);

                            int cl_fd, sv_fd;
                            sscanf((char*) received_message.payload, "%d %d", &cl_fd, &sv_fd);

                            cl_to_sv[i][cl_fd] = sv_fd;

                            FD_SET(sv_fd, &read_fds); // 
                            if (sv_fd > fdmax)
                                fdmax = sv_fd;

                        } else if (received_message.type == FILE_TRANSFER) { 
                            uint16_t cl_fd, sv_fd;
                            memcpy(&cl_fd, received_message.payload, 2);
                            sv_fd = cl_to_sv[i][cl_fd]; // the client socket is i
                            write(sv_fd, received_message.payload + 2, received_message.length - 2);

                        } else if (received_message.type == CLOSE_TRANSFER) {
                            uint16_t cl_fd, sv_fd;
                            memcpy(&cl_fd, received_message.payload, 2);
                            sv_fd = cl_to_sv[i][cl_fd];

                            close(sv_fd); // close the uploaded file
                            files_to_transfer[sv_fd].is_in_upload = false;

                            struct stat st = {0};
                            stat(files_to_transfer[sv_fd].full_path, &st);

                            message msg_to_send;
                            msg_to_send.type = CMD_SUCCESS;
                            sprintf((char*)msg_to_send.payload, "Upload finished: %s -- %ld bytes", files_to_transfer[sv_fd].filename.c_str(), st.st_size);
                            msg_to_send.length = strlen((char*)msg_to_send.payload) + 1;

                            if (mySend(i, msg_to_send) < 0)
                                error("upload success: ERROR on send");

                        }
                    }
                } else { // to read from the file, for download
                    unsigned char buffer[4096];
                    int byte_count = read(i, buffer, 4090);
                    int cl_socket = files_to_transfer[i].cl_socket;
                    uint16_t sv_fd;
                    sv_fd = i;

                    if (byte_count < 0)
                        error("citire din fisier: ERROR on read");

                    if (byte_count > 0) { //I haven't reached the end
                        message msg_to_send;
                        msg_to_send.type = FILE_TRANSFER;
                        memcpy(msg_to_send.payload, &sv_fd, 2);
                        memcpy(msg_to_send.payload + 2, buffer, byte_count);
                        msg_to_send.length = byte_count + 2;

                        if (mySend(cl_socket, msg_to_send) < 0)
                            error("file_transfer: ERROR on send");
                    } else { // I arrived at EOF
                        message msg_to_send;
                        msg_to_send.type = CLOSE_TRANSFER;
                        memcpy(msg_to_send.payload, &sv_fd, 2);
                        msg_to_send.length = 2;

                        FD_CLR(sv_fd, &read_fds);
                        close(sv_fd);
                        files_to_transfer[sv_fd].is_in_download = false;

                        if (mySend(cl_socket, msg_to_send) < 0)
                            error("close_transfer: ERROR on send");
                    }

                }
            }
        }

    }

    return 0;
}