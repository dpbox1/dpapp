#include "dpapp/dpret.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const char* _errmsgs[600] = {NULL};

const char* dperr_detail(int err)
{
    if (_errmsgs[150] == NULL) {
        _errmsgs[150] = "Unknown Error";
        _errmsgs[151] = "Repeat Operation";
        _errmsgs[152] = "Not initialized";
        _errmsgs[153] = "Open Failed";
        _errmsgs[154] = "File or source closed";
        _errmsgs[155] = "Set Attribute Failed";
        _errmsgs[156] = "End of data or file";
        _errmsgs[157] = "Not exists";
        _errmsgs[158] = "Has been initialized";
        _errmsgs[159] = "Initialization due to Invalid parameter";

        _errmsgs[160] = "The parameter type mismatch";
        _errmsgs[166] = "Missing parameter";
        _errmsgs[167] = "Prepareing or bind parameter error";
        _errmsgs[168] = "Need authentication";
        _errmsgs[169] = "Not open source";
        _errmsgs[170] = "Invalid command";
        _errmsgs[171] = "Already authenticated";
        _errmsgs[172] = "Invalid data type";
        _errmsgs[173] = "Listen no events";
        _errmsgs[174] = "Authentication error";
        _errmsgs[175] = "Not support";
        _errmsgs[176] = "No source";
        _errmsgs[177] = "Object or source deleted";
        _errmsgs[178] = "Not enough data or buffer space";
        _errmsgs[179] = "Partially OK";
        _errmsgs[180] = "Ignore the operation";

        _errmsgs[190] = "Continue";
        _errmsgs[191] = "Switching Protocols";
        _errmsgs[192] = "Processing";
        _errmsgs[193] = "Early Hints";

        _errmsgs[200] = "OK";
        _errmsgs[201] = "Created";
        _errmsgs[202] = "Accepted";
        _errmsgs[203] = "Non-Authoritative Information";
        _errmsgs[204] = "No Content";
        _errmsgs[205] = "Reset Content";
        _errmsgs[206] = "Partial Content";
        _errmsgs[207] = "Multi-Status";
        _errmsgs[208] = "Already Reported";
        _errmsgs[226] = "IM Used";
        _errmsgs[300] = "Multiple Choices";
        _errmsgs[301] = "Moved Permanently";
        _errmsgs[302] = "Found";
        _errmsgs[303] = "See Other";
        _errmsgs[304] = "Not Modified";
        _errmsgs[305] = "Use Proxy";
        _errmsgs[307] = "Temporary Redirect";
        _errmsgs[308] = "Permanent Redirect";
        _errmsgs[400] = "Bad Request";
        _errmsgs[401] = "Unauthorized";
        _errmsgs[402] = "Payment Required";
        _errmsgs[403] = "Forbidden";
        _errmsgs[404] = "Not Found";
        _errmsgs[405] = "Method Not Allowed";
        _errmsgs[406] = "Not Acceptable";
        _errmsgs[407] = "Proxy Authentication Required";
        _errmsgs[408] = "Request Timeout";
        _errmsgs[409] = "Conflict";
        _errmsgs[410] = "Gone";
        _errmsgs[411] = "Length Required";
        _errmsgs[412] = "Precondition Failed";
        _errmsgs[413] = "Payload Too Large";
        _errmsgs[414] = "URI Too Long";
        _errmsgs[415] = "Unsupported Media Type";
        _errmsgs[416] = "Range Not Satisfiable";
        _errmsgs[417] = "Expectation Failed";
        _errmsgs[418] = "I'm a teapot";
        _errmsgs[422] = "Unprocessable Entity";
        _errmsgs[425] = "Too Early";
        _errmsgs[426] = "Upgrade Required";
        _errmsgs[428] = "Precondition Required";
        _errmsgs[429] = "Too Many Requests";
        _errmsgs[431] = "Request Header Fields Too Large";
        _errmsgs[451] = "Unavailable For Legal Reasons";
        _errmsgs[500] = "Internal Server Error";
        _errmsgs[501] = "Not Implemented";
        _errmsgs[502] = "Bad Gateway";
        _errmsgs[503] = "Service Unavailable";
        _errmsgs[504] = "Gateway Timeout";
        _errmsgs[505] = "HTTP Version Not Supported";
        _errmsgs[506] = "Variant Also Negotiates";
        _errmsgs[507] = "Insufficient Storage";
        _errmsgs[508] = "Loop Detected";
        _errmsgs[510] = "Not Extended";
        _errmsgs[511] = "Network Authentication Required";
    }

    const char* msg = NULL;

    if ((-150 < err && err <= 0) || err > 0) {
        msg = strerror(abs(err));
    } else if (-600 < err && err <= -150) {
        msg = _errmsgs[abs(err)];
    }

    if (msg == NULL) {
        msg = _errmsgs[150];
    }

    return msg;
}

const char* dperr_http_detail(int http_status)
{
    http_status = abs(http_status);
    if (http_status >= 100 && http_status < 110) {
        http_status += 90;
    }

    return dperr_detail(-http_status);
}
