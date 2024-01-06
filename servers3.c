#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include<dirent.h>
#include<semaphore.h>
#include<ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define MUTEXFILE "/sem-mutex1"
#define MUTEXUSER "/sem-mutex2"
#define MUTEXDISPLAY "/sem-mutex3"
#define MAX_USERS 500
#define MAX_FILES 200
#define FILE_STORAGE_DIRECTORY "./files/"
#define MAX_DOWNLOADS 50

sem_t *mutex_file;
sem_t *mutex_user;
sem_t *mutex_display;

struct ClientInfo {
    int socket;
};

struct Credentials {
    char username[256];
    char hashedPassword[256];
};

struct AdministratorCredentials {
    char username[256];
    char hashedPassword[256];
};

struct DownloadStatus {
    char filename[256];
    double download_progress;
};

struct FileInfo {
    char filename[256];
    long size;
    time_t last_modified;
    char uploader[256];
    char last_downloader[256];
    int num_downloads;
};

struct FileInfo file_info_list[MAX_FILES]; // adjust as needed
int num_files = 0;
int numUsers=0;
struct Credentials userCredentials[MAX_USERS];
struct AdministratorCredentials adminCredentials = {"admin", "3456"};

void saveDownloadStatusToFile(const char *username,struct DownloadStatus *download_status_list, int *num_downloads) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s.txt", FILE_STORAGE_DIRECTORY,username);
    FILE *file = fopen(filepath, "w");
    if (file == NULL) {
        perror("Error opening client download status file");
        return;
    }

    for (int i = 0; i < *num_downloads; i++) {
        fprintf(file, "%s %lf\n", download_status_list[i].filename, download_status_list[i].download_progress);
    }
    //printf("SAVE\n");
    fclose(file);
}

void loadDownloadStatusFromFile(char *username,struct DownloadStatus *download_status_list, int *num_downloads) {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s.txt", FILE_STORAGE_DIRECTORY,username);
    FILE *file = fopen(filepath, "r");
    if (file == NULL) {
        printf("Download status file not found. No data loaded.\n");
        return;
    }
    while (fscanf(file, "%s %lf", download_status_list[*num_downloads].filename, &download_status_list[*num_downloads].download_progress) == 2) {
        (*num_downloads)++; // Increment the number of downloads
    }
    //printf("LOAD\n");
    fclose(file);
}

void saveCredentialsToFile() {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", FILE_STORAGE_DIRECTORY,"credentials.txt");
    FILE* file = fopen(filepath, "w");
    if (file == NULL) {
        printf("Error opening file for writing.\n");
        return;
    }

    for (int i = 0; i < numUsers; i++) {
        fprintf(file, "%s %s\n", userCredentials[i].username, userCredentials[i].hashedPassword);
    }
    fclose(file);
}

void saveFileInfoListToFile() {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", FILE_STORAGE_DIRECTORY,"file_info.txt");
    FILE* file = fopen(filepath, "w");
    if (file == NULL) {
        perror("Error opening file_info.txt");
        return;
    }

    for (int i = 0; i < num_files; i++) {
        fprintf(file, "%s %ld %ld %s %s %d\n",
                file_info_list[i].filename,
                file_info_list[i].size,
                file_info_list[i].last_modified,
                file_info_list[i].uploader,
                file_info_list[i].last_downloader,
                file_info_list[i].num_downloads);
    }

    fclose(file);
}

void loadFileInfoListFromFile() {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", FILE_STORAGE_DIRECTORY,"file_info.txt");
    FILE* file = fopen(filepath, "r");
    if (file == NULL) {
        printf("File info not found. No data loaded.\n");
        return;
    }

    while (num_files < sizeof(file_info_list) / sizeof(file_info_list[0]) &&
           fscanf(file, "%s %ld %ld %s %s %d",
                  file_info_list[num_files].filename,
                  &file_info_list[num_files].size,
                  &file_info_list[num_files].last_modified,
                  file_info_list[num_files].uploader,
                  file_info_list[num_files].last_downloader,
                  &file_info_list[num_files].num_downloads) == 6) {
        num_files++;
    }

    fclose(file);
}

void loadCredentialsFromFile() {
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s%s", FILE_STORAGE_DIRECTORY,"credentials.txt");
    FILE* file = fopen(filepath, "r");
    if (file == NULL) {
        printf("Credentials file not found. No data loaded.\n");
        return;
    }

    while (fscanf(file, "%s %s", userCredentials[numUsers].username, userCredentials[numUsers].hashedPassword) == 2) {
        numUsers++; // Increment the number of registered users
    }

    fclose(file);
}

