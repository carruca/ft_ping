# ft_ping

A custom implementation of the `ping` utility in C.

This project is a simple version of the `ping` command, which sends ICMP ECHO_REQUEST packets to a specified network host. It's a learning exercise to understand network programming with raw sockets.

## Features

*   Sends ICMP ECHO_REQUEST packets to a specified host.
*   Calculates and displays round-trip time, packet loss, and other statistics.
*   Supports `-v` (verbose) and `--ttl` command-line options.

## Building

To build the project, you can use the provided `Makefile`:

```sh
make
```

This will create the `ft_ping` executable in the root directory.

## Usage

To use `ft_ping`, you need to provide a hostname or IP address as an argument:

```sh
./ft_ping <hostname>
```

For example:

```sh
./ft_ping google.com
```

### Options

*   `-v`, `--verbose`: Enable verbose output.
*   `--ttl N`: Set the time-to-live (TTL) for outgoing packets.

## Cleaning up

To remove the compiled files, you can use:

```sh
make clean
```

## Project Structure

*   `src/`: Contains the source code for the `ft_ping` utility.
*   `ft_ping.h`: The main header file.
*   `Makefile`: The build script for the project.
*   `doc/`: Contains additional documentation or notes.
