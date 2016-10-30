#include <cstdlib>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include "httpTransaction.h"

using namespace std;

/*
 * 
 */
HttpTransaction :: HttpTransaction(){
    httpVersion = "";
    headers.clear();
}
void HttpTransaction :: setHeaders(string& key, string& value){
    headers[key] = value;
}
string HttpTransaction ::getHeaders(){
    string result = "";
    map<string, string>::iterator iter = headers.begin();
    while (iter != headers.end())
	{
            result = result + iter->first + ": " + iter->second + "\r\n";
            iter++;
	}
	result = result + "\r\n";
	return result;
}
string HttpTransaction::getHeader(string& key){
    if(headers.count(key) > 0){
        return headers[key];
    }else{
        return "none";
    }  
}
void HttpTransaction ::setHttpVersion(string& version){
    httpVersion = version;
}
string HttpTransaction::getHttpVersion(){
    return httpVersion;
}

void HttpRequest::setRequestUri(string& uri){
    requestUri = uri;
}
string HttpRequest::getRequestUri(){
    return requestUri;
}
void HttpRequest::setMethod(string& newMethod){
    method = newMethod;
}
string HttpRequest::getMethod(){
    return method;
}
string HttpRequest::toRequestString(){
    string result = "";
    result = result + getMethod() + " " + getRequestUri() + " " + getHttpVersion() + "\r\n";
    result = result + getHeaders();
    return result;
}
vector<char> HttpRequest::encode(){
    string str = toRequestString();
    vector<char> result;
    result.assign(str.begin(), str.end());
    return result;
}
bool HttpRequest::decodeFirstline(ByteVector& first){
    string str = "";
    int numOfSp = 0;
    for(int i = 0; first[i] != '\r'; i++){
        if(first[i] == ' '){
            numOfSp++;
            if(numOfSp == 1){
                setMethod(str);
                str = "";
            }else if(numOfSp == 2){
                setRequestUri(str);
                str = "";
            }
            if(i > 0 && first[i - 1] == ' ' && numOfSp > 2){
                return false;
            }
            continue;
        }
        str = str + first[i];;
    }
    setHttpVersion(str);
    if(getMethod().compare("GET") != 0){
        return false;
    }
    if(getHttpVersion().compare("HTTP/1.0") != 0){
        return false;
    }
    return true;
}
//split helper
void split(const string &s, vector<string> &elems) {
        stringstream ss;
        ss.str(s);
        string item;
        while (getline(ss, item)) {
            if ( item.size() != 0 && item[item.size()-1] == '\r' ){
            item = item.substr( 0, item.size() - 1 );
            }
            if(item.size() != 0){
                elems.push_back(item);
            }
        }
    }
void HttpTransaction::decodeHeaders(ByteVector& lines){
    vector<string> elems;
    string s(lines.begin(), lines.end());
    split(s, elems);
    string key = "";
    string value = "";
    for(unsigned int i = 0; i < elems.size(); i++){
        unsigned int pos = elems[i].find(':');
        if(pos == elems[i].npos){
            key == elems[i];
            setHeaders(key, value);
            key = "";
            value = "";
            continue;
        }
        key = elems[i].substr(0, pos);
        if(pos + 2 >= elems[i].length()){
            value = "";
        }else{
            value = elems[i].substr(pos + 2, elems[i].length() - pos - 2);
        }
        setHeaders(key, value);
        key = "";
        value = "";
    }
}
bool HttpRequest::consume(ByteVector& wire){
    ByteVector Firstline;
    ByteVector Headerlines;
    unsigned int i = 0;
    int flag = 0;
    while(i < wire.size()){
        if(i >= 4 && wire[i - 4] == '\r' && wire[i - 3] == '\n' && 
                wire[i - 2] == '\r' && wire[i - 1] == '\n' && flag == 1){
            flag = 2;
        }else if(i >= 2 && wire[i - 2] == '\r' && wire[i - 1] == '\n' && flag == 0){
            flag = 1;
        }
        if(flag == 0){
            Firstline.push_back(wire[i]);
        }else if(flag == 1){
            Headerlines.push_back(wire[i]);
        }else{
            break;
        }
        i++;
    }
    if(!decodeFirstline(Firstline)){
        return false;
    }
    decodeHeaders(Headerlines);
    return true;
}
void HttpResponse::setStatus(int responsestatus){
    status = responsestatus;
    switch(status){
        case 200 :
            statusDef = "OK";
            break;
        case 400 :
            statusDef = "Bad request";
            break;
        case 404 :
            statusDef = "Not found";
    }
}
int HttpResponse::getStatus(){
    return status;
}
string HttpResponse::getStatusDefinition(){
    return statusDef;
}
string HttpResponse::toResponseString(){
    string str = "";
    string statusString = to_string(getStatus());
    str = str + getHttpVersion() + " " + statusString + " " + getStatusDefinition() + "\r\n";
    str = str + getHeaders();
    return str;
    
}
vector<char> HttpResponse::encode(){
    vector<char> result;
    string str = "";
    string statusString = to_string(getStatus());
    str = str + getHttpVersion() + " " + statusString + " " + getStatusDefinition() + "\r\n";
    str = str + getHeaders();
    result.assign(str.begin(), str.end());
    
    return result;
}
bool HttpResponse::decodeFirstline(ByteVector& first){
    string str = "";
    int numOfSp = 0;
    for(int i = 0; first[i] != '\r'; i++){
        if(first[i] == ' '){
            numOfSp++;
            if(numOfSp == 1){
                setHttpVersion(str);
                str = "";
            }else if(numOfSp == 2){
                setStatus(atoi(str.c_str()));
                str = "";
            }
            continue;
        }
        str = str + first[i];
    }
    return true;
}
bool HttpResponse::consume(ByteVector& wire){
    ByteVector Firstline;
    ByteVector Headerlines;
    unsigned int i = 0;
    int flag = 0;
    while(i < wire.size()){
        if(i >= 4 && wire[i - 4] == '\r' && wire[i - 3] == '\n' && 
                wire[i - 2] == '\r' && wire[i - 1] == '\n' && flag == 1){
            flag = 2;
        }else if(i >= 2 && wire[i - 2] == '\r' && wire[i - 1] == '\n' && flag == 0){
            flag = 1;
        }
        if(flag == 0){
            Firstline.push_back(wire[i]);
        }else if(flag == 1){
            Headerlines.push_back(wire[i]);
        }else{
            break;
        }
        i++;
    }
    if(!decodeFirstline(Firstline)){
        return false;
    }
    decodeHeaders(Headerlines);
    return true;
}