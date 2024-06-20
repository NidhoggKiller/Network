# Network Homework

Warning！！！  
Plagiarism is prohibited, for reference only.

To YUHAN MA 마위한

### Feature Introduction

```/invite``` Command: This command allows you to invite a user for a one-on-one private chat. The invited user can use the ```/yes``` or ```/no``` commands to accept or decline the invitation. The ```/out``` command can be used by both parties to simultaneously exit the one-on-one private chat.```/id```you can find you id code.

Press ```p``` or ```P``` to exit the client.

### Compile client and server files using a compiler.
```linux
gcc -o chat_clnt chat_clnt.c
```
```linux
gcc -o chat_serv chat_serv.c
```

### Start the server and client.
```linux
./chat_serv <Port>
```
```linux
./chat_clnt <Ip> <port> <name>
```

