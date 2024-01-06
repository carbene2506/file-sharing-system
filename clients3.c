#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <crypt.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<pthread.h>
#include <netinet/tcp.h>

#define STORAGE_DIRECTORY "./clientfiles/"
char salt[] = "12";
struct FileInfo {
    char filename[256];
    long size;
    time_t last_modified;
    char uploader[256];
    char last_downloader[256];
    int num_downloads;
};
struct DownloadStatus {
    char filename[256];
    double download_progress;
};
void displayMenu() {
    printf("\n----- Menu -----\n");
    printf("1. Download File\n");
    printf("2. Upload File\n");
    printf("3. Display All Files\n");
    printf("4. Display my Uploaded Files\n");
    printf("5. Remove File\n");
    printf("6. Check Download History\n");
    printf("7. Manage Account (Change Password)\n");
    printf("8. Exit\n");
    printf("Enter your choice: ");
}
void displayLog()
{
    printf("\n-------Welcome to the File Sharing System------\n");
    printf("1. Login\n");
    printf("2. Create an Account\n");
    printf("3. Exit\n");
    printf("Enter your choice: ");
}


void hPassword(const char* password, char* hashedPassword) {
    char* hashed = crypt(password, salt);
    strcpy(hashedPassword, hashed);
}

int isValidUsername(const char* username) {
   
    for (int i = 0; username[i] != '\0'; i++) {
        if (!isalnum(username[i])) {
            return 0; 
        }
    }
    return 1; 
}
void registerNewAccount(int client_socket) {
    char username[256];
    char newPassword[256];
    char hashedPassword[256];
    char salt[256];
    int exceed;
    recv(client_socket, &exceed, sizeof(int), 0);
    if(!exceed)
    {
        int valid=0;

        while(!valid)
        {
            printf("Enter your new username: ");
            scanf("%s", username);
            
            if (!isValidUsername(username)) {
                printf("Invalid username format. Please use alphanumeric characters only.\n");
            }
            else
                valid=1;

        }
        
        printf("Enter your new password: ");
        scanf("%s", newPassword);

        hPassword(newPassword, hashedPassword);
        // Send username and hashed password to the server for registration
        send(client_socket, username, sizeof(username), 0);
       
        send(client_socket, hashedPassword, sizeof(hashedPassword), 0);
        char registrationResult[256];
        recv(client_socket, registrationResult, sizeof(registrationResult), 0);
        printf("%s\n", registrationResult);

    }
    else
        printf("Maximum user limit exceeded.No more new registrations are possible!!!\n");
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    struct timeval timeout;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345); // Server port number
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Server IP address

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    char username[256];
    char salt[256];
    char password[256];
    char hashedPassword[256];
    char newPassword[256];
    char msg[256];
    int choice;
    int loggedIn = 0;
    char command[256];

    while (!loggedIn) {
        memset(command, 0, sizeof(command));
        displayLog();
        scanf("%d", &choice);
        switch(choice)
        {
            case 1: strcpy(command, "login");
                    send(client_socket, command, strlen(command), 0);
                    printf("Enter your username: ");
                    scanf("%s", username);
                    send(client_socket, username, sizeof(username), 0);
                    printf("Enter your password: ");
                    scanf("%s", password);
                    hPassword(password, hashedPassword);
                    send(client_socket, hashedPassword, sizeof(hashedPassword), 0);
                    char authResult[1];
                    recv(client_socket,authResult,sizeof(authResult),0);
                    
                    if (authResult[0] == '1') {
                        printf("Login successful!\n");
                        loggedIn = 1;
                    } else {
                        printf("Authentication failed. Please try again.\n");
                    }
                    break;
            case 2: strcpy(command, "newregister");
                    send(client_socket, command, sizeof(command), 0);
                    registerNewAccount(client_socket);
                    break;
            case 3: 
                    close(client_socket);
                    exit(EXIT_SUCCESS);
            default: printf("Invalid choice. Please try again.\n");
        }
    }
    if (loggedIn) {
        char downloadCommand[256] = "download";
        char uploadCommand[256] = "upload";
        char listCommand[256] = "list";
        char downloadListCommand[256] = "downloadlist";
        char removeCommand[256] = "remove";
        char changePasswordCommand[256]="changepassword";
        char exitCommand[256]="exit";
        char filename[256];
        char* buffer;
        char full_path[512];
        char newPassword[256];
        int num_files;
        int num_downloads;
        int error;
        int download=0;
        int upload=0;
        while (1) {
            memset(full_path, 0, sizeof(full_path));
            displayMenu();
            scanf("%d", &choice);

            switch (choice) {
                case 1: download=0;
                        send(client_socket, downloadCommand, sizeof(downloadCommand), 0);
                        while(!download)
                        {
                            // Logic for downloading file
                            printf("Enter the filename to download: ");
                            scanf("%s", filename);
                            send(client_socket, filename, sizeof(filename),0);
                            char errorMessage[1];
                            recv(client_socket, errorMessage, sizeof(errorMessage), 0);

                            if (errorMessage[0] == '0') {
                                // File not found on the server, handle this case
                                printf("File not found on the server.\n");
                            }
                            else
                            {
                                download=1;
                            }

                        }
                        snprintf(full_path, sizeof(full_path), "%s%s", STORAGE_DIRECTORY, filename);
                        
                        clock_t start_time = clock();
                        clock_t time_start=start_time;

                        // Receive the file size
                        long file_size;
                        recv(client_socket, &file_size, sizeof(long),0);
                        long size=file_size;

                        // Check if the file already exists for resumable download
                        long start_point = 0;
                        FILE* file = fopen(full_path, "ab");
                        if (file != NULL) {
                            fseek(file, 0, SEEK_END);
                            start_point = ftell(file);
                            fclose(file);
                        }
                        file_size -= start_point;
                        // Send the starting point to the server
                        send(client_socket, &start_point, sizeof(long), 0);

                        // Open the file for writing (append mode)
                        file = fopen(full_path, "ab");
                        if (file == NULL) {
                            perror("Error opening file for writing");
                        }

                        // Allocate buffer dynamically based on file size
                        buffer = (char *)malloc(file_size);
                        if (buffer == NULL) {
                            perror("Error allocating memory for buffer");
                            fclose(file);
                            return 1;
                        }

                        int bytes_received;
                        double download_percentage = 0.0;

                        while (file_size > 0) {
                            bytes_received = recv(client_socket, buffer, sizeof(buffer),0);
                            fwrite(buffer, 1, bytes_received, file);
                            file_size -= bytes_received;

                            // Calculate and display download speed every 1 second
                            clock_t current_time = clock();
                            double elapsed_time = (double)(current_time - start_time) / CLOCKS_PER_SEC;
                            if (elapsed_time >= 1.0) {
                                double time_elapsed=(double)(current_time - time_start) / CLOCKS_PER_SEC;
                                double download_speed = ftell(file) / time_elapsed;
                                download_percentage = ((double)ftell(file) / (double)(size)) * 100.0;
                                printf("Download Speed: %.2f bytes/second\n", download_speed);
                                printf("Download Percentage: %.2f%%\n",download_percentage);
                                start_time = clock();
                            }
                        }
                        clock_t end_time = clock();

                        // Calculate and display download speed
                        double download_time = (double)(end_time - time_start) / CLOCKS_PER_SEC;
                        double download_speed = size / download_time;

                        printf("File '%s' downloaded successfully. Download speed: %.2f bytes/second\n", filename, download_speed);

                        free(buffer);
                        fclose(file);
                        break;

                case 2:
                    
                    upload=0;
                    send(client_socket, uploadCommand, sizeof(uploadCommand), 0);
                    while(!upload)
                    {
                        memset(full_path, 0, sizeof(full_path));
                        printf("Enter the filename to upload: ");
                        scanf("%s", filename);
                        snprintf(full_path, sizeof(full_path), "%s%s", STORAGE_DIRECTORY, filename);
                        if (access(full_path, F_OK) == -1) {
                            // File does not exist, send an error message to the client
                            printf("File '%s' not found.\n", filename);
                        }
                        else
                        {
                            upload=1;
                            send(client_socket, filename, sizeof(filename),0);
                        }
                    }
                    recv(client_socket, &error, sizeof(int), 0);
                    if(!error)
                    {
                        clock_t start_time = clock();
                        clock_t time_start=start_time;

                        // Send the file size first
                        FILE *file1 = fopen(full_path, "rb");
                        if (file1 == NULL) {
                            perror("Error opening file");
                        }

                        fseek(file1, 0, SEEK_END);
                        long file_size1 = ftell(file1);
                        fseek(file1, 0, SEEK_SET);

                        send(client_socket, &file_size1, sizeof(long),0);

                        // Allocate buffer dynamically based on file size
                        buffer = (char *)malloc(file_size1);
                        if (buffer == NULL) {
                            perror("Error allocating memory for buffer");
                            fclose(file1);
                            return 1;
                        }

                        int bytes_read;
                        double upload_percentage = 0.0;
                        
                        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file1)) > 0) {
                            send(client_socket, buffer, bytes_read,0);

                            // Calculate and display upload speed every 1 second
                            clock_t current_time = clock();
                            double elapsed_time = (double)(current_time - start_time) / CLOCKS_PER_SEC;
                            if (elapsed_time >= 1.0) {
                                double time_elapsed=(double)(current_time - time_start) / CLOCKS_PER_SEC;
                                double upload_speed = ftell(file1) / time_elapsed;
                                upload_percentage = ((double)ftell(file1) / (double)(file_size1)) * 100.0;
                                printf("Upload Percentage: %.2f%%\n",upload_percentage);
                                printf("Upload Speed: %.2f bytes/second\n", upload_speed);
                                start_time = clock();
                            }
                        }

                        clock_t end_time = clock();

                        // Calculate and display upload speed
                        double upload_time = (double)(end_time - time_start) / CLOCKS_PER_SEC;
                        double upload_speed = file_size1 / upload_time;

                        printf("File '%s' uploaded successfully. Upload speed: %.2f bytes/second\n", filename, upload_speed);

                        free(buffer);
                        fclose(file1);

                    }
                    else
                        printf("File '%s' already exists in server.Rename the file to upload!\n", filename);

                    break;
                case 3:
                        // Send the list command to the server
                        send(client_socket, listCommand, sizeof(listCommand), 0);
                        recv(client_socket, &num_files, sizeof(int), 0);
                        printf("\n");
                        printf("%-15s | %-10s | %-20s | %-15s | %-15s | %-10s\n",
                                   "File Name", "Size (bytes)", "Last Modified", "Uploader", "Last Downloader", "Downloads");
                        printf("---------------------------------------------------------------------------------------------------------\n");

                        // Receive and print individual FileInfo structures
                        for (int i = 0; i < num_files; i++) {
                            struct FileInfo file_info;
                            bytes_received = recv(client_socket, &file_info, sizeof(struct FileInfo), 0);

                            if (bytes_received <= 0) {
                                perror("Error receiving file information");
                                break;
                            }
                            char* timestamp = ctime(&file_info.last_modified);
                            timestamp[strcspn(timestamp, "\n")] = '\0';  // Remove the newline character

                            printf("%-15s | %-10ld | %-20s | %-15s | %-15s | %-10d\n",
                                   file_info.filename, file_info.size, timestamp,
                                   file_info.uploader, file_info.last_downloader, file_info.num_downloads);

                        }
                    break;
                case 4: 
                        // Send the list command to the server
                        send(client_socket, listCommand, sizeof(listCommand), 0);
                        recv(client_socket, &num_files, sizeof(int), 0);
                        printf("%-15s | %-10s | %-20s | %-15s | %-15s | %-10s\n",
                                   "File Name", "Size (bytes)", "Last Modified", "Uploader", "Last Downloader", "Downloads");
                        printf("--------------------------------------------------------------------------------------------------------\n");

                        // Receive and print individual FileInfo structures
                        for (int i = 0; i < num_files; i++) {
                            struct FileInfo file_info;
                            bytes_received = recv(client_socket, &file_info, sizeof(struct FileInfo), 0);

                            if (bytes_received <= 0) {
                                perror("Error receiving file information");
                                break;
                            }
                            if(strcmp(file_info.uploader,username)==0)
                            {
                                char* timestamp = ctime(&file_info.last_modified);
                                timestamp[strcspn(timestamp, "\n")] = '\0';  // Remove the newline character

                                printf("%-15s | %-10ld | %-20s | %-15s | %-15s | %-10d\n",
                                       file_info.filename, file_info.size, timestamp,
                                       file_info.uploader, file_info.last_downloader, file_info.num_downloads);

                            }
                            else
                                continue;
                        }
                        break;
                case 5: 
                        send(client_socket, removeCommand, sizeof(removeCommand), 0);
                           
                        printf("Enter the filename to remove(Only files uploaded by you can be removed): ");
                        scanf("%s", filename);
                        send(client_socket, filename, sizeof(filename),0);
                        recv(client_socket, &error, sizeof(int), 0);

                        if (!error) {
                           
                            printf("File not found on the server or is not uploaded by you.\n");
                        }
                        else{
                            printf("File %s removed successfully\n",filename);
                        }
                        break;

                case 6: // Send the list command to the server
                        send(client_socket, downloadListCommand, sizeof(downloadListCommand), 0);
                        recv(client_socket, &num_downloads, sizeof(int), 0);
                        printf("\n");
                        printf("%-20s | %-10s\n","File Name", "Download Status(in %)");
                        printf("--------------------------------------------------\n");

                        // Receive and print individual FileInfo structures
                        for (int i = 0; i < num_downloads; i++) {
                            struct DownloadStatus download_info;
                            bytes_received = recv(client_socket, &download_info, sizeof(struct DownloadStatus), 0);

                            if (bytes_received <= 0) {
                                perror("Error receiving file information");
                                break;
                            }
                            printf("%-20s | %-.1f%%\n",download_info.filename, download_info.download_progress);

                        }
                        break;
                case 7:
                    // Logic for managing account (change password)
                
                    send(client_socket, changePasswordCommand, sizeof(changePasswordCommand), 0);
                    // Get the new password from the user and hash it for authentication
                    printf("Enter your new password: ");
                    scanf("%s", newPassword);
                    hPassword(newPassword, hashedPassword);

                    // Send hashed new password to the server for modification
                    send(client_socket, hashedPassword, sizeof(hashedPassword), 0);
                    char changeResult[256];
                    recv(client_socket, changeResult, sizeof(changeResult), 0);
                    printf("%s\n", changeResult);
                   
                    break;
                case 8:
                    // Exit the program
                    send(client_socket, exitCommand, sizeof(exitCommand), 0);
                    close(client_socket);
                    //close(client_socket_redundant);
                    exit(EXIT_SUCCESS);
                default:
                    printf("Invalid choice. Please try again.\n");
            }
        }
        
    }

return 0;
}
