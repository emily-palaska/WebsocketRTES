#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libwebsockets.h>
#include <pthread.h>
#include <jansson.h>
#include <time.h>

//Number of threads which is also the number of symbols
#define NUM_THREADS 1

// Websocket state flags
static int destroy_flag = 0; // destroy flag
static int connection_flag = 0; // connection flag
static int writeable_flag = 0; // writeable flag

// This function sets the destroy flag to 1 when the SIGINT signal (Ctr+C) is received
// This is used to close the websocket connection and free the memory
static void interrupt_handler(int signal);

// A dictionary to store the latest trade data for each symbol
typedef struct {
    int thread_id;
    char symbol[20];
    double price;
    double volume;
    double timestamp;
    double enqueue_time;
} TradeData;

// An array of TradeData structures to store the latest trade data for each symbol
TradeData trades[NUM_THREADS];

// This function sends a message to the websocket
static void websocket_write_back(struct lws *wsi) {
	//Check if the websocket instance is NULL
    if (wsi == NULL){
        printf("[Websocket write back] Websocket instance is NULL.\n");
        return;
    }

    char *out = NULL;
    char str[100];

    for(int i = 0; i < NUM_THREADS; i++){
        sprintf(str, "{\"type\":\"subscribe\",\"symbol\":\"%s\"}\n", trades[i].symbol);
        //Printing the subscription request
        printf("Websocket write back: %s\n", str);
        int len = strlen(str);
        out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));
        memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);
        lws_write(wsi, out+LWS_SEND_BUFFER_PRE_PADDING, len, LWS_WRITE_TEXT);
    }
    
    //Free the memory
    free(out);

}

// A callback function that handles different websocket events
static int ws_callback_echo(struct lws *wsi, enum lws_callback_reasons reason,
                         void *user, char *in, size_t len) {
    switch (reason) {
    	//This case is called when the connection is established
    	case LWS_CALLBACK_CLIENT_ESTABLISHED:
    		printf("[Main Service] Successful Client Connection.\n");
            //Set flags
            connection_flag = 1;
            break;
        //This case is called when there is an error in the connection
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("[Main Service] Client Connection Error: %s.\n", in);
            //Set flags
            destroy_flag = 1;
            connection_flag = 0;
            break;
        //This case is called when the client receives a message from the websocket
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("[Main Service] The Client received a message:%s\n", (char *)in);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf("[Main Service] The websocket is writeable.\n");
            //Subscribe to the symbols
            websocket_write_back(wsi);
            //Set flags
            writeable_flag = 1;
            break;

        //This case is called when the connection is closed
        case LWS_CALLBACK_CLOSED:
            printf("[Main Service] Websocket connection closed\n");
            //Set flags
            destroy_flag = 1;
            connection_flag = 0;
            break;

        default:
            break;
    }
    return 0;
}

// Protocols used for the websocket
static struct lws_protocols protocols[] = {
    {
        "example", //name
        ws_callback_echo, //callback function
        0, // user data size
        0, // receive buffer size
    },
    { NULL, NULL, 0, 0 } // terminator
};

int main(int argc, char **argv) {
	// Register the signal SIGINT handler
    struct sigaction act;
    act.sa_handler = interrupt_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction( SIGINT, &act, 0);

	// Initialize the trades array with symbols
    strcpy(trades[0].symbol, "AAPL");
    
    struct lws_context *context = NULL;
    struct lws_context_creation_info info;
    struct lws *wsi = NULL;    

   	// Setting up the context creation info
    memset(&info, 0, sizeof info);
    info.port = CONTEXT_PORT_NO_LISTEN; 
    info.protocols = protocols; 
    info.gid = -1; 
    info.uid = -1;
    info.ssl_ca_filepath = "/etc/ssl/certs/ca-certificates.crt"; 
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.max_http_header_pool = 1024; // Increase if necessary
    info.pt_serv_buf_size = 4096; // Default is 4096 bytes, increase buffer size to 16 KB if needed 16384 bytes
	
	// The URL of the websocket
    char inputURL[300] = "ws.finnhub.io/?token=couu7o1r01qhf5ns046gcouu7o1r01qhf5ns0470";
    const char *urlProtocol="wss";
    const char *urlTempPath="/";
    char urlPath[300];
    
    // Creating the context using the context creation info
    context = lws_create_context(&info);
    printf("[Main] Successful context creation.\n");
    
    if (!context) {
        fprintf(stderr, "[Main] Context creation error: Context is NULL.\n");
        return -1;
    }
    
    struct lws_client_connect_info clientConnectionInfo;
    memset(&clientConnectionInfo, 0, sizeof(clientConnectionInfo));
    clientConnectionInfo.context = context;
    if (lws_parse_uri(inputURL, &urlProtocol, &clientConnectionInfo.address,
                    &clientConnectionInfo.port, &urlTempPath)){
        printf("Couldn't parse the URL\n");
    }
    
    urlPath[0] = '/';
    strncpy(urlPath + 1, urlTempPath, sizeof(urlPath) - 2);
    urlPath[sizeof(urlPath)-1] = '\0';

    //Setting up the client connection info
    clientConnectionInfo.port = 443;
    clientConnectionInfo.path = urlPath;
    clientConnectionInfo.ssl_connection = LCCSCF_USE_SSL | LCCSCF_ALLOW_SELFSIGNED | LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    clientConnectionInfo.host = clientConnectionInfo.address;
    clientConnectionInfo.origin = clientConnectionInfo.address;
    clientConnectionInfo.ietf_version_or_minus_one = -1;
    clientConnectionInfo.protocol = protocols[0].name;

    // Print the connection info
    printf("Testing %s\n\n", clientConnectionInfo.address);
    printf("Connecting to %s://%s:%d%s \n\n", urlProtocol, clientConnectionInfo.address, clientConnectionInfo.port, urlPath);

    // Create the websocket instance
    wsi = lws_client_connect_via_info(&clientConnectionInfo);
    if (wsi == NULL) {
        printf("[Main] Web socket instance creation error.\n");
        return -1;
    }

    printf("[Main] Successful web socket instance creation.\n");   
    
    while(!destroy_flag){
        // Service the WebSocket
        lws_service(context, 1000);

        // Print the flags status
        printf("Flags-Status\n");
        printf("C: %d, W: %d, D: %d\n", connection_flag, writeable_flag, destroy_flag);

    }
	
	// Destroy the websocket connection
    lws_context_destroy(context);
    return 0;
}

static void interrupt_handler(int signal){
    destroy_flag = 1;
    printf("[Main] Program terminated.\n");
}


