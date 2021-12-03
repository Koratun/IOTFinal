# This script will be the server listening for audio data from the m5stick
# It will then send the data to the AudioHub.py script
# It will take the output from the AudioHub.py script and send it to the m5stick

import socket
import asyncio
import select
import AudioHub
import time

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

#Get the asyncio event loop
loop = asyncio.get_event_loop()

# A method that creates asyncio tasks to recognize the audio
def createAudioTask(audio_file):
    # Create a task to recognize the audio
    return loop.create_task(AudioHub.getShazam(audio_file))

async def main():
    audioTasks = []

    timer = 0
    seconds_to_wait = 25

    last_execution = seconds_to_wait
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
                # Read the data and trim off the beginning b' and end '
                data = str(s.recv(1024))[2:-1]
                if data:
                    if not "Hello" in data:
                        # transmit audio data to the AudioHub.py script and add the task to the list of tasks

                        # If the timer has not started, then start it
                        if timer == 0:
                            print("Timer started")
                            # Decode the data we received and save it to a file
                            AudioHub.decodeAudio(data)
                            # Start timer
                            timer = time.time() + seconds_to_wait
                        elif int(timer - time.time()) % 5 == 0 and not int(timer - time.time()) == last_execution:
                            last_execution = int(timer - time.time())
                            # If 5 seconds have passed since the timer started (or since the last task was created),
                            # then create a task to recognize the audio
                            print(f"Creating task with {last_execution} seconds left")
                            audioTasks += createAudioTask(AudioHub.decodeAudio(data, append=True, convert=True))
                            print("Task created")
                        else:
                            # If we have data to decode but aren't ready to create a task, then just save the data
                            AudioHub.decodeAudio(data, append=True)

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
            # If a task has completed successfully, send the result to the client
            for task in audioTasks:
                if task.done():
                    try:
                        s.send(str(task.result()).encode())
                        print(f"Sent {task.result()}")
                        outputs.remove(s)
                        audioTasks.remove(task)
                        for t in audioTasks:
                            t.cancel()
                        audioTasks = []
                        break
                    except Exception as e:
                        print(e)
                        audioTasks.remove(task)

            if timer > 0 and timer - time.time() < 0:
                # If the timer has expired, then reset the timer and audioTasks
                print("Timer expired")
                if len(audioTasks) > 0:
                    # Tell the client that a song was not found if there are still tasks in the list
                    s.send("Song not found\n".encode())
                    audioTasks = []
                outputs.remove(s)
                timer = 0
                last_execution = seconds_to_wait
        
        # Handle "exceptional conditions"
        for s in exceptional:
            print('handling exceptional condition')
            # Stop listening for input on the connection
            inputs.remove(s)
            if s in outputs:
                outputs.remove(s)
            s.close()

asyncio.run(main())