void updateFileInfo(const char *filename, const char *uploader) {
    for (int i = 0; i < num_files; i++) {
        if (strcmp(filename, file_info_list[i].filename) == 0) {
            file_info_list[i].num_downloads++;
            strncpy(file_info_list[i].last_downloader, uploader, sizeof(file_info_list[i].last_downloader));
            file_info_list[i].last_modified = time(NULL);
            break;
        }
    }
}

void updateDownloadStatus(const char *filename,const char *username,double download_status,struct DownloadStatus *download_status_list, int *num_downloads) {
    int new=1;
    for (int i = 0; i < (*num_downloads); i++) {
        if (strcmp(filename, download_status_list[i].filename) == 0) {
            // Update download status for the specified client
            download_status_list[i].download_progress = download_status;

            // Save the updated status to the file
            saveDownloadStatusToFile(username,download_status_list,num_downloads);
            new=0;
            break;
        }
    }
    if(new)
    {
        // Create a new structure
        if ((*num_downloads) < MAX_DOWNLOADS) {
            strncpy(download_status_list[*num_downloads].filename, filename, sizeof(download_status_list[*num_downloads].filename));
            download_status_list[*num_downloads].download_progress = download_status;
            (*num_downloads)++;
            saveDownloadStatusToFile(username,download_status_list,num_downloads);
        } else {
            printf("Maximum number of downloads reached.\n");
        }
    }
    //printf("UPDATE\n");
}

int handleRemoveFile(int client_socket,char *username) {
    char filepath[512];  // Assumes the maximum directory path length
    char filename[256];
    recv(client_socket, filename, sizeof(filename), 0);
    snprintf(filepath, sizeof(filepath), "%s%s", FILE_STORAGE_DIRECTORY, filename);

    sem_wait(mutex_file);

    for (int i = 0; i < num_files; i++) {
        if (strcmp(file_info_list[i].filename, filename) == 0) {
            if(strcmp(file_info_list[i].uploader,username) == 0)
            {
                // File found, remove it from the directory
                if (remove(filepath) != 0) {
                    perror("Error deleting the file");
                    sem_post(mutex_file);
                    return 0; // Error deleting the file
                }
                sem_post(mutex_file);
                sem_wait(mutex_display);
                // Remove the corresponding entry from fileStorage
                for (int j = i; j < num_files - 1; j++) {
                    file_info_list[j] = file_info_list[j + 1];
                }
                // Clear the last element
                memset(&file_info_list[num_files - 1], 0, sizeof(struct FileInfo));
                num_files--;
                saveFileInfoListToFile();
                sem_post(mutex_display);
                return 1; // File removed successfully

            }
            else
            {
                break;
            }
        }
    }
    sem_post(mutex_file);
    return 0; // File not found
}

void handleFileDownload(int client_socket,char *username,struct DownloadStatus *download_status_list, int *num_downloads) {
    char filename[256];
    int error=0;
    char errorMessage[1];
    char filepath[512];
    while(!error)
    {
        memset(filename, 0, sizeof(filename));
        memset(filepath, 0, sizeof(filepath));
        recv(client_socket, filename, sizeof(filename), 0);
        snprintf(filepath, sizeof(filepath), "%s%s", FILE_STORAGE_DIRECTORY, filename);
        sem_wait(mutex_file);
        if (access(filepath, F_OK) == -1) {
            // File does not exist, send an error message to the client
            errorMessage[0] = '0';
            send(client_socket, errorMessage, sizeof(errorMessage), 0);
            printf("File '%s' not found.\n", filename);
        }
        else
        {
            error=1;
            errorMessage[0] = '1';
            send(client_socket, errorMessage, sizeof(errorMessage), 0);
        }
        sem_post(mutex_file);

    }
    clock_t start_time = clock();
    sem_wait(mutex_file);

    // Send the file size first
    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        perror("Error opening file");
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    send(client_socket, &file_size, sizeof(long),0);

    // Receive the starting point of the download
    long start_point;
    recv(client_socket, &start_point, sizeof(long), 0);

    // Move the file pointer to the starting point
    fseek(file, start_point, SEEK_SET);

    // Dynamically allocate buffer based on file size
    char *buffer = (char *)malloc(file_size);
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        fclose(file);
        sem_post(mutex_file);
        return; // Exit the function
    }

    int bytes_read;
    int total_bytes_sent = start_point;
    ssize_t bytes_sent;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        bytes_sent=send(client_socket, buffer, bytes_read,0);

        total_bytes_sent += bytes_read;

        // Check for client disconnection
        if (bytes_sent < 0) {
            // Handle cleanup, update FileInfo structure, etc.
            double download_progress = ((double)total_bytes_sent / file_size) * 100.0;

            fclose(file);
            free(buffer);
            updateDownloadStatus(filename,username,download_progress,download_status_list,num_downloads);
            sem_post(mutex_file);
            printf("Download is disrupted\n");
            // Exit the function or thread gracefully
            // Client disconnected
            close(client_socket);
            pthread_exit(NULL);
        }
    }

    fclose(file);
    free(buffer);

    clock_t end_time = clock();

    // Calculate and display download speed
    double download_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    double download_speed = file_size / download_time;

    // Update the FileInfo structure for the downloaded file
    updateDownloadStatus(filename,username, 100.0,download_status_list,num_downloads);
    sem_post(mutex_file);
    sem_wait(mutex_display);
    updateFileInfo(filename, username); // Assuming download_status of 100
    saveFileInfoListToFile();
    sem_post(mutex_display);
    
    printf("File '%s' sent to client. Download speed: %.2f bytes/second\n", filename, download_speed);
}

