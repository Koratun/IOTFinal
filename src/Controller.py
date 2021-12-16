# This script will be the server listening for audio data from the m5stick
# It will then send the data to the AudioHub.py script
# It will take the output from the AudioHub.py script and send it to the m5stick

import socket
import select
import datetime

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
    try:
        readable, writable, exceptional = select.select(inputs, outputs, inputs)

        sendtime = False

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
                # Read the data and trim off the beginning b' and end '
                data = str(s.recv(1024))[2:-1]
                if data:
                    if data == "Time":
                        # Set flag to send the time to the client
                        sendtime = True
                    else:
                        # parse datetime from data
                        dt = [[datetime.datetime.strptime(datapoint, '%Y-%m-%d %H:%M:%S') for datapoint in pair.split(',')] for pair in data.split(";")[:-1]]

                        # Output the data to a txt file
                        with open("data.txt", "a") as f:
                            last_date = ""
                            for pair in dt:
                                # Write the date to the file, but only if it's a new date
                                if str(pair[0].date()) != last_date:
                                    last_date = str(pair[0].date())
                                    f.write(last_date+"\n")
                                # Write the times to the file
                                f.write(str(pair[0].time()) + ", " + str(pair[1].time()) + "\n")
                                # Also write the number of seconds between the two datapoints
                                f.write(str(pair[1] - pair[0]) + "\n")
                    
                    # Add output channel for response
                    outputs.append(s)
                else:
                    # Interpret empty result as closed connection
                    print('closing')
                    # Stop listening for input on the connection
                    inputs.remove(s)
                    if s in outputs:
                        outputs.remove(s)
                    s.close()
        
        # Handle outputs
        for s in writable:
            # A "writable" connection socket is ready to send data
            # If the flag is set, send the time to the client
            if sendtime:
                print("Sending time")
                s.send(str(datetime.datetime.now()).encode())
                sendtime = False
        
        # Handle "exceptional conditions"
        for s in exceptional:
            print('handling exceptional condition')
            # Stop listening for input on the connection
            inputs.remove(s)
            if s in outputs:
                outputs.remove(s)
            s.close()
    except ValueError:
        # Remove the socket that raised the exception
        inputs = [server]
        outputs = []
