package main

import (
	"bufio"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"strings"
	"sync"
	"syscall"
	"time"
)

const (
	PORT        = ":8888"
	MAX_CLIENTS = 50
)

type Client struct {
	conn     net.Conn
	name     string
	messages chan string
}

type ChatServer struct {
	clients    map[*Client]bool
	broadcast  chan string
	register   chan *Client
	unregister chan *Client
	mutex      sync.RWMutex
}

func NewChatServer() *ChatServer {
	return &ChatServer{
		clients:    make(map[*Client]bool),
		broadcast:  make(chan string),
		register:   make(chan *Client),
		unregister: make(chan *Client),
	}
}

func (server *ChatServer) run() {
	for {
		select {
		case client := <-server.register:
			server.mutex.Lock()
			server.clients[client] = true
			server.mutex.Unlock()
			
			// Send welcome message
			joinMsg := fmt.Sprintf("*** %s has joined the chat ***", client.name)
			log.Println(joinMsg)
			server.broadcast <- joinMsg
			
			// Send user list
			server.sendUserList()

		case client := <-server.unregister:
			server.mutex.Lock()
			if _, ok := server.clients[client]; ok {
				delete(server.clients, client)
				close(client.messages)
				client.conn.Close()
			}
			server.mutex.Unlock()
			
			// Send leave message
			leaveMsg := fmt.Sprintf("*** %s has left the chat ***", client.name)
			log.Println(leaveMsg)
			server.broadcast <- leaveMsg
			
			// Send updated user list
			server.sendUserList()

		case message := <-server.broadcast:
			server.mutex.RLock()
			for client := range server.clients {
				select {
				case client.messages <- message:
				default:
					// Client's message channel is full, remove client
					delete(server.clients, client)
					close(client.messages)
					client.conn.Close()
				}
			}
			server.mutex.RUnlock()
		}
	}
}

func (server *ChatServer) sendUserList() {
	server.mutex.RLock()
	var users []string
	for client := range server.clients {
		users = append(users, client.name)
	}
	server.mutex.RUnlock()
	
	if len(users) > 0 {
		userList := fmt.Sprintf("*** Online users: %s ***", strings.Join(users, ", "))
		server.broadcast <- userList
	}
}

func (server *ChatServer) handleClient(conn net.Conn) {
	defer conn.Close()
	
	// Get username
	reader := bufio.NewReader(conn)
	conn.Write([]byte("Enter your username: "))
	
	name, err := reader.ReadString('\n')
	if err != nil {
		log.Printf("Error reading username: %v", err)
		return
	}
	
	name = strings.TrimSpace(name)
	if len(name) < 2 || len(name) > 32 {
		conn.Write([]byte("Username must be 2-32 characters.\n"))
		return
	}
	
	// Create client
	client := &Client{
		conn:     conn,
		name:     name,
		messages: make(chan string, 256),
	}
	
	// Check max clients
	server.mutex.RLock()
	clientCount := len(server.clients)
	server.mutex.RUnlock()
	
	if clientCount >= MAX_CLIENTS {
		conn.Write([]byte("Server is full. Try again later.\n"))
		return
	}
	
	// Register client
	server.register <- client
	
	// Send welcome message to client
	welcomeMsg := fmt.Sprintf("=== Welcome to Go Chat Server ===\nYour username: %s\nType 'exit' to quit\n===================================\n\n", name)
	client.messages <- welcomeMsg
	
	// Start goroutines for reading and writing
	go server.writePump(client)
	go server.readPump(client)
	
	// Keep connection alive until client disconnects
	select {}
}

func (server *ChatServer) readPump(client *Client) {
	defer func() {
		server.unregister <- client
	}()
	
	reader := bufio.NewReader(client.conn)
	
	for {
		message, err := reader.ReadString('\n')
		if err != nil {
			log.Printf("Error reading from client %s: %v", client.name, err)
			break
		}
		
		message = strings.TrimSpace(message)
		
		if message == "exit" {
			break
		}
		
		if len(message) > 0 {
			// Add timestamp and format message
			timestamp := time.Now().Format("15:04:05")
			formattedMsg := fmt.Sprintf("[%s] %s: %s", timestamp, client.name, message)
			
			log.Println(formattedMsg)
			server.broadcast <- formattedMsg
		}
	}
}

func (server *ChatServer) writePump(client *Client) {
	defer client.conn.Close()
	
	for {
		select {
		case message, ok := <-client.messages:
			if !ok {
				return
			}
			
			if _, err := client.conn.Write([]byte(message + "\n")); err != nil {
				log.Printf("Error writing to client %s: %v", client.name, err)
				return
			}
		}
	}
}

func main() {
	// Create server
	server := NewChatServer()
	
	// Handle graceful shutdown
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)
	go func() {
		<-c
		fmt.Println("\nShutting down server...")
		os.Exit(0)
	}()
	
	// Start server
	go server.run()
	
	// Listen for connections
	listener, err := net.Listen("tcp", PORT)
	if err != nil {
		log.Fatal("Error starting server:", err)
	}
	defer listener.Close()
	
	fmt.Printf("=== GO CHAT SERVER STARTED ===\n")
	fmt.Printf("Listening on port %s\n", PORT)
	fmt.Printf("Waiting for connections...\n")
	fmt.Printf("Press Ctrl+C to stop\n\n")
	
	for {
		conn, err := listener.Accept()
		if err != nil {
			log.Printf("Error accepting connection: %v", err)
			continue
		}
		
		log.Printf("New connection from: %s", conn.RemoteAddr())
		go server.handleClient(conn)
	}
}

/*
USAGE:

1. Save as main.go

2. Run the server:
   go run main.go

3. Connect using telnet or netcat:
   telnet localhost 8888
   # or
   nc localhost 8888

4. Or use the C client from previous example:
   ./chat_client

FEATURES:
- Concurrent client handling with goroutines
- Thread-safe client management with channels
- Real-time message broadcasting
- User join/leave notifications
- Online user list updates
- Graceful shutdown handling
- Connection limit (50 clients)
- Message timestamps
- Clean error handling

GO ADVANTAGES:
- Built-in concurrency with goroutines
- Channels for safe communication
- Automatic garbage collection
- Simple HTTP server capabilities
- Cross-platform compilation
- Rich standard library
*/