/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   httpTransaction.cpp
 * Author: HOU
 *
 * Created on 2016年10月14日, 下午8:04
 */

#include <cstdlib>

#include "httpTransaction.h"

using namespace std;

/*
 * 
 */
void HttpTransaction :: HttpTransaction(){
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
string HttpTransaction ::getHeader(string& key){
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

void HttpRequest::setRequestUrl(string& uri){
    requestUrl = uri;
}
string HttpRequest::getRequestUrl(){
    return requestUrl;
}
string HttpRequest::setMethod(string& newMethod){
    method = newMethod;
}
string HttpRequest::getMethod(){
    return method;
}
string HttpRequest::toRequestString(){
    string result = "";
    result = result + getMethod() + " " + getRequestUrl() + " " + getHttpVersion() + "\r\n";
    result = result + getHeaders();
    return result;
}
vector<uint8_t> HttpRequest::encode(){
    string str = toRequestString();
    vector<unit8_t> result;
    result.assign(str.begin(), str.end());
    return result;
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
void HttpResponse::getStatus(){
    return status;
}
string HttpResponse::getStatusDefinition(){
    return statusDef;
}
/*string HttpResponse::toResponseString(){
    string result;
    
}*/
vector<unit8_t> HttpResponse::encode(){
    vector<unit8_t> result;
    string str = "";
    str = str + getHttpVersion() + " " + getStatus() + " " + getStatusDefinition() + "\r\n";
    str = str + getHeaders();
    result.assign(str.begin(), str.end());
    
    return result;
}
