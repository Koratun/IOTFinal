# This script will be the server listening for audio data from the m5stick
# It will then send the data to the AudioHub.py script
# It will take the output from the AudioHub.py script and send it to the m5stick

import socket
import sys
import select

# Create a TCP/IP socket
server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.setblocking(0)

# Bind the socket to the port
server_address = ('', 10000)
print(f'starting up on port {server_address[1]}')
server.bind(server_address)

# Listen for incoming connections
server.listen(5)

# Sockets from which we expect to read
inputs = [ server ]

# Sockets to which we expect to write
outputs = []

while inputs:

    # Wait for at least one of the sockets to be ready for processing
    readable, writable, exceptional = select.select(inputs, outputs, inputs)

    # Handle inputs
    for s in readable:
            
            if s is server:
                # A "readable" server socket is ready to accept a connection
                connection, client_address = s.accept()
                print(f'connection from {client_address}')
                connection.setblocking(0)
                inputs.append(connection)
    
                # Give the connection a queue for data we want to send
                outputs.append(connection)
    
            else:
                # A "readable" connection socket is ready to send/receive data
                data = s.recv(1024)
                if data:
                    print(data)
                    # Add output channel for response
                    outputs.append(s)
                else:
                    # Interpret empty result as closed connection
                    print('closing')
                    # Stop listening for input on the connection
                    inputs.remove(s)
                    s.close()
    
    # Handle outputs
    for s in writable:
        # A "writable" connection socket is ready to send data
        s.send(b'HTTP/1.1 200 OK\r\n\r\n')
        outputs.remove(s)
    
    # Handle "exceptional conditions"
    for s in exceptional:
        print('handling exceptional condition')
        # Stop listening for input on the connection
        inputs.remove(s)
        if s in outputs:
            outputs.remove(s)
        s.close()