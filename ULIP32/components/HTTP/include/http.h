#pragma once
#include "stdbool.h"
// #define HTTP_STATUS_GENERIC_ERROR  -1   // In case of TCP or DNS error the callback is called with this status.
// #define BUFFER_SIZE_MAX            5000 // Size of http responses that will cause an error.

// /*
//  * "full_response" is a string containing all response headers and the response body.
//  * "response_body and "http_status" are extracted from "full_response" for convenience.
//  *
//  * A successful request corresponds to an HTTP status code of 200 (OK).
//  * More info at http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
//  */
typedef void (*http_callback)(char *url, char *response_body, int http_status,
                              char *response_header_key,char *response_header_value, int body_size);
//  void http_init(const char *agent);

// /*
//  * Download a web page from its URL.
//  * Try:
//  * http_get("http://wtfismyip.com/text", http_callback_example);
//  */
// void http_get(const char *url, const char *headers,
//                                 http_callback user_callback);

// /*
//  * Post data to a web form.
//  * The data should be encoded as application/x-www-form-urlencoded.
//  * Try:
//  * http_post("http://httpbin.org/post", "first_word=hello&second_word=world", http_callback_example);
//  */
// void http_post(const char *url, const char *post_data,
//                                  const char *headers, http_callback user_callback);

// /*
//  * Call this function to skip URL parsing if the arguments are already in separate variables.
void start_http_client();
void http_raw_request(const char *hostname, int port, bool secure,
                                        const char *user, const char *passwd, const char *path, const char *post_data,
                                        const char *header_key, const char *header_value, int retries, http_callback user_callback);

