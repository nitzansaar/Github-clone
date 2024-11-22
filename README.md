# Distributed File Management System

A distributed system that enables file management across multiple servers with authentication, file operations, and deployment capabilities.

## System Architecture

The system consists of multiple interconnected servers:
- **Main Server (ServerM)**: Central coordinator handling client requests and routing
- **Authentication Server (ServerA)**: Manages user authentication
- **Repository Server (ServerR)**: Handles file storage and management
- **Deployment Server (ServerD)**: Manages file deployment operations

## Features

- User Authentication (Member and Guest access)
- File Operations:
  - Lookup: View files associated with users
  - Push: Add new files to the repository
  - Deploy: Deploy files to production
  - Remove: Delete files from the repository
- Operation Logging
- File Overwrite Protection
- Encrypted Password Storage

## Technical Details

- Written in C
- Uses UDP for inter-server communication
- Uses TCP for client-server communication
- Implements socket programming
- File-based storage for user data and files

## Building and Running

1. Compile all servers and client:
clang -o serverM serverM.c
clang -o serverA serverA.c
clang -o serverR serverR.c
clang -o serverD serverD.c
clang -o client client.c


2. Start the servers in order:
./serverM
./serverA
./serverR
./serverD

3. Run the client:
./client <username> <password>


## Available Commands

After authentication, users can use the following commands:
- `lookup <username>`: View files associated with a user
- `push <filename>`: Add a new file to the repository
- `deploy`: Deploy all files to production
- `remove <filename>`: Remove a file from the repository
- `log`: View operation history

## What I Learned

1. **Distributed Systems Architecture**
   - Understanding how multiple servers can work together
   - Handling inter-process communication
   - Managing distributed state

2. **Network Programming**
   - TCP vs UDP implementation differences
   - Socket programming in C
   - Network error handling
   - Protocol design for client-server communication

3. **Security Concepts**
   - User authentication systems
   - Password encryption/decryption
   - Session management
   - Access control (guest vs member permissions)

4. **System Design**
   - Separation of concerns across servers
   - Stateful vs stateless operations
   - Error handling across distributed systems
   - Logging and monitoring

5. **C Programming**
   - File I/O operations
   - String manipulation
   - Memory management
   - Struct usage for data organization
   - Socket API implementation

6. **Software Engineering Practices**
   - Code organization
   - Error handling patterns
   - Logging implementation
   - Protocol design

