/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "httpd2.h"
#include "esp_eth.h"
#include "esp_netif.h"
#include "esp_tls_crypto.h"
#include "mbedtls/base64.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <sys/param.h>
#define CONFIG_EXAMPLE_BASIC_AUTH_USERNAME "user"
#define CONFIG_EXAMPLE_BASIC_AUTH_PASSWORD "password"
#define HTTPD_MAX_CONNECTIONS 2
// #define GET_REQ_TEST 0
/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "example";
static esp_err_t (*httpd_get_cb)(httpd_req_t *req) = NULL;
static httpdConnData *connData[HTTPD_MAX_CONNECTIONS];
#if 1

#define HTTPD_401 "401 UNAUTHORIZED" /*!< HTTP Response 401 */

static char *http_auth_basic(const char *username, const char *password)
{
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info)
    {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
     */
    digest = calloc(1, 6 + n + 1);
    if (digest)
    {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

/* An HTTP GET handler */
esp_err_t basic_auth_get_handler(httpd_req_t *req)
{

    char *buf = NULL;
    size_t buf_len = 0;
    httpdConnData *basic_auth_info = req->user_ctx;
    esp_err_t rc = ESP_OK;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1)
    {
        buf = calloc(1, buf_len);
        if (!buf)
        {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        }
        else
        {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials)
        {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len))
        {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
            rc = ESP_FAIL;
        }
        else
        {
            ESP_LOGI(TAG, "Authenticated!");
            rc = ESP_OK;
        }
        free(auth_credentials);
        free(buf);
    }
    else
    {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, NULL, 0);
        rc = ESP_FAIL;
    }

    return rc;
}

static httpd_uri_t basic_auth = {
    .uri = "/basic_auth",
    .method = HTTP_GET,
    .handler = basic_auth_get_handler,
};

static void httpd_register_basic_auth(httpd_handle_t server, httpd_uri_t *uri_handler)
{
    httpdConnData *basic_auth_info = calloc(1, sizeof(httpdConnData));
    if (basic_auth_info)
    {
        basic_auth_info->username = CONFIG_EXAMPLE_BASIC_AUTH_USERNAME;
        basic_auth_info->password = CONFIG_EXAMPLE_BASIC_AUTH_PASSWORD;
        // basic_auth_info->cgiData = (void *)0;
        // ESP_LOGI("httpd", "user_uri %s", uri_handler->uri);
        // ESP_LOGI("httpd", "ctx %p", &basic_auth_info->cgiData);
        uri_handler->user_ctx = basic_auth_info;
        httpd_register_uri_handler(server, uri_handler);
    }
}

int authBasicGetUsername(httpd_req_t *req,
                         char *username, int len)
{
    char auth[AUTH_MAX_USER_LEN + AUTH_MAX_PASS_LEN + 2];
    char *user = NULL;
    char *pass = NULL;
    char hdr[512];
    long r;

    if (!req || !username)
        return -1;
    r = httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr) - 1);
    // r = httpdGetHeader(connData, "Authorization", hdr, sizeof(hdr) - 1);
    if (r == ESP_OK && strncmp(hdr, "Basic", 5) == 0)
    {
        mbedtls_base64_decode((unsigned char *)auth, sizeof(auth), &r, (const unsigned char *)hdr + 6, strlen(hdr) - 6);
        ESP_LOGI("httpd", "auth %s", auth);
        if (r < 0)
            r = 0;
        auth[r] = 0;
        user = strtok_r(auth, ":", &pass);
        strncpy(username, user, len - 1);
        username[len - 1] = '\0';
        return 0;
    }

    return -1;
}

int authBasicGetPassword(httpd_req_t *req,
                         char *password, int len)
{
    char auth[AUTH_MAX_USER_LEN + AUTH_MAX_PASS_LEN + 2];
    char *user = NULL;
    char *pass = NULL;
    char hdr[512];
    long r;

    if (!req || !password)
        return -1;
    r = httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr) - 1);
    // r = httpdGetHeader(connData, "Authorization", hdr, sizeof(hdr) - 1);
    if (r && strncmp(hdr, "Basic", 5) == 0)
    {
        // r = base64Decode(strlen(hdr) - 6, hdr + 6, sizeof(auth),
        //  (unsigned char *)auth);
        mbedtls_base64_decode((unsigned char *)auth, sizeof(auth), &r, (const unsigned char *)hdr + 6, strlen(hdr) - 6);
        if (r < 0)
            r = 0;
        auth[r] = 0;
        user = strtok_r(auth, ":", &pass);
        strncpy(password, pass, len - 1);
        password[len - 1] = '\0';
        return 0;
    }

    return -1;
}
#endif

