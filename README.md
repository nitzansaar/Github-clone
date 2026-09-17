# Distributed Github Clone

A distributed file management system written in C, featuring a central coordinator server and specialized backend services for authentication, repository storage, and deployment — communicating over TCP and UDP sockets.

## Architecture

```
                    ┌──────────┐
                    │  Client  │
                    └────┬─────┘
                         │ TCP
                    ┌────▼─────┐
                    │ ServerM  │  (Main Coordinator)
                    └──┬──┬──┬─┘
               UDP /   │  │  \  UDP
            ┌─────▼┐ ┌▼──▼┐ ┌▼─────┐
            │ServerA│ │ SR │ │ServerD│
            │(Auth) │ │(Repo)│ │(Deploy)│
            └──────┘ └─────┘ └───────┘
```

**ServerM** (Main) — Central coordinator that accepts client connections over TCP and routes requests to backend services over UDP.

**ServerA** (Auth) — Handles user credential verification with encrypted password storage and Caesar cipher decryption. Supports member and guest authentication.

**ServerR** (Repo) — Manages the file repository: lookup, push, remove, and overwrite confirmation. Maintains a persistent file database.

**ServerD** (Deploy) — Handles file deployment with timestamped logging and status reporting.

## Features

- Concurrent client connections with thread-safe operation logging
- Dual-protocol communication: TCP for client-server, UDP for inter-service
- Member and guest authentication modes
- File operations: lookup, push, remove, deploy
- Custom wire protocol with structured request/response message formats
- Deployment history tracking with timestamps

## Wire Protocol

| Operation | Client → ServerM | ServerM → Backend | Response |
|---|---|---|---|
| Auth | `AUTH user pass` | `user pass` → ServerA | `MEMBER_AUTH_SUCCESS` / `AUTH_FAILED` |
| Lookup | `LOOKUP user` | `LOOKUP user requester` → ServerR | File list or error |
| Push | `PUSH filename` | `PUSH user filename` → ServerR | Success or `OVERWRITE_CONFIRM` |
| Deploy | `DEPLOY` | `DEPLOY user` → ServerD | Deployment status |
| Remove | `REMOVE filename` | `REMOVE user filename` → ServerR | Success or error |

## Build and Run

```bash
# Compile all components
make all

# Start servers (each in a separate terminal)
./serverM
./serverA
./serverR
./serverD

# Connect as a member
./client username password

# Connect as a guest
./client guest guest
```

## Tech Stack

- **Language:** C
- **Networking:** POSIX sockets (TCP + UDP)
- **Concurrency:** pthreads with mutex-protected logging
- **Build:** Make
