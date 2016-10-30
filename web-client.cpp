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
            unsigned int pos = url.find_first_of(':', http.length());
            //no port or filepath
            if (pos < url.size() && url[pos] == ':') {
                // try to find filepath
                host = url.substr(http.size(), pos - http.size());
                unsigned int pos2 = url.find_first_of('/', pos);
                // no filepath
                if (pos2 == url.npos) {
                    portnumber = url.substr(pos+1);
                }else{
                    portnumber = url.substr(pos+1, pos2 - pos - 1);
                    filepath = url.substr(pos2);
                }
            }else{
                unsigned int pos2 = url.find_first_of('/', http.length());
                if(pos2 < url.length() && url[pos2] == '/'){
                    host = url.substr(http.size(), pos2 - http.size());
                    filepath = url.substr(pos2);
                    return;
                }else{
                    host = url.substr(http.size());
                    return;
                }
            }
            
        }//if URL does not contain http://
        else {
            unsigned int pos = url.find_first_of(':', 0);
            //no port or filepath
            if (pos < url.size() && url[pos] == ':') {
                // try to find filepath
                host = url.substr(0, pos);
                unsigned int pos2 = url.find_first_of('/', pos);
                // no filepath
                if (pos2 == url.npos) {
                    portnumber = url.substr(pos+1);
                }else{
                    portnumber = url.substr(pos+1, pos2 - pos - 1);
                    filepath = url.substr(pos2);
                }
            }else{
                unsigned int pos2 = url.find_first_of('/', 0);
                if(pos2 < url.length() && url[pos2] == '/'){
                    host = url.substr(0, pos2);
                    filepath = url.substr(pos2);
                    return;
                }else{
                    host = url.substr(0);
                    return;
                }
            }
        }
    }
int main(int argc, char *argv[]){
    if(argc < 2){
        fprintf(stderr,"no [URL]\n");
        exit(1);
    }
    vector<HttpRequest> request;
    string host, uri, filepath;
    string portnumber = "80";
    for(int i = 1; i < argc; i++){
        host = "";
        uri = "";
        string filename = "";
        char s[INET6_ADDRSTRLEN];
        
        string arg = argv[i];
        getUrl(arg, host, filepath, portnumber);
        uri = filepath;
        if(portnumber == ""){
            portnumber = "80";
        }
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
        string value = host;
        eleRequest.setHeaders(key, value);
        /*key = "Connection";
        value = "close";
        eleRequest.setHeaders(key, value);
        request.push_back(eleRequest);*/
    
        //set up socket and connection
        int status;
        struct addrinfo hints;
        struct addrinfo *servinfo;  // will point to the results

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
        cout<< sendRequest <<endl;
        int len = sendRequest.size();
        sendall(sockfd, sendRequest.c_str(), len);
        
        //receive files
        ByteVector headerFile;
        int numbytes = 0, endOfHead = 0;
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
        cout << "file " + filename+ " received" << endl;
        close(sockfd);
    }
    cout << "all files received" << endl;
    return 0;
}
