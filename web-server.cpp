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


// set socket fd to non blocking
int setNonblocking(int fd) {
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}     


class WebServer {
private:
    class FileSender;
    fd_set master;      // master file descriptor list

    const static int MAX_BUF_SIZE = 4096;
    int listener;

    // timer on select()
    int sec;
    int usec;

    // file location
    std::string baseDir;
    std::unordered_map<int, FileSender*> sf_map;

    // get and set for listener socket fd
    int getListener() { return listener; }
    void setListener(int i) { listener = i; }

    // nonblocking file sender class
    class FileSender {
    private:
        int f_fd; // file to send
        int sock_fd; // socket fd
        char f_buf[MAX_BUF_SIZE];
        int f_buf_len; // bytes in buffer
        int f_buf_used; // bytes used so far <= f_buf_len
    public:
        
        // Non-blocking file sender
        void sendFile(const char* filename, int s_fd) {
            setNonblocking(s_fd);
            std::cout << "SERVER: Start to open file." << std::endl;

            if ((f_fd = open(filename, O_RDONLY)) < 0) {
                perror("file open failed");
            }

            f_buf_used = 0;
            f_buf_len = 0;
            sock_fd = s_fd;
        }

        int handle_io() {
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
    };

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

    // send all helper function
    static int sendall(int s, const char *buf, int *len) {
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

    // send file helper function
    void setFileSender(int sock, const std::string &fileDir) {
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

    static void send404(int sock) {
        // TODO
    }

    static void send400(int sock) {
    
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
                        std::cout << "SERVER: File has been sent." << std::endl;
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
                        }
                    } else {
                        // handle data from a client 
                        if ((nbytes = recv(i, buf, sizeof(buf), 0)) <= 0) {
                            if (nbytes == 0) {
                                // nothing new to receive
                                printf("selectserver: socket %d hung up\n", i);
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


};

int main(int argc, char* argv[]) {
    WebServer server;
    server.setBaseDir("/vagrant/code");
    server.setSelectTimer(3, 0);
    server.setUpListnerSocket(NULL, "3490");
    server.run();
}
