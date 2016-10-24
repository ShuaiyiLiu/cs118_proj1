#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <cstring>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstddef>
#include "httpTransaction.h"
using namespace std;
static int MAX_BUF_SIZE = 1024;
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
static int sendall(int s, const char *buf, int len)
    {
        int total = 0;        // how many bytes we've sent
        int bytesleft = len; // how many we have left to send
        int n;
        while(total < len) {
            n = send(s, buf+total, bytesleft, 0);
            if (n == -1) { break; }
            total += n;
            bytesleft -= n;
        }
        len = total; // return number actually sent here
        return n==-1?-1:0; // return -1 on failure, 0 on success
    }
void getUrl(string& url, string& host, string& filepath, string& portnumber){//extract uri, host from input url
        host = "";
        string http = "http://";
         if (url.compare(0, http.size(), http) == 0) {
            //try to find port or filepath
            unsigned int pos = url.find_first_of(":", http.length());
            //no port or filepath
            if (pos == url.npos) {
                host = url.substr(http.size());
                return;
            }
            host = url.substr(http.size(), pos - http.size());
            if (pos < url.size() && url[pos] == ':') {
                // try to find filepath
                unsigned int pos2 = url.find_first_of('/', pos);
                // no filepath
                if (pos2 == url.npos) {
                    portnumber = url.substr(pos+1);
                }
            else
            {
                portnumber = url.substr(pos+1, pos2 - pos - 1);
                filepath = url.substr(pos2);
            }
        }
        // no port in URL
            else{
                filepath = url.substr(pos);
            }
        }
//if URL does not contain http://
        else {
            unsigned int pos = url.find_first_of(':', 0);
            if (pos == url.npos) {
                host = url;
                return;
            }
            host = url.substr(0, pos);
            if (pos < url.size() && url[pos] == ':') {
                // A port is provided
                unsigned int pos2 = url.find_first_of('/', pos);
                if (pos2 == url.npos) {
                    portnumber = url.substr(pos + 1);
                }
                else
                {
                    portnumber = url.substr(pos + 1, pos2 - pos - 1);
                    filepath = url.substr(pos2);
                }
            }
            else{
                filepath = url.substr(pos);
            }
        }
        /*if(url.compare(0, http.length(), http) == 0){
            uri = url.substr(http.length());
            unsigned int pos = uri.find_first_of('/', 0);
            if(pos == uri.npos){
                continue;
            }else{
                //host = uri.substr(0, pos);
                unsigned int pos2 = uri.find(':');
                unsigned int pos3 = uri.find_last_of('/');
                if()
                if(pos < uri.length() - 1){
                    filename = uri.substr(pos + 1);
                }
            }
        }else{
            uri = url;
            int pos = uri.find_first_of('/', 0);
            if(pos == uri.npos){
                host = uri;
            }else{
                host = uri.substr(0, pos);
                if(pos < uri.length() - 1){
                    filename = uri.substr(pos + 1);
                }
            }
        }*/
    }
int main(int argc, char *argv[]){
    if(argc < 2){
        fprintf(stderr,"no [URL]\n");
        exit(1);
    }
    vector<HttpRequest> request;
    string host, uri, filepath;
    string portnumber = "3490";
    for(int i = 1; i < argc; i++){
        host = "";
        uri = "";
        string filename = "";
        char s[INET6_ADDRSTRLEN];
        
        string arg = argv[i];
        getUrl(arg, host, filepath, portnumber);
        uri = host + filepath;
        unsigned int pos3 = filepath.find_last_of('/');
        if(pos3 + 1 < filepath.length()){
            filename = filepath.substr(pos3 + 1);
        }
        HttpRequest eleRequest;
        eleRequest.setRequestUri(filepath);
        string temp = "HTTP/1.0";
        eleRequest.setHttpVersion(temp);
        temp = "GET";
        eleRequest.setMethod(temp);
        string key = "Host";
        eleRequest.setHeaders(key, host);
        request.push_back(eleRequest);
        
        //set up socket and connection
        int status;
        struct addrinfo hints;
        struct addrinfo *servinfo, *p;  // will point to the results

        memset(&hints, 0, sizeof hints); // make sure the struct is empty
        hints.ai_family = AF_INET;    //  IPv4
        hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
        if((status = getaddrinfo(host.c_str(), portnumber.c_str(), &hints, &servinfo)) != 0){
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
            continue;
        }
        int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, 0);
        if(sockfd == -1){
            perror("cllient: socket");
            continue;
        }
        if (connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        if(servinfo == NULL){
            fprintf(stderr, "client: failed to connect\n");
            continue;
        }
        inet_ntop(servinfo->ai_family, get_in_addr((struct sockaddr *)servinfo->ai_addr),
            s, sizeof s);
        printf("client: connecting to %s\n", s);
        freeaddrinfo(servinfo);
        
        //send request
        string sendRequest = eleRequest.toRequestString();
        cout<< sendRequest + "used for debug!" <<endl;
        int len = sendRequest.size();
        sendall(sockfd, sendRequest.c_str(), len);
        
        //receive files
        ByteVector headerFile;
        int numbytes = 0, endOfHead;
        char buf[MAX_BUF_SIZE];
        if ((numbytes = recv(sockfd, buf, MAX_BUF_SIZE - 1, 0)) == -1) {
            perror("recv");
            continue;
        }
        for(int j = 0; j < numbytes; j++){
            if(j > 4 && buf[j] == '\n' && buf[j - 1] == '\r' && buf[j - 2] == '\n' && buf[j - 3] == '\r'){
                endOfHead = j;
                break;
            }
            headerFile.push_back(buf[j]);
        }
        if(endOfHead + 1 >= numbytes){
            cout<<"no file received"<<endl;
            continue;
        }
        // construct a HttpResponse
        HttpResponse response;
        response.consume(headerFile);
        cout<< response.toResponseString()<<endl;
        //download the file
        temp = "Content-Length";
        int contentLength = atoi(response.getHeader(temp).c_str());
        ofstream myfile;
        if(filename == ""){
            filename = "index.html";
        }
        myfile.open (filename);
        for(int j = endOfHead + 1; j < numbytes; j++){
            myfile<<buf[j];
        }
        int numReceived = numbytes - endOfHead - 1;
        while(numReceived < contentLength){
            if ((numbytes = recv(sockfd, buf, MAX_BUF_SIZE - 1, 0)) == -1) {
                perror("recv");
                continue;
            }
            numReceived = numReceived + numbytes;
            for(int j = 0; j < numbytes; j++){
                myfile << buf[j];
            }
        }
        myfile.close();
        cout << "file received" << endl;
        close(sockfd);
    }
    cout << "all files received" << endl;
    return 0;
}

    
/*    void run(){
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(3490);     // short, network byte order
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

  // connect to the server
        if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
        perror("connect");
        return 2;
        }
    }*/
