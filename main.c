#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <ncurses.h>

WINDOW *chatwin, *inputwin, *titlewin;
char username[64];

// Clean up and exit
void cleanup(int sig) {
    if (chatwin) delwin(chatwin);
    if (inputwin) delwin(inputwin);
    if (titlewin) delwin(titlewin);
    endwin();
    printf("\nExited chat. Thanks for using Terminal Chat!\n");
    exit(0);
}

void init_ui() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    
    // Check minimum terminal size
    if (LINES < 10 || COLS < 40) {
        endwin();
        printf("Terminal too small. Need at least 10 lines and 40 columns.\n");
        exit(1);
    }
    
    // Create windows
    titlewin = newwin(1, COLS, 0, 0);
    chatwin = newwin(LINES - 4, COLS, 1, 0);
    inputwin = newwin(3, COLS, LINES - 3, 0);
    
    // Setup scrolling for chat window
    scrollok(chatwin, TRUE);
    
    // Draw title bar
    wbkgd(titlewin, COLOR_PAIR(1));
    mvwprintw(titlewin, 0, 0, "Terminal Chat - Type '/quit' to exit, '/help' for commands");
    wrefresh(titlewin);
    
    // Draw input window border
    box(inputwin, 0, 0);
    mvwprintw(inputwin, 0, 2, " Input ");
    wrefresh(inputwin);
    
    // Welcome message
    wprintw(chatwin, "=== Welcome to Terminal Chat ===\n");
    wprintw(chatwin, "Your username: %s\n", username);
    wprintw(chatwin, "Type '/help' for available commands\n");
    wprintw(chatwin, "================================\n\n");
    wrefresh(chatwin);
}

void show_help() {
    wprintw(chatwin, "--- Available Commands ---\n");
    wprintw(chatwin, "/help    - Show this help\n");
    wprintw(chatwin, "/quit    - Exit the chat\n");
    wprintw(chatwin, "/clear   - Clear chat history\n");
    wprintw(chatwin, "/name    - Change username\n");
    wprintw(chatwin, "/time    - Show current time\n");
    wprintw(chatwin, "-------------------------\n\n");
    wrefresh(chatwin);
}

void change_username() {
    char new_name[64];
    werase(inputwin);
    box(inputwin, 0, 0);
    mvwprintw(inputwin, 0, 2, " New Username ");
    mvwprintw(inputwin, 1, 2, "Enter new name: ");
    wrefresh(inputwin);
    
    echo();
    mvwgetnstr(inputwin, 1, 18, new_name, 50);
    noecho();
    
    if (strlen(new_name) > 0) {
        strncpy(username, new_name, sizeof(username) - 1);
        username[sizeof(username) - 1] = '\0';
        wprintw(chatwin, "*** Username changed to: %s ***\n", username);
    } else {
        wprintw(chatwin, "*** Username unchanged ***\n");
    }
    wrefresh(chatwin);
}

void show_time() {
    time_t now = time(NULL);
    char *timestr = ctime(&now);
    timestr[strlen(timestr) - 1] = '\0'; // Remove newline
    wprintw(chatwin, "*** Current time: %s ***\n", timestr);
    wrefresh(chatwin);
}

void process_command(char *msg) {
    if (strcmp(msg, "/help") == 0) {
        show_help();
    } else if (strcmp(msg, "/quit") == 0) {
        cleanup(0);
    } else if (strcmp(msg, "/clear") == 0) {
        werase(chatwin);
        wprintw(chatwin, "*** Chat cleared ***\n\n");
        wrefresh(chatwin);
    } else if (strcmp(msg, "/name") == 0) {
        change_username();
    } else if (strcmp(msg, "/time") == 0) {
        show_time();
    } else {
        wprintw(chatwin, "*** Unknown command: %s (type /help for commands) ***\n", msg);
        wrefresh(chatwin);
    }
}

int main() {
    // Seed RNG and setup signal handler
    srand(time(NULL));
    signal(SIGINT, cleanup);
    
    // Generate username
    snprintf(username, sizeof(username), "anon%d", rand() % 100000);
    
    // Initialize UI
    init_ui();
    
    char msg[256];
    
    while (1) {
        // Clear and redraw input window
        werase(inputwin);
        box(inputwin, 0, 0);
        mvwprintw(inputwin, 0, 2, " Input ");
        mvwprintw(inputwin, 1, 2, "%s> ", username);
        wrefresh(inputwin);
        
        // Get input
        echo();
        mvwgetnstr(inputwin, 1, strlen(username) + 4, msg, 200);
        noecho();
        
        // Skip empty messages
        if (strlen(msg) == 0) {
            continue;
        }
        
        // Process commands or regular messages
        if (msg[0] == '/') {
            process_command(msg);
        } else {
            // Get timestamp
            time_t now = time(NULL);
            struct tm *tm_info = localtime(&now);
            char timestamp[20];
            strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
            
            // Display message with timestamp
            wprintw(chatwin, "[%s] %s: %s\n", timestamp, username, msg);
            wrefresh(chatwin);
        }
    }
    
    return 0;
}