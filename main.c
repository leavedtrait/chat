#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <ncurses.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888

WINDOW *chatwin, *inputwin, *titlewin, *statuswin;
char username[64];
int sockfd = 0;
volatile int connected = 0;
volatile int should_exit = 0;
pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;

// Clean up and exit
void cleanup(int sig) {
    should_exit = 1;
    
    if (sockfd > 0) {
        send(sockfd, "exit\n", 5, 0);
        close(sockfd);
    }
    
    pthread_mutex_lock(&ui_mutex);
    if (chatwin) delwin(chatwin);
    if (inputwin) delwin(inputwin);
    if (titlewin) delwin(titlewin);
    if (statuswin) delwin(statuswin);
    endwin();
    pthread_mutex_unlock(&ui_mutex);
    
    printf("\nDisconnected from server. Thanks for using Terminal Chat!\n");
    exit(0);
}

void update_status(const char *status, int is_error) {
    pthread_mutex_lock(&ui_mutex);
    werase(statuswin);
    if (is_error) {
        wattron(statuswin, A_BOLD);
        mvwprintw(statuswin, 0, 0, "ERROR: %s", status);
        wattroff(statuswin, A_BOLD);
    } else {
        mvwprintw(statuswin, 0, 0, "Status: %s", status);
    }
    wclrtoeol(statuswin);
    wrefresh(statuswin);
    pthread_mutex_unlock(&ui_mutex);
}

void init_ui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Check minimum terminal size
    if (LINES < 12 || COLS < 50) {
        endwin();
        printf("Terminal too small. Need at least 12 lines and 50 columns.\n");
        exit(1);
    }
    
    // Create windows
    titlewin = newwin(1, COLS, 0, 0);
    statuswin = newwin(1, COLS, 1, 0);
    chatwin = newwin(LINES - 6, COLS, 2, 0);
    inputwin = newwin(3, COLS, LINES - 3, 0);
    
    // Setup scrolling for chat window
    scrollok(chatwin, TRUE);
    
    // Draw title bar
    wbkgd(titlewin, A_REVERSE);
    mvwprintw(titlewin, 0, 0, "Socket Chat Client - User: %s - Server: %s:%d", 
              username, SERVER_IP, SERVER_PORT);
    wclrtoeol(titlewin);
    wrefresh(titlewin);
    
    // Initialize status bar
    update_status("Connecting to server...", 0);
    
    // Draw input window border
    box(inputwin, 0, 0);
    mvwprintw(inputwin, 0, 2, " Input ");
    wrefresh(inputwin);
    
    // Welcome message
    pthread_mutex_lock(&ui_mutex);
    wprintw(chatwin, "=== Socket Terminal Chat Client ===\n");
    wprintw(chatwin, "Your username: %s\n", username);
    wprintw(chatwin, "Server: %s:%d\n", SERVER_IP, SERVER_PORT);
    wprintw(chatwin, "Commands: /quit, /help, /clear, /name, /time\n");
    wprintw(chatwin, "===================================\n\n");
    wrefresh(chatwin);
    pthread_mutex_unlock(&ui_mutex);
}

void show_help() {
    pthread_mutex_lock(&ui_mutex);
    wprintw(chatwin, "--- Available Commands ---\n");
    wprintw(chatwin, "/help    - Show this help\n");
    wprintw(chatwin, "/quit    - Exit the chat\n");
    wprintw(chatwin, "/clear   - Clear chat history\n");
    wprintw(chatwin, "/name    - Change username\n");
    wprintw(chatwin, "/time    - Show current time\n");
    wprintw(chatwin, "/status  - Show connection status\n");
    wprintw(chatwin, "-------------------------\n\n");
    wrefresh(chatwin);
    pthread_mutex_unlock(&ui_mutex);
}

