# Great Approximator (TCP Client-Server Game)

A concurrent TCP client-server network application implemented in C/C++ using raw POSIX/BSD sockets. Players (clients) compete to approximate a secret polynomial provided by the server within a limited number of moves.

## Key Features

* **Dual-Stack Networking:** Full support for both **IPv4 and IPv6** connections (`getaddrinfo`), seamlessly handling mixed environments.
* **Concurrent Multi-Client Architecture:** Non-blocking I/O multiplexing (using `poll`/`select`) allowing the server to handle multiple simultaneous clients, connection timeouts, and delayed responses without blocking other players.
* **Custom Text Protocol & Validation:** Strict parsing and validation of `\r\n`-terminated messages (`HELLO`, `COEFF`, `PUT`, `STATE`, `BAD_PUT`, `PENALTY`, `SCORING`), including penalty enforcement and lexicographical score sorting.
* **Two Client Modes:**
  * **Interactive Mode:** Non-blocking reading from `stdin` concurrent with network socket communication.
  * **Autonomous Bot Mode (`-a`):** Automated algorithmic strategy minimizing the mean squared error against the target polynomial.

## Build & Run

Build both binaries using the provided `Makefile`:
```bash
make
```

### Running the Server
```bash
./approx-server -f <coeffs_file> [-p port] [-k max_x] [-n degree] [-m max_moves]
```

### Running the Client
```bash
./approx-client -u <player_id> -s <server_address> -p <port> [-4 | -6] [-a]
```
