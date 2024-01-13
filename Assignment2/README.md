Name: Sourodeep Datta

Roll Number: 21CS10064

I have included a makefile for the ease of compilation. To compile the code, run the following command in the terminal:
```
make
```
This will create 2 executables, named file_server and file_client. Both the client and the server run on the hosts IP address. The server creates a socket that binds to the port 20004.

To run the server, run the following command in the terminal:
```
./file_server
```
Note that the server will run continuously. To stop the server, press Ctrl+C.

To run the client, run the following command in the terminal:
```
./file_client
```
The client will request the file name and the value of k. It will then send the request to the server. The server will then send the encoded file to the client. The client will then ask for another file name and this process will continue. To stop the client, press Ctrl+C.

To clean the directory, run the following command in the terminal:
```
make clean
```