void change_username() {
    char new_name[64];
    
    pthread_mutex_lock(&ui_mutex);
    werase(inputwin);
    box(inputwin, 0, 0);
    mvwprintw(inputwin, 0, 2, " New Username ");
    mvwprintw(inputwin, 1, 2, "Enter new name: ");
    wrefresh(inputwin);
    
    echo();
    mvwgetnstr(inputwin, 1, 18, new_name, 50);
    noecho();
    pthread_mutex_unlock(&ui_mutex);
    
    if (strlen(new_name) > 0) {
        strncpy(username, new_name, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
        
        pthread_mutex_lock(&ui_mutex);
        wprintw(chatwin, "*** Username changed to: %s ***\n", username);
        wprintw(chatwin, "*** Note: Server still sees your original name ***\n");
        wrefresh(chatwin);
        pthread_mutex_unlock(&ui_mutex);
        
        // Update title bar
        pthread_mutex_lock(&ui_mutex);
        werase(titlewin);
        wbkgd(titlewin, A_REVERSE);
        mvwprintw(titlewin, 0, 0, "Socket Chat Client - User: %s - Server: %s:%d", 
                  username, SERVER_IP, SERVER_PORT);
        wclrtoeol(titlewin);
        wrefresh(titlewin);
        pthread_mutex_unlock(&ui_mutex);
    } else {
        pthread_mutex_lock(&ui_mutex);
        wprintw(chatwin, "*** Username unchanged ***\n");
        wrefresh(chatwin);
        pthread_mutex_unlock(&ui_mutex);
    }
}

void show_time() {
    time_t now = time(NULL);
    char *timestr = ctime(&now);
    timestr[strlen(timestr) - 1] = '\0'; // Remove newline
    
    pthread_mutex_lock(&ui_mutex);
    wprintw(chatwin, "*** Current time: %s ***\n", timestr);
    wrefresh(chatwin);
    pthread_mutex_unlock(&ui_mutex);
}

void show_status() {
    pthread_mutex_lock(&ui_mutex);
    wprintw(chatwin, "*** Connection Status ***\n");
    wprintw(chatwin, "Server: %s:%d\n", SERVER_IP, SERVER_PORT);
    wprintw(chatwin, "Socket FD: %d\n", sockfd);
    wprintw(chatwin, "Connected: %s\n", connected ? "Yes" : "No");
    wprintw(chatwin, "Username: %s\n", username);
    wprintw(chatwin, "************************\n");
    wrefresh(chatwin);
    pthread_mutex_unlock(&ui_mutex);
}

int process_command(char *msg) {
    if (strcmp(msg, "/help") == 0) {
        show_help();
        return 0;
    } else if (strcmp(msg, "/quit") == 0) {
        return 1; // Signal to quit
    } else if (strcmp(msg, "/clear") == 0) {
        pthread_mutex_lock(&ui_mutex);
        werase(chatwin);
        wprintw(chatwin, "*** Chat cleared ***\n\n");
        wrefresh(chatwin);
        pthread_mutex_unlock(&ui_mutex);
        return 0;
    } else if (strcmp(msg, "/name") == 0) {
        change_username();
        return 0;
    } else if (strcmp(msg, "/time") == 0) {
        show_time();
        return 0;
    } else if (strcmp(msg, "/status") == 0) {
        show_status();
        return 0;
    } else {
        pthread_mutex_lock(&ui_mutex);
        wprintw(chatwin, "*** Unknown command: %s (type /help for commands) ***\n", msg);
        wrefresh(chatwin);
        pthread_mutex_unlock(&ui_mutex);
        return 0;
    }
}

void *receive_messages(void *arg) {
    char buffer[BUFFER_SIZE];
    int bytes_received;
    
    while (!should_exit && connected) {
        bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                update_status("Server disconnected", 1);
            } else {
                update_status("Connection error", 1);
            }
            connected = 0;
            break;
        }
        
        buffer[bytes_received] = '\0';
        
        // Remove trailing newline if present
        if (buffer[bytes_received - 1] == '\n') {
            buffer[bytes_received - 1] = '\0';
        }
        
        pthread_mutex_lock(&ui_mutex);
        wprintw(chatwin, "%s\n", buffer);
        wrefresh(chatwin);
        pthread_mutex_unlock(&ui_mutex);
    }
    
    return NULL;
}