void handleFileList(int client_socket) {
    sem_wait(mutex_display);
    int num_files_to_send = num_files;

    // Send the number of files to the client
    send(client_socket, &num_files_to_send, sizeof(int), 0);

    for (int i = 0; i < num_files; i++) {
        send(client_socket, &file_info_list[i], sizeof(struct FileInfo), 0);
    }
    sem_post(mutex_display);

    printf("File details sent to client.\n");
}

void handleDownloadFileList(int client_socket,struct DownloadStatus *download_status_list, int num_downloads) {
    sem_wait(mutex_file);
    int num_downloads_to_send = num_downloads;

    // Send the number of files to the client
    send(client_socket, &num_downloads_to_send, sizeof(int), 0);

    for (int i = 0; i < num_downloads; i++) {
        send(client_socket, &download_status_list[i], sizeof(struct DownloadStatus), 0);
    }
    sem_post(mutex_file);

    printf("Download file details sent to client.\n");
}


void handleFileUpload(int client_socket,char *uploader) {
    char filename[256];
    recv(client_socket, filename, sizeof(filename), 0);
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", FILE_STORAGE_DIRECTORY, filename);
    int error=0;

    clock_t start_time = clock();
    sem_wait(mutex_file);

    // Check if the file already exists in the directory
    if (access(full_path, F_OK) != -1) {
        // File already exists, send an error message to the client
        error=1;
        send(client_socket, &error, sizeof(int), 0);
        printf("File '%s' already exists in server.Upload aborted\n", filename);
        sem_post(mutex_file);
        return; // Exit the function
    }
    send(client_socket, &error, sizeof(int), 0);
    long file_size;
    recv(client_socket, &file_size, sizeof(long),0);
    long size=file_size;

    // Dynamically allocate buffer based on file size
    char *buffer = (char *)malloc(file_size);
    if (buffer == NULL) {
        perror("Error allocating memory for buffer");
        sem_post(mutex_file);
        return; // Exit the function
    }

    // Receive the file data
    FILE *file = fopen(full_path, "wb");
    if (file == NULL) {
        perror("Error opening file");
    }
    
    int bytes_received;
    while (file_size > 0) {
        bytes_received = recv(client_socket, buffer, sizeof(buffer),0);
        fwrite(buffer, 1, bytes_received, file);
        file_size -= bytes_received;
    }

    fclose(file);
    free(buffer);

    clock_t end_time = clock();

    // Calculate and display upload speed
    double upload_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
    double upload_speed = size / upload_time;
    sem_post(mutex_file);
    sem_wait(mutex_display);
    // Create a new FileInfo structure for the uploaded file
    if (num_files < sizeof(file_info_list) / sizeof(file_info_list[0])) {
        strncpy(file_info_list[num_files].filename, filename, sizeof(file_info_list[num_files].filename));
        file_info_list[num_files].size = size;
        file_info_list[num_files].last_modified = time(NULL);
        strncpy(file_info_list[num_files].uploader, uploader, sizeof(file_info_list[num_files].uploader));
        strncpy(file_info_list[num_files].last_downloader, "None", sizeof(file_info_list[num_files].last_downloader));
        file_info_list[num_files].num_downloads = 0;
        num_files++;
    } else {
        printf("Maximum number of files reached.\n");
    }
    saveFileInfoListToFile();
    
    sem_post(mutex_display);
    printf("File '%s' uploaded successfully. Upload speed: %.2f bytes/second\n", filename, upload_speed);
}

