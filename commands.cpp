#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "commands.h"
#include "utils.h"

/**
 * Parse the input and create a valid command, or an error, if
 * the arguments are not good.
 */
command create_cmd(unsigned char *cmd, char *user) {
    command new_cmd;
    new_cmd.argc = 0;

    char temp_cmd[BUFLEN];
    strcpy(temp_cmd, (char*)cmd);

    char *pch;

    pch = strtok(temp_cmd, " \n\t");
    if (pch == NULL) {
        strcpy(new_cmd.name, "WHITESPACE");
        return new_cmd;
    }
    strcpy(new_cmd.name, pch);
    if (!strcmp(pch, "signup")) { // signup <username> <pass> <cfpass>
        pch = strtok(NULL, " "); // no username given
        if (pch == NULL) {
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // no pass given
        if (pch == NULL) {
            return new_cmd;
        }
        strcpy(new_cmd.args[1], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // no cfpass given
        if (pch == NULL) {
            return new_cmd;
        }
        strcpy(new_cmd.args[2], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            new_cmd.argc++;
        }

    }else if (!strcmp(pch, "login")) { // login <username> <pass>
        pch = strtok(NULL, " "); // no username given
        if (pch == NULL) {
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // no pass given
        if (pch == NULL) {
            return new_cmd;
        }
        strcpy(new_cmd.args[1], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            new_cmd.argc++;
        }

    } else if (!strcmp(pch, "logout")) { // logout
        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            new_cmd.argc++;
        }

    } else if (!strcmp(pch, "getuserlist")) { // getuserlist
        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            new_cmd.argc++;
        }

    } else if (!strcmp(pch, "getfilelist")) { // getfilelist <username>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no username given
            return new_cmd;
        }
        if (!strcmp(pch, "@"))
            strcpy(new_cmd.args[0], user);
        else
            strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            new_cmd.argc++;
        }

    } else if (!strcmp(pch, "upload")) { // upload <filename> <des>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no filename given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // no des_str given

        if (!strcmp(pch, "@"))
            strcpy(new_cmd.args[1], "");
        else
            strcpy(new_cmd.args[1], pch);

        new_cmd.argc++;
        
        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            new_cmd.argc++;
        }

    } else if(!strcmp(pch, "uploadFolder")){ //upload <folder> <path|@>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no filename given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // no des_str given

        if (!strcmp(pch, "@"))
            strcpy(new_cmd.args[1], "");
        else
            strcpy(new_cmd.args[1], pch);

        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            printf("pch: %s\n",pch);
            new_cmd.argc++;
        }
    } else if(!strcmp(pch, "downloadFolder")){ //downloadFolder <username> <folder>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no username given
            return new_cmd;
        }

        if (!strcmp(pch, "@"))
            strcpy(new_cmd.args[0], user);
        else
            strcpy(new_cmd.args[0], pch);

        new_cmd.argc++;

        pch = strtok(NULL, " "); 
        strcpy(new_cmd.args[1], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            printf("pch: %s\n",pch);
            new_cmd.argc++;
        }
    } else if (!strcmp(pch, "download")) { // download <username> <filename> <desFolder|@>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no username given
            return new_cmd;
        }
        if (!strcmp(pch, "@"))
            strcpy(new_cmd.args[0], user);
        else
            strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " ");
        if (pch == NULL) { // no filename given
            return new_cmd;
        }
        strcpy(new_cmd.args[1], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " ");
        if (pch == NULL) { // no des given
            return new_cmd;
        }
        strcpy(new_cmd.args[2], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            printf("pch: %s\n",pch);
            new_cmd.argc++;
        }

    } else if (!strcmp(pch, "share")) { // share <filename>
        pch = strtok(NULL, "");
        if (pch == NULL) { // no filename given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

    } else if (!strcmp(pch, "unshare")) { // unshare <filename>
        pch = strtok(NULL, "");
        if (pch == NULL) { // no filename given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

    } else if (!strcmp(pch, "delete")) { // delete <filename>
        pch = strtok(NULL, "");
        if (pch == NULL) { // no filename given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

    }else if (!strcmp(pch, "rename")) { // rename <filename> <newfilename>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no filename given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " ");
        if (pch == NULL) { // no newfilename given
            return new_cmd;
        }
        
        strcpy(new_cmd.args[1], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            printf("pch: %s\n",pch);
            new_cmd.argc++;
        }
        printf("nnnn: %d\n",new_cmd.argc);
    }
    else if (!strcmp(pch, "move")) { // move <src> <des>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no src given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " ");
        if (pch == NULL) { // no des given
            return new_cmd;
        }
        if (!strcmp(pch, "@"))
            strcpy(new_cmd.args[1], "");
        else
            strcpy(new_cmd.args[1], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            printf("pch: %s\n",pch);
            new_cmd.argc++;
        }
    }else if (!strcmp(pch, "copy")) { // move <src> <des>
        pch = strtok(NULL, " ");
        if (pch == NULL) { // no src given
            return new_cmd;
        }
        strcpy(new_cmd.args[0], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " ");
        if (pch == NULL) { // no des given
            return new_cmd;
        }
        if (!strcmp(pch, "@"))
            strcpy(new_cmd.args[1], "");
        else
            strcpy(new_cmd.args[1], pch);
        new_cmd.argc++;

        pch = strtok(NULL, " "); // checking for extra args
        if (pch != NULL) {
            printf("pch: %s\n",pch);
            new_cmd.argc++;
        }
    }
     else if (!strcmp(pch, "quit")) { // quit
        pch = strtok(NULL, " ");
        if (pch != NULL) {
            new_cmd.argc++;
        }
    } else { // not a command
        strcpy(new_cmd.name, "ERROR");
    }
    return new_cmd;
}

/**
 * I used for testing, recompose the string "command <arguments>"
 */
void cmd_to_char(command cmd, char *dest) {

    strcpy(dest, cmd.name);
    for (int i = 0; i < cmd.argc; ++i) {
        strcat(dest, " ");
        strcat(dest, cmd.args[i]);
    }
}