void  httpdCgiIsDone(httpd_req_t *req)
{
    httpdConnData *conn = (httpdConnData *)req->user_ctx;
	// conn->cgi=NULL; //no need to call this anymore
	// httpdFlushSendBuffer(conn);
	//Note: Do not clean up sendBacklog, it may still contain data at this point.
	// conn->priv->headPos=0;
	// conn->post->len=-1;
	// conn->priv->flags=0;
	// if (conn->post->buff) {
	// 	free(conn->post->buff);
	// 	conn->post->buff=NULL;
	// }
	// conn->post->buffLen=0;
	// conn->post->received=0;
	// conn->hostName=NULL;
	// //Cannot re-use this connection. Mark to get it killed after all data is sent.
	// conn->priv->flags|=HFL_DISCONAFTERSENT;
}

// Can be called after a CGI function has returned HTTPD_CGI_MORE to
// resume handling an open connection asynchronously
void httpdContinue(httpd_req_t *req)
{
    ESP_LOGI(TAG, "httpdContinue");
    int r;
    httpdConnData *conn = (httpdConnData *)req->user_ctx;
    if (req == NULL)
    {
        ESP_LOGD("HTTPD", "Connection is null!");
        return;
    }

    // if (conn->priv->sendBacklog != NULL) {
    // 	//We have some backlog to send first.
    // 	HttpSendBacklogItem *next = conn->priv->sendBacklog->next;
    // 	httpdPlatSendData(conn->conn, conn->priv->sendBacklog->data, conn->priv->sendBacklog->len);
    // 	conn->priv->sendBacklogSize -= conn->priv->sendBacklog->len;
    // 	free(conn->priv->sendBacklog);
    // 	conn->priv->sendBacklog = next;
    // 	httpdPlatUnlock();
    // 	return;
    // }

    // if (conn->priv->flags & HFL_DISCONAFTERSENT) { //Marked for destruction?
    // 	os_debug("HTTPD", "Connection slot %d is done", conn->slot);
    // 	httpdPlatDisconnect(conn->conn);
    // 	httpdPlatUnlock();
    // 	return; //No need to call httpdFlushSendBuffer.
    // }

    // //If we don't have a CGI function, there's nothing to do but wait for something from the client.
    // if (conn->cgi == NULL) {
    // 	httpdPlatUnlock();
    // 	return;
    // }

    // if (!conn->priv->sendBuff) {
    //     conn->priv->sendBuff = malloc(HTTPD_MAX_SENDBUFF_LEN);
    //     if (conn->priv->sendBuff == NULL) {
    //         os_warning("HTTPD", "Buffer allocation failed!");
    //         httpdPlatDisconnect(conn->conn);
    //         httpdPlatUnlock();
    //         return;
    //     }
    //     conn->priv->sendBuffLen = 0;
    // }

    // r = conn->cgi(conn); // Execute cgi fn.
    r = (*(httpd_get_cb))(req);
    if (r == HTTPD_CGI_DONE)
    {
        httpdCgiIsDone(conn);
    }
    else if (r == HTTPD_CGI_MORE)
    {
        /* Wait to complete request */
        if (conn->timeout)
        {
            esp_timer_create_args_t timer_args = {
                .callback = &httpdContinue,
                .arg = req,
                .name = "httpd_timeout_cb"};
            esp_timer_create(&timer_args, &conn->timer);
            esp_timer_start_once(conn->timer, conn->timeout * 1000);
            return;
        }
    }
    if (r == HTTPD_CGI_NOTFOUND || r == HTTPD_CGI_AUTHENTICATED)
    {
        printf("ERROR! CGI fn returns code %d after sending data! Bad CGI!\n", r);
        httpdCgiIsDone(conn);
    }
}