void handleRegistration(int client_socket) {
    int exceed;
    sem_wait(mutex_user);
    if(numUsers<MAX_USERS)
    {
        exceed=0;
        send(client_socket, &exceed, sizeof(int), 0);
        char username[256];
        char password[256];
        recv(client_socket, username, sizeof(username), 0);
        recv(client_socket, password, sizeof(password), 0);
        
        strcpy(userCredentials[numUsers].username, username);
        strcpy(userCredentials[numUsers].hashedPassword, password);
        numUsers++; // Increment the number of registered users

        saveCredentialsToFile();
        const char* registrationSuccessMessage = "Registration successful.";
        send(client_socket, registrationSuccessMessage, strlen(registrationSuccessMessage), 0);

    }
    else
    {
        exceed=1;
        send(client_socket, &exceed, sizeof(int), 0);
    }
    sem_post(mutex_user);
}
int authenticateAdmin(const char* username, const char* receivedHashedPassword) {
    int i;
    // Check if the client is an administrator
    if (strcmp(username, adminCredentials.username) == 0 && strcmp(receivedHashedPassword, adminCredentials.hashedPassword) == 0) {
        return 1; // Authentication successful for administrator
    }
    return 0;
}
int authenticateClient(const char* username, const char* receivedHashedPassword) {
    int i;
    sem_wait(mutex_user);
    for (i = 0; i < numUsers; ++i) {
        if (strcmp(userCredentials[i].username, username) == 0){
            if(strcmp(userCredentials[i].hashedPassword, receivedHashedPassword) == 0){
                sem_post(mutex_user);
                return 1; // Authentication successful
            }
        }
    }
    sem_post(mutex_user);
    return 0; // Authentication failed
}

// Modify the function to handle password change requests
void handleChangePassword(int client_socket,char *username,char *new_password) {
    // Code to store the new password for the client
    // ...
    int i;
    sem_wait(mutex_user);
    for (i = 0; i < numUsers; ++i) {
        if (strcmp(userCredentials[i].username, username) == 0){
            //printf("%s\n",userCredentials[i].hashedPassword);
            memset(userCredentials[i].hashedPassword, 0, sizeof(userCredentials[i].hashedPassword));
            strcpy(userCredentials[i].hashedPassword, new_password);
            printf("User %s Password changed\n",username);
            //printf("%s\n",userCredentials[i].hashedPassword);
            saveCredentialsToFile();
            const char* changeSuccessMessage = "Password changed successfully.";
            send(client_socket, changeSuccessMessage, strlen(changeSuccessMessage), 0);
            sem_post(mutex_user);
            return;
        }
    }
    sem_post(mutex_user);
    // If username not found, send an error message to the client
    const char* changeFailureMessage = "Username not found. Password change failed.";
    send(client_socket, changeFailureMessage, sizeof(changeFailureMessage), 0);
}


