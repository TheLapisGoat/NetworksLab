Name: Sourodeep Datta

Roll Number: 21CS10064

I have included a makefile for the ease of compilation. To compile the code, run the following command in the terminal:
```
make
```
This will create 2 executables, named wordserver and wordclient. Both the client and the server run on the hosts IP address. The server creates a socket that binds to the port 20010. The client creates a socket that binds to the port 20011.

To run the server, run the following command in the terminal:
```
./wordserver
```
Note that the server will continue running even after the client has finished running. To stop the server, press Ctrl+C.

The client requires 2 arguments, the first being the name of the file to request from the server and the second being the name of the file it should create. Both these names must be different.

To run the client, an example command in the terminal is:
```
./wordclient file.txt file2.txt
```
The client will request the file file.txt from the server and create a file named file2.txt in the same directory as the client. If the file file2.txt already exists, it will be overwritten.

To clean the directory, run the following command in the terminal:
```
make clean
```