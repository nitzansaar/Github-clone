# EE450 Socket Programming Project (Github Code Management Platform)

## Personal Information
- **Full Name:** Nitzan Saar
- **Student ID:** 8106373693

## Project Overview
I have completed the main requirements of the project, including extra credit, which includes:
- Implementation of a distributed file management system with authentication
- Support for both member and guest access
- File operations (lookup, push, remove)
- Deployment functionality
- Operation logging

## Code Files
1. **serverM.c**
   - Main server coordinating all operations
   - Handles client connections and routes requests
   - Manages communication with other servers
   - Implements thread-safe logging
   - Supports both TCP (for clients) and UDP (for backend servers)
   - Handles authentication, file operations, and deployment requests

2. **serverA.c**
   - Authentication server
   - Handles user credential verification
   - Manages encrypted password storage
   - Implements Caesar cipher decryption
   - Supports both member and guest authentication
   - Reads from members.txt and original.txt for credential verification

3. **serverR.c**
   - Repository server
   - Manages file storage and operations
   - Handles file lookup, push, and remove requests
   - Maintains filenames.txt database
   - Supports file overwrite confirmation
   - Implements concurrent file access safety

4. **serverD.c**
   - Deployment server
   - Manages file deployment operations
   - Maintains deployment logs in deployed.txt
   - Coordinates with serverR for file information
   - Timestamps all deployments
   - Provides detailed deployment status reports

5. **client.c**
   - Client application
   - Handles user interaction
   - Manages connection with main server using dynamic ports:
     - System automatically assigns available ports for each connection
     - Uses getsockname() to retrieve the assigned port number
     - Supports multiple concurrent client connections
     - Different ports for member (25693) and guest (25694) connections
   - Supports all file operations
   - Implements guest and member modes
   - Provides colored output for better readability
   - Reports port numbers in operation logs for debugging

6. **Supporting Files**
   - **members.txt**: Encrypted user credentials
   - **original.txt**: Unencrypted user credentials
   - **filenames.txt**: Repository file database
   - **deployed.txt**: Deployment history log
   - **server_logs.txt**: Operation history log
   - **Makefile**: Build and management scripts

## Message Formats

### Authentication Messages
- Client → ServerM: `AUTH username password`
- ServerM → ServerA: `username password`
- ServerA → ServerM: `[MEMBER_AUTH_SUCCESS|GUEST_AUTH_SUCCESS|AUTH_FAILED]`

### File Operation Messages
1. **Lookup**
   - Client → ServerM: `LOOKUP username`
   - ServerM → ServerR: `LOOKUP username requesting_user`
   - ServerR → ServerM: `[file list or error message]`

2. **Push**
   - Client → ServerM: `PUSH filename`
   - ServerM → ServerR: `PUSH username filename`
   - ServerR → ServerM: `[success message or OVERWRITE_CONFIRM]`

3. **Deploy**
   - Client → ServerM: `DEPLOY`
   - ServerM → ServerD: `DEPLOY username`
   - ServerD → ServerM: `[deployment status message]`

4. **Remove**
   - Client → ServerM: `REMOVE filename`
   - ServerM → ServerR: `REMOVE username filename`
   - ServerR → ServerM: `[success or error message]`

5. **Log**
   - Client → ServerM: `LOG`
   - ServerM → Client: `[operation history]`

## Project Limitations and Idiosyncrasies
1. The system assumes all servers are running on localhost (127.0.0.1)
2. File operations are limited to text files
3. The system may fail under the following conditions:
   - If any server is not running when needed
   - If the network connection is unstable
   - If file permissions are not properly set
   - If the system runs out of memory

## Reused Code
No code was reused from external sources. All code was written from scratch for this project, with reference to standard socket programming documentation and course materials.

## System Requirements
- Ubuntu 20.04 LTS
- C compiler (clang)
- Standard C libraries
- Network connectivity for localhost communication

## Building and Running
The project includes a Makefile for easy compilation and management:

### Makefile Commands
- `make all`: Compiles all servers and client
- `make serverM`: Compiles only the main server
- `make serverA`: Compiles only the authentication server
- `make serverR`: Compiles only the repository server
- `make serverD`: Compiles only the deployment server
- `make client`: Compiles only the client application
- `make clean`: Removes all compiled executables
- `make kill`: Terminates all running server processes (requires sudo)

### Running the System
1. Compile all components:
   ```bash
   make all
   ```

2. Start the servers in separate terminals in this order:
   ```bash
   ./serverM
   ./serverA
   ./serverR
   ./serverD
   ```

3. Run the client:
   ```bash
   ./client username password
   ```
   Or for guest access:
   ```bash
   ./client guest guest
   ```