void* handleClient(void* arg) {

    struct ClientInfo* clientInfo = (struct ClientInfo*)arg;
    int client_socket = clientInfo->socket;

    // Receive hashed password from the client
    char username[256];
    char receivedHashedPassword[256];
    int login=0;
    char command[256];

    while(!login)
    {
        memset(command, 0, sizeof(command));
        
        recv(client_socket, command, sizeof(command), 0);
        //printf("%s\n",command);
        if(strcmp(command, "login") == 0) {
            recv(client_socket, username, sizeof(username), 0);
            //printf("%s\n",username);
            recv(client_socket, receivedHashedPassword, sizeof(receivedHashedPassword), 0);
            //printf("%s\n",receivedHashedPassword);
            int authResult=authenticateClient(username, receivedHashedPassword);
            char authResultStr[1];
            if (authResult) {
                    // Authentication successful
                    //printf("%d\n",authResult);
                    login=1;
                    authResultStr[0] = '1';
                    //printf("%s\n",authResultStr);
                    send(client_socket, authResultStr, sizeof(authResultStr), 0);
                    struct DownloadStatus download_status_list[MAX_DOWNLOADS]; // adjust as needed
                    int num_downloads = 0;
                    sem_wait(mutex_file);
                    loadDownloadStatusFromFile(username,download_status_list,&num_downloads);
                    sem_post(mutex_file);
                    while(1)
                    {
                        memset(command, 0, sizeof(command));
                        recv(client_socket, command, sizeof(command), 0);

                        if (strcmp(command, "upload") == 0) {
                            handleFileUpload(client_socket,username);
                        } 
                        else if (strcmp(command, "download") == 0) {
                            handleFileDownload(client_socket,username,download_status_list,&num_downloads);
                        } 
                        else if (strcmp(command, "list") == 0) {
                            handleFileList(client_socket);
                        }
                        else if (strcmp(command, "downloadlist") == 0) {
                            handleDownloadFileList(client_socket,download_status_list,num_downloads);
                        } 
                        else if (strcmp(command, "changepassword") == 0) {
                            char new_password[256];
                            recv(client_socket, new_password, sizeof(new_password), 0);
                            //printf("%s\n",username);
                            handleChangePassword(client_socket,username,new_password);
                        }
                        else if (strcmp(command, "remove") == 0) {
                            int ans=handleRemoveFile(client_socket,username);
                            send(client_socket, &ans, sizeof(int),0);
                        }
                        else if (strcmp(command, "exit") == 0) {
                            close(client_socket);
                            free(clientInfo);
                            pthread_exit(NULL);
                            
                        }
                        else {
                            close(client_socket);
                            free(clientInfo);
                            pthread_exit(NULL);                           
                        }

                    }
                }
            else
            {
                //printf("%d\n",authResult);
                authResultStr[0] = '0';
                send(client_socket, authResultStr, sizeof(authResultStr), 0);
            }  
        }
        else if (strcmp(command, "newregister") == 0) {
            // Handle registration request
            handleRegistration(client_socket);
        }
    }

 // Client disconnected
    close(client_socket);
    free(clientInfo);
    pthread_exit(NULL);
}

int main() {

  
    int primary_server_socket;
    struct sockaddr_in primary_server_addr;
    pthread_t primary_thread_id;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    int login=0;
    char username[256];
    char password[256];

    while(!login)
    {
        printf("Enter the admin username:");
        scanf("%s",username);

        printf("Enter the admin password:");
        scanf("%s",password);

        int authenticateResult = authenticateAdmin(username,password);

        if (authenticateResult)
        {
            login=1;
            printf("Administrator authentication successful.\n\n");
        }
        else
            printf("Authentication failed. Please try again.\n");

    }
    
    // Create socket
    primary_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (primary_server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address struct
    primary_server_addr.sin_family = AF_INET;
    primary_server_addr.sin_port = htons(12345); // Port number
    primary_server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any network interface

    // Bind the socket
    if (bind(primary_server_socket, (struct sockaddr*)&primary_server_addr, sizeof(primary_server_addr)) == -1) {
        perror("Binding failed");
        close(primary_server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(primary_server_socket, 5) == -1) {
        perror("Listening failed");
        close(primary_server_socket);
        exit(EXIT_FAILURE);
    }
    
    

    printf("Primary Server listening on port 12345...\n");
    mutex_file = sem_open (MUTEXFILE, O_CREAT, 0660, 1);
    mutex_user = sem_open (MUTEXUSER, O_CREAT, 0660, 1);
    mutex_display = sem_open (MUTEXDISPLAY, O_CREAT, 0660, 1);

    sem_wait(mutex_user);
    loadCredentialsFromFile();
    sem_post(mutex_user);

    sem_wait(mutex_display);
    loadFileInfoListFromFile();
    sem_post(mutex_display);

    while (1) {
        int primary_client_socket = accept(primary_server_socket, (struct sockaddr*)&primary_server_addr, &client_addr_len);
        if (primary_client_socket == -1) {
            perror("Acceptance failed");
            continue;
        }

        struct ClientInfo* clientInfo1 = malloc(sizeof(struct ClientInfo));
        clientInfo1->socket = primary_client_socket;

        if (pthread_create(&primary_thread_id, NULL, handleClient, (void*)clientInfo1) != 0) {
            perror("Thread creation failed");
            close(primary_client_socket);
            free(clientInfo1);
        } else {
            printf("New client connected to primary server. Thread created.\n");
        }
    }
       
    sem_close (mutex_file);
    sem_close (mutex_display);
    sem_close (mutex_user);
    
    sem_unlink (MUTEXFILE);
    sem_unlink (MUTEXDISPLAY);
    sem_unlink (MUTEXUSER);
    // Close server sockets
    close(primary_server_socket);
   // close(redundant_server_socket);

    return 0;
}