int connect_to_server() {
    struct sockaddr_in server_addr;
    
    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        update_status("Failed to create socket", 1);
        return 0;
    }
    
    // Setup server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        update_status("Invalid server address", 1);
        close(sockfd);
        return 0;
    }
    
    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        update_status("Failed to connect to server", 1);
        close(sockfd);
        return 0;
    }
    
    connected = 1;
    update_status("Connected successfully", 0);
    return 1;
}

int main() {
    // Setup signal handler
    signal(SIGINT, cleanup);
    
    // Get username
    printf("Enter your username (2-32 characters): ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0; // Remove newline
    
    if (strlen(username) < 2 || strlen(username) > 32) {
        printf("Invalid username length. Must be 2-32 characters.\n");
        return 1;
    }
    
    // Initialize UI
    init_ui();
    
    // Connect to server
    if (!connect_to_server()) {
        pthread_mutex_lock(&ui_mutex);
        wprintw(chatwin, "Failed to connect to server %s:%d\n", SERVER_IP, SERVER_PORT);
        wprintw(chatwin, "Make sure the server is running and try again.\n");
        wprintw(chatwin, "Press any key to exit...\n");
        wrefresh(chatwin);
        pthread_mutex_unlock(&ui_mutex);
        getch();
        cleanup(0);
    }
    
    // Send username to server (for Go server compatibility)
    char username_msg[128];
    snprintf(username_msg, sizeof(username_msg), "%s\n", username);
    send(sockfd, username_msg, strlen(username_msg), 0);
    
    // Start receive thread
    pthread_t receive_thread;
    if (pthread_create(&receive_thread, NULL, receive_messages, NULL) != 0) {
        update_status("Failed to create receive thread", 1);
        cleanup(0);
    }
    
    update_status("Connected and ready", 0);
    
    char msg[256];
    
    while (!should_exit && connected) {
        // Clear and redraw input window
        pthread_mutex_lock(&ui_mutex);
        werase(inputwin);
        box(inputwin, 0, 0);
        mvwprintw(inputwin, 0, 2, " Input ");
        mvwprintw(inputwin, 1, 2, "%s> ", username);
        wrefresh(inputwin);
        
        // Get input
        echo();
        mvwgetnstr(inputwin, 1, strlen(username) + 4, msg, 200);
        noecho();
        pthread_mutex_unlock(&ui_mutex);
        
        // Skip empty messages
        if (strlen(msg) == 0) {
            continue;
        }
        
        // Process commands or send messages
        if (msg[0] == '/') {
            if (process_command(msg)) {
                break; // Quit command
            }
        } else {
            // Send message to server
            char full_msg[300];
            snprintf(full_msg, sizeof(full_msg), "%s\n", msg);
            
            if (send(sockfd, full_msg, strlen(full_msg), 0) < 0) {
                update_status("Failed to send message", 1);
                if (errno == EPIPE || errno == ECONNRESET) {
                    connected = 0;
                    break;
                }
            }
        }
    }
    
    cleanup(0);
    return 0;
}

/*
COMPILATION AND USAGE:

1. Compile:
   gcc -o socket_chat_client socket_chat_client.c -lncurses -lpthread

2. Start the Go server (or C server):
   go run main.go

3. Run the client:
   ./socket_chat_client

FEATURES:
- Connects to socket-based chat servers
- Real-time message receiving in separate thread
- Enhanced UI with status bar
- Thread-safe UI updates
- Local commands (/help, /quit, /clear, etc.)
- Connection status monitoring
- Graceful error handling
- Compatible with both Go and C servers

NEW SOCKET FEATURES:
- TCP socket connection to server
- Automatic username transmission
- Real-time message receiving thread
- Connection status monitoring
- Error handling for network issues
- Clean disconnection handling
*/