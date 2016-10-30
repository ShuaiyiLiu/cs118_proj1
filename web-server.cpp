#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h> // for POSIX function ``stat'' to see if a file exsit
#include <unordered_map>

#include "httpTransaction.h"
#include "web-server.h"

#define MAX_LISTENER 10

// set socket fd to non blocking
int setNonblocking(int fd) {
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}     

void WebServer::FileSender::sendFile(const char* filename, int s_fd) {
    setNonblocking(s_fd);
    std::cout << "SERVER: Start to open file." << std::endl;

    if ((f_fd = open(filename, O_RDONLY)) < 0) {
        perror("file open failed");
    }

    f_buf_used = 0;
    f_buf_len = 0;
    sock_fd = s_fd;
}

int WebServer::FileSender::handle_io() {
    if (f_buf_used == f_buf_len) {
        f_buf_len = read(f_fd, f_buf, MAX_BUF_SIZE);
        if (f_buf_len < 0) { perror("read"); }
        else if (f_buf_len == 0) {
            close(f_fd);
            return 1;
        }
        f_buf_used = 0;
    }

    int nsend = send(sock_fd, f_buf + f_buf_used, f_buf_len - f_buf_used, 0);
    if (nsend < 0) {
        perror("send error");
        close(sock_fd);
    }
    f_buf_used += nsend;
    return 0;
}

void WebServer::setFileSender(int sock, const std::string &fileDir) {
    std::string fileSize = std::to_string(filesize(fileDir.c_str()));
    std::string ct;
    std::string pos = endswith(fileDir);
    if (pos == "html") {
        ct = "text/html";
    } else if (pos == "png" || pos == "jpg" || pos == "jepg" || pos == "bmp") { // TODO: add a image file detector
        ct = "image/" + pos;
    } else {
        ct = "octet-stream";
    }

    std::string head = "HTTP/1.1 200 OK\r\n"
        "Accept-Ranges: bytes\r\n"
        "Content-Length: " + fileSize + "\r\n" +
        "Content-Type: " + ct + "\r\n\r\n";
    int len = strlen(head.c_str());
    sendall(sock, head.c_str(), &len);

    FileSender *fs = new FileSender;
    fs->sendFile(fileDir.c_str(), sock);
    sf_map[sock] = fs;
}

// get dir function
std::string WebServer::getDir(const char *chs) {
    std::string s(chs);
    std::vector<std::string> splits = split(s, ' ');
    std::string dir = splits[1];
    for (unsigned int i = 0; i < dir.size() - 1; ++i) {
        if (dir[i] == '/' && dir[i + 1] != '/') 
            if (i == 0 || (i > 0 && dir[i - 1] != '/')) {
                return dir.substr(i + 1, dir.size() - i - 1); 
            }
    }
    return "";
}

void WebServer::setUpListnerSocket(const char *addr, const char *port) {
    struct addrinfo hints; // for retrieving addrinfo

    // filling hints object
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rv;
    struct addrinfo *ai, *p;
    if ((rv = getaddrinfo(addr, port, &hints, &ai)) != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    int listener;
    int yes = 1;
    for (p = ai; p != NULL; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue; 
        }
        // lose the pesky "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue; 
        }
        break;
    }

    // if we got here, it means we didn't get bound
    if (p == NULL) {
        fprintf(stderr, "selectserver: failed to bind\n");
        exit(2);
    }
    freeaddrinfo(ai); // all done with this

    // listen
    if (listen(listener, MAX_LISTENER) == -1) {
        perror("listen");
        exit(3);
    }
    setListener(listener);
    std::cout << "SERVER: Listening on " << addr << ":" << port << std::endl;
}
    
void WebServer::run() {
    fd_set read_fds;    // temp file descriptor list for select()
    fd_set write_fds;
    int fdmax;          // maximum file descriptor number
    int newfd;          // newly accept()ed socket descriptor
    struct sockaddr_storage remoteaddr; // client address
    socklen_t addrlen;
    char buf[MAX_BUF_SIZE];      // TODO: Remove this magic literal
    int nbytes;
    char remoteIP[INET6_ADDRSTRLEN];

    FD_ZERO(&master);       // clear the master and temp sets
    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);
    FD_SET(listener, &master);

    struct timeval tv;
    // keep track of the largest file descriptor
    fdmax = listener;
    int nReadyFds = 0;
    while (true) {
        read_fds = master;
        tv.tv_sec = sec;
        tv.tv_usec = usec;
        if ((nReadyFds = select(fdmax + 1, &read_fds, &write_fds, NULL, &tv)) == -1) {
            perror("select");
        } else if (nReadyFds == 0) {
            std::cout << "SERVER: No data received for " << sec << " seconds!" << std::endl; 
            continue;
        }

        // run through the exisiting connections looking for data to read
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &write_fds)) {
                int sig = sf_map[i]->handle_io();
                if (sig == 1) { // all set
                    std::cout << "SERVER: The requested file has been successfully sent." << std::endl;
                    FD_CLR(i, &write_fds);
                }
            }
            if (FD_ISSET(i, &read_fds)) { // we got one
                if (i == listener) {
                    // handle new connections
                    addrlen = sizeof(remoteaddr);
                    newfd = accept(listener, (struct sockaddr *)&remoteaddr, &addrlen);
                    if (newfd == -1) {
                        perror("accept");
                    } else {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax) {    // keep track of the max
                            fdmax = newfd;
                        }
                        printf("selectserver: socket %d\n", newfd);
                        std::cout << "SERVER: New client on socket " << newfd << std::endl;
                    }
                } else {
                    // handle data from a client 
                    if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
                        if (nbytes == 0) {
                            // nothing new to receive
                            printf("SERVER: Socket %d hung up\n", i);
                        } else {
                            perror("recv");
                        }
                        close(i);
                        FD_CLR(i, &master);
                        FD_CLR(i, &write_fds);
                    } else {
                        // got some goodies from clients
                        // TODO: need decoder to do parsing
                        std::string dir = getDir(buf);
                        std::string fileDir = getBaseDir() + "/" + dir;

                        // check if file/dir exsit
                        struct stat s;
                        if( (stat(fileDir.c_str(), &s)) == 0 ) {
                            if( s.st_mode & S_IFDIR ) {
                                if (fileDir.size() == 0 || fileDir[fileDir.size() - 1] == '/') 
                                    fileDir = fileDir + "index.html";
                                else fileDir = fileDir + '/' + "index.html";
                                setFileSender(i, fileDir);
                                FD_SET(i, &write_fds);
                            } else if( s.st_mode & S_IFREG ) {
                                setFileSender(i, fileDir);
                                FD_SET(i, &write_fds);
                            } else {
                                std::cout << "not exsit or cannot recognize, 404" << std::endl;
                                send404(i);
                            }
                        } else {
                            std::cout << "not exsit, 404" << std::endl;
                            send404(i);
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    WebServer server;
    if (argc == 1) { // use default
        server.setBaseDir(".");
        server.setSelectTimer(10, 0);
        server.setUpListnerSocket("localhost", "4000");
    } else if (argc == 4) {
        server.setUpListnerSocket(argv[1], argv[2]);
        server.setBaseDir(argv[3]);
        server.setSelectTimer(10, 0);
    } else {
        std::cout << "Invalid arguments! \n"
                     "Plase follow the format: ./web-server [hostname port file-dir]" 
                  << std::endl;
        return 0;
    }
    server.run();
    return 0;
}
