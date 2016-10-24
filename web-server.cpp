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

#include "httpTransaction.h"

class WebServer {
private:
    const static int MAX_BUF_SIZE = 81920;
    int listener;

    // timer on select()
    int sec;
    int usec;

    // file location
    std::string baseDir;

    // get and set for listener socket fd
    int getListener() { return listener; }
    void setListener(int i) { listener = i; }

    // file size helper function
    static std::ifstream::pos_type filesize(const char* filename)
    {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        return in.tellg(); 
    }

    // send all helper function
    static int sendall(int s, const char *buf, int *len)
    {
        int total = 0;        // how many bytes we've sent
        int bytesleft = *len; // how many we have left to send
        int n;
        while(total < *len) {
            n = send(s, buf+total, bytesleft, 0);
            if (n == -1) { break; }
            total += n;
            bytesleft -= n;
        }
        *len = total; // return number actually sent here
        return n==-1?-1:0; // return -1 on failure, 0 on success
    }

    // handle difference in ipv6 addresses and ipv4 addresses
    static void *get_in_addr(struct sockaddr *sa) {
        if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in*)sa)->sin_addr);
        }
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }
    
    // endswith helper function
    static std::string endswith(const std::string &str) {
        int dot = str.find_last_of('.');
        if (dot == -1) return "";
        return str.substr(dot + 1, str.size() - 1 - dot);
    }

    // send file helper function
    static void sendFile(int sock, const  std::string &fileDir) {
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

        int f; // file descriptor
        char send_buf[MAX_BUF_SIZE]; 
        if ( (f = open(fileDir.c_str(), O_RDONLY)) < 0) {
            perror("error");
        } else {
            ssize_t read_bytes;
            while ( (read_bytes = read(f, send_buf, MAX_BUF_SIZE)) > 0 ) {
                len = read_bytes;
                if ( (sendall(sock, send_buf, &len)) == -1 ) {
                    perror("send error");
                } else {
                }
            }
            std::cout << "Transfer Over!" << std::endl; 
            close(f);
        }
    }

    static void send404() {
        // TODO
    }

    static void send400() {
    
    }

    // split helper function
    static void split(const std::string &s, char delim, std::vector<std::string> &elems) {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            elems.push_back(item);
        }
    }


    static std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> elems;
        split(s, delim, elems);
        return elems;
    }
    
public:
    void setBaseDir(const std::string &s) {
        baseDir = s;
    }
    std::string getBaseDir() {
        return baseDir;
    }

    // set Timer for select all
    void setSelectTimer(int s, int u) {
        sec = s;
        usec = u;
    }

    // get dir function
    std::string getDir(const char *chs) {
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

    void setUpListnerSocket(const char *addr, const char *port) {
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
        if (listen(listener, 10) == -1) {
            perror("listen");
            exit(3);
        }
        setListener(listener);
    }
    
    void run() {
        fd_set master;      // master file descriptor list
        fd_set read_fds;    // temp file descriptor list for select()
        int fdmax;          // maximum file descriptor number

        int newfd;          // newly accept()ed socket descriptor

        struct sockaddr_storage remoteaddr; // client address
        socklen_t addrlen;

        char buf[MAX_BUF_SIZE];      // TODO: Remove this magic literal
        int nbytes;

        char remoteIP[INET6_ADDRSTRLEN];

        FD_ZERO(&master);       // clear the master and temp sets
        FD_ZERO(&read_fds);


        FD_SET(listener, &master);
        // keep track of the largest file descriptor
        fdmax = listener;
        while (true) {
            read_fds = master;
            if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
                perror("select");
                exit(4);
            }

            // run through the exisiting connections looking for data to read
            for (int i = 0; i <= fdmax; i++) {
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
                            printf("selectserver: new connection from %s on "
                                    "socket %d\n",
                                    inet_ntop(remoteaddr.ss_family,
                                        get_in_addr((struct sockaddr*)&remoteaddr),
                                        remoteIP, INET6_ADDRSTRLEN),
                                    newfd);
                        }
                    } else {
                        // handle data from a client 
                        if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
                            if (nbytes == 0) {
                                // connection closed
                                printf("selectserver: socket %d hung up\n", i);
                            } else {
                                perror("recv");
                            }
                            close(i);
                            FD_CLR(i, &master);
                        } else {
                            // got some goodies from clients
                            // TODO: need decoder to do parsing
                            std::string dir = getDir(buf);
                            std::string fileDir = getBaseDir() + "/" + dir;
                            std::cout << buf  <<  "\0" << std::endl;
                            std::cout << dir << std::endl;
                            std::cout << fileDir << std::endl;
                            
                            // check if file/dir exsit
                            struct stat s;
                            if( (stat(fileDir.c_str(), &s)) == 0 ) {
                                if( s.st_mode & S_IFDIR ) {
                                    // it's a dir. check if index.html exsit
                                    std::cout << "requesting a dir, looking for index.html" << std::endl;
                                    if (fileDir.size() == 0 || fileDir[fileDir.size() - 1] == '/') 
                                        fileDir = fileDir + "index.html";
                                    else fileDir = fileDir + '/' + "index.html";
                                    std::cout << "sending " + fileDir << std::endl;
                                    sendFile(i, fileDir);
                                } else if( s.st_mode & S_IFREG ) {
                                    std::cout << "sending " + fileDir << std::endl;
                                    sendFile(i, fileDir);
                                } else {
                                    // cannot recognize this sh*t, 404
                                    std::cout << "not exsit or cannot recognize, 404" << std::endl;
                                }
                            } else {
                                std::cout << "not exsit, 404" << std::endl;
                                
                            }

                        }

                    }
                }
            }

        }
    
    }


};


int main() {
    WebServer server;
    server.setBaseDir("/vagrant/code");
    server.setSelectTimer(3, 0);
    server.setUpListnerSocket(NULL, "3490");
    server.run();
}
