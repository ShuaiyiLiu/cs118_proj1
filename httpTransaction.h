/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   httpTransaction.h
 * Author: HOU
 *
 * Created on 2016年10月13日, 下午10:55
 */

#ifndef HTTPTRANSACTION_H
#define HTTPTRANSACTION_H
#include <map>
#include <vector>
#include <string>
#include <istream>

typedef std::map<std::string, std::string> HeaderMap;
typedef std::vector<std::uint8_t> ByteVector;

class HttpTransaction {
private:
	HeaderMap headers;
	std::string httpVersion;

public:
    HttpTransaction();
	void setHeaders(std::string key, std::string value);
	std::string getHeaders();
        std::string getHeader(std::string key);
    void setHttpVersion(std::string &);
    std::string getHttpVersion();
};

class HttpRequest: public HttpTransaction {
private:
    std::string method;
    std::string requestUrl;
public:
    // TODO: build a constructor that takes vector/array of bytes as input

    void setRequestUri(std::string &);
    std::string getRequestUrl();

    void setMethod(std::string &);
    std::string getMethod();

    std::string toRequestString();
    void parseFromMessageString(std::string &);

    std::vector<uint8_t> encode();
};

class HttpResponse: public HttpTransaction {
private:
    int status;
    std::string statusDef;
public:
    // TODO: build a constructor that takes vector/array of bytes as input

    static const int SC_OK = 200;
    static const int SC_BAD_REQUEST = 400;
    static const int SC_NOT_FOUND = 404;

    void setStatus(int);
    int getStatus();
    std::string getStatusDefinition();
    //std::string toResponseString();
    std::vector<uint8_t> encode();

    // use std::istream type to represent file in the content
};


#endif /* HTTPTRANSACTION_H */