/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    int rc;
    httpdConnData *conn = (httpdConnData *)req->user_ctx;
    if (httpd_get_cb != NULL)
    {
        rc = (*(httpd_get_cb))(req);
        if (rc == ESP_OK)
        {
            return ESP_OK;
        }
        else if (rc == HTTPD_CGI_MORE)
        {
            conn->timeout = 10;
            if (conn->timeout)
            {
                esp_timer_create_args_t timer_args = {
                    .callback = &httpdContinue,
                    .arg = req,
                    .name = "httpd_timeout_cb"};
                esp_timer_create(&timer_args, &conn->timer);
                // esp_timer_start_once(conn->timer, conn->timeout * 1000);
                return ESP_OK;
            }
        }
    }
    else
    {
        ESP_LOGE(TAG, "user_hw_timer_cb is NULL");
    }
#ifdef GET_REQ_TEST
    esp_err_t rc = basic_auth_get_handler(req);
    // ESP_LOGE("httpd", "uri %s", req->uri);
    if (rc != ESP_OK)
        return rc;
    char *buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-2") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-2", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Test-Header-2: %s", buf);
        }
        free(buf);
    }

    buf_len = httpd_req_get_hdr_value_len(req, "Test-Header-1") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Test-Header-1", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found header => Test-Header-1: %s", buf);
        }
        free(buf);
    }

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char param[32];
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "query1", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query1=%s", param);
            }
            if (httpd_query_key_value(buf, "query3", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query3=%s", param);
            }
            if (httpd_query_key_value(buf, "query2", param, sizeof(param)) == ESP_OK)
            {
                ESP_LOGI(TAG, "Found URL query parameter => query2=%s", param);
            }
        }
        free(buf);
    }

    /* Set some custom headers */
    httpd_resp_set_hdr(req, "Custom-Header-1", "Custom-Value-1");
    httpd_resp_set_hdr(req, "Custom-Header-2", "Custom-Value-2");

    /* Send response with custom headers and body set as the
     * string passed in user context*/
    const char *resp_str = "maicon boiola\n"; //(const char *)req->user_ctx;
    httpd_resp_send(req, INDEXREDE, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0)
    {
        ESP_LOGI(TAG, "Request headers lost");
    }
#endif
    return ESP_OK;
}

static httpd_uri_t hello = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = hello_get_handler};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        /* Read the data for the request */
        if ((ret = httpd_req_recv(req, buf,
                                  MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                /* Retry receiving if timeout occurred */
                continue;
            }
            return ESP_FAIL;
        }

        /* Send back the same data */
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        /* Log data received */
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static httpd_uri_t echo = {
    .uri = "/",
    .method = HTTP_POST,
    .handler = hello_get_handler};

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    }
    else if (strcmp("/echo", req->uri) == 0)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

/* An HTTP PUT handler. This demonstrates realtime
 * registration and deregistration of URI handlers
 */
static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf;
    int ret;

    if ((ret = httpd_req_recv(req, &buf, 1)) <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (buf == '0')
    {
        /* URI handlers can be unregistered using the uri string */
        ESP_LOGI(TAG, "Unregistering /hello and /echo URIs");
        httpd_unregister_uri(req->handle, "/hello");
        httpd_unregister_uri(req->handle, "/echo");
        /* Register the custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    else
    {
        ESP_LOGI(TAG, "Registering /hello and /echo URIs");
        httpd_register_uri_handler(req->handle, &hello);
        httpd_register_uri_handler(req->handle, &echo);
        /* Unregister custom error handler */
        httpd_register_err_handler(req->handle, HTTPD_404_NOT_FOUND, NULL);
    }

    /* Respond with empty body */
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t ctrl = {
    .uri = "/ctrl",
    .method = HTTP_PUT,
    .handler = ctrl_put_handler,
    .user_ctx = NULL};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 4096 * 4;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_basic_auth(server, &hello);
        // httpd_register_uri_handler(server, &echo);
        httpd_register_uri_handler(server, &ctrl);
        httpd_register_basic_auth(server, &basic_auth);
        httpd_register_basic_auth(server, &echo);
#if 1
#endif
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

void start_httpd(esp_err_t (*httpd_get_cb_set)(httpd_req_t *req))
{
    httpd_get_cb = httpd_get_cb_set;
    static httpd_handle_t server = NULL;
    /* Start the server for the first time */
    server = start_webserver();
}
