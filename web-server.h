#ifndef WEB_SERVER_H
#define WEB_SERVER_H

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
#define MAX_LISTENER 20
int setNonblocking(int fd);

class WebServer {
private:
    class FileSender;
    fd_set master;      // master file descriptor list
    ByteVector wire[MAX_LISTENER];
    bool recvStatus[MAX_LISTENER]; // 0 receiving 1 complete
    

    const static int MAX_BUF_SIZE = 4096;
    int listener;
    std::string host;
    std::string port;

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
        enum {IDLE, SENDING } f_state;
    public:
        FileSender() { f_state = IDLE; }
        // Non-blocking file sender
        void sendFile(const char* filename, int s_fd);
        int handle_io();
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
    void setFileSender(int sock, const std::string &fileDir);

    static std::ifstream::pos_type filesize(const char* filename) {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        return in.tellg(); 
    }

    static void send404(int sock) {
        std::string str404 = "HTTP/1.0 404 Not Found\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: 9\r\n"
                             "\r\n"         
                             "Not Found";
        int len = str404.length();
        sendall(sock, str404.c_str(), &len);
    }

    static void send400(int sock) {
        std::string str400 = "HTTP/1.0 400 Bad Request\r\n"
                             "Content-Type: text/html\r\n"
                             "Content-Length: 11\r\n"
                             "\r\n"         
                             "Bad Request";
        int len = str400.length();
        sendall(sock, str400.c_str(), &len);
    }

public:
    void setBaseDir(const std::string &s) { baseDir = s; }
    std::string getBaseDir() { return baseDir; }

    // set Timer for select all
    void setSelectTimer(int s, int u) { sec = s; usec = u; }

    void setUpListnerSocket(const char *addr, const char *port);
    
    void run();
};

#endif
