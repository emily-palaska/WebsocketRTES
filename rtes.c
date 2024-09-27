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
#define NUM_THREADS 3 // the maximum number of trades that can be handled at once
#define NUM_SYMBOLS 3
#define BUFFER_SIZE 1024

// Global mutex
pthread_mutex_t mutex;

// Websocket state flags
static int destroy_flag = 0; // destroy flag
static int connection_flag = 0; // connection flag
static int writeable_flag = 0; // writeable flag

// This function sets the destroy flag to 1 when the SIGINT signal (Ctr+C) is received
// This is used to close the websocket connection and free the memory
static void interrupt_handler(int signal) {
    destroy_flag = 1;
    printf("[Main] Program terminated.\n");
}

// FUnction for the current time
long long current_time_ms() {
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return (time_now.tv_sec * 1000LL + time_now.tv_usec / 1000); // current time in ms
}

// Structure to hold symbol-specific file paths
typedef struct {
	char symbol[BUFFER_SIZE];
    char trade_file[BUFFER_SIZE];
    char cand_file[BUFFER_SIZE];
    char mov_file[BUFFER_SIZE];
	float price;
	long long timestamp;
	float volume;
} SymbolData;

typedef struct {
	int id;
	float price;
	long long timestamp;
	float volume;
} TradeData;

// An array of SymbolData and the producer consumer threads
SymbolData symbols[NUM_SYMBOLS];
pthread_t producers[NUM_THREADS], consumers[NUM_SYMBOLS];

// Initialize JSON files for each symbol
void initialize_json(const char* symbol, SymbolData* data);

// Add trade sample to the JSON file (Producer)
void add_trade_sample(const char* file_path, double price, const char* symbol, long long timestamp, double volume);

// Process trades (Consumer)
int process_trades(const char *trade_file, const char *cand_file, const char *mov_file);

// FUnction for the current time
long long current_time_ms();

// Producer thread function
void* producer_thread(void* arg) {
    pthread_mutex_lock(&mutex);
    TradeData* data = (TradeData*)arg;
    const char* trade_file = symbols[data->id].trade_file;
    const char* symbol = symbols[data->id].symbol;
    add_trade_sample(trade_file, data->price, symbol, data->timestamp, data->volume);
	free(data);  // Free the dynamically allocated memory
	pthread_mutex_unlock(&mutex);
	printf("[%s producer] Added trade to %s\n", symbol, trade_file);
    
    return NULL;
}

// Consumer thread function
void* consumer_thread(void* arg) {
	int id = *(int*)arg;
	free(arg);
	while(!destroy_flag) {
		sleep(60);
		pthread_mutex_lock(&mutex);
		int index_1 = process_trades(symbols[id].trade_file, symbols[id].cand_file, symbols[id].mov_file);
		pthread_mutex_unlock(&mutex);
		printf("[%s consumer] Processed %d trades\n", symbols[id].symbol, index_1);
    }
    return NULL;
}

// This function sends a message to the websocket
static void websocket_write_back(struct lws *wsi) {
	//Check if the websocket instance is NULL
    if (wsi == NULL){
        printf("[Websocket write back] Websocket instance is NULL.\n");
        return;
    }

    char *out = NULL;
    char str[BUFFER_SIZE + 34];
	
	pthread_mutex_lock(&mutex);
    for(int i = 0; i < NUM_SYMBOLS; i++){
        snprintf(str, sizeof(str), "{\"type\":\"subscribe\",\"symbol\":\"%s\"}\n", symbols[i].symbol);
        //Printing the subscription request
        printf("Websocket write back: %s\n", str);
        int len = strlen(str);
        out = (char *)malloc(sizeof(char)*(LWS_SEND_BUFFER_PRE_PADDING + len + LWS_SEND_BUFFER_POST_PADDING));
        memcpy(out + LWS_SEND_BUFFER_PRE_PADDING, str, len);
        lws_write(wsi, (unsigned char *) (out+LWS_SEND_BUFFER_PRE_PADDING), len, LWS_WRITE_TEXT);
    }
    pthread_mutex_unlock(&mutex);
    
    //Free the memory
    free(out);
}

static int ws_callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

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

// A callback function that handles different websocket events
static int ws_callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
    	//This case is called when the connection is established
    	case LWS_CALLBACK_CLIENT_ESTABLISHED:
    		printf("[Main Service] Successful Client Connection.\n");
            //Set flags
            connection_flag = 1;
            break;
        //This case is called when there is an error in the connection
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            printf("[Main Service] Client Connection Error: %s.\n", (char *)in);
            //Set flags
            destroy_flag = 1;
            connection_flag = 0;
            break;
        //This case is called when the client receives a message from the websocket
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("[Main Service] The Client received a message:%s\n", (char *)in);
            
            // Parse the received message
            json_t *root;
            json_error_t error;
            root = json_loads((char *)in, 0, &error);
            if (!root) {
                printf("Error: on line %d: %s\n", error.line, error.text);
                break;
            }
			
			json_t *type = json_object_get(root, "type");
			if (type && json_is_string(type)) {
				const char *type_str = json_string_value(type);
				if (strcmp(type_str, "ping") == 0) {
					json_decref(root);
					break;
				}
			}
			
            json_t *data = json_object_get(root, "data");
            if (!json_is_array(data)) {
                json_decref(root);
                break;
            }

            size_t index;
            json_t *value;
            int tid = 0;
            json_array_foreach(data, index, value) {
                const char *symbol = json_string_value(json_object_get(value, "s"));
                double price = json_number_value(json_object_get(value, "p"));
                double volume = json_number_value(json_object_get(value, "v"));
                long long timestamp = json_integer_value(json_object_get(value, "t"));
                for (int i = 0; i < NUM_SYMBOLS; i++) {
                    if (strcmp(symbols[i].symbol, symbol) == 0) {
                    	// Make a dynamically allocated copy of the data to avoid overriding
						TradeData* temp = malloc(sizeof(TradeData));  // Allocate memory for the struct
						temp->id = i;                            
						temp->price = price;
						temp->volume = volume;
						temp->timestamp = timestamp;

						int result = pthread_create(&producers[tid], NULL, producer_thread, (void *)temp);
						// Check if thread creation was successful
						if (result != 0) {
							fprintf(stderr, "Error creating thread: %d\n", result);
							free(temp);
						}
						
						tid++;
                    }
                    // Exit loop when maximum amount of threads is created to avoid stack smashing
                	if (tid >= NUM_THREADS) break;
                    
                }
                // Exit loop when maximum amount of threads is created to avoid stack smashing
                if (tid >= NUM_THREADS) break;
            }
            
            json_decref(root);
            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf("[Main Service] The websocket is writeable.\n");
            //Subscribe to the symbols
            websocket_write_back(wsi);
            //Set flags
            writeable_flag = 1;
            break;
		
		
        // This case is called when the connection is closed
        case LWS_CALLBACK_CLIENT_CLOSED:
            printf("[Main Service] WebSocket connection closed. Attempting to reconnect...\n");
            destroy_flag = 1;
            
            break;

            
        default:
            break;
    }
    return 0;
}



int main(int argc, char **argv) {
	// Initialize the mutex
    pthread_mutex_init(&mutex, NULL);
    
	// Register the signal SIGINT handler
    struct sigaction act;
    act.sa_handler = interrupt_handler;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction( SIGINT, &act, 0);

	// Initialize JSON files for each symbol
    const char *symbol_names[3] = {"AAPL", "GOOG", "MSFT"};    
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        initialize_json(symbol_names[i], &symbols[i]);   
    }
    
    // Initialize websocket structs
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
    
    // Start consumer threads
    for (int i = 0; i < NUM_SYMBOLS; i++) {
    	int* id = malloc(sizeof(int));
    	*id = i;
        pthread_create(&consumers[i], NULL, consumer_thread, id);
    }
    
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

// Initialize JSON files for each symbol
void initialize_json(const char* symbol, SymbolData* data) {
    snprintf(data->symbol, BUFFER_SIZE, "%s", symbol);
    snprintf(data->trade_file, BUFFER_SIZE, "%s.json", symbol);
    snprintf(data->cand_file, BUFFER_SIZE, "%s_cand.json", symbol);
    snprintf(data->mov_file, BUFFER_SIZE, "%s_mov.json", symbol);
	
	/*
    const char* types[] = {"trade", "candlestick", "moving_average"};
    const char* file_names[] = {data->trade_file, data->cand_file, data->mov_file};
	
	
	// Loop to delete existing files if they exist
    for (int i = 0; i < 3; i++) {
        if (remove(file_names[i]) == 0) {
            printf("Main: Deleted existing file %s\n", file_names[i]);
        } else {
            printf("Main: No existing file %s to delete\n", file_names[i]);
        }
    }

	// Loop to create and initialize new JSON files
    for (int i = 0; i < 3; i++) {
        json_t* json_obj = json_object();
        json_object_set_new(json_obj, "type", json_string(types[i]));
        json_object_set_new(json_obj, "data", json_array());

        FILE* file = fopen(file_names[i], "w");
        if (file) {
            json_dumpf(json_obj, file, JSON_INDENT(4));
            fclose(file);
        }
        json_decref(json_obj);
    }
    */
    printf("Main: Initialized %s JSON files\n", symbol);
}

// Add trade sample to the JSON file (Producer)
void add_trade_sample(const char* file_path, double price, const char* symbol, long long timestamp, double volume) {
    FILE* file = fopen(file_path, "r+");
    if (!file) return;

    json_error_t error;
    json_t* root = json_loadf(file, 0, &error);
    if (!root) {
        printf("Error loading %s JSON: %s\n", file_path, error.text);
        fclose(file);
        exit(1);
    }

    json_t* data_array = json_object_get(root, "data");
    json_t* trade = json_pack("{s:f, s:s, s:I, s:f, s:i}",
                              "p", price, "s", symbol, "t", timestamp, "v", volume, "d", 0);

    json_array_append_new(data_array, trade);
    fseek(file, 0, SEEK_SET);
    json_dumpf(root, file, JSON_INDENT(4));

    json_decref(root);
    fclose(file);
}

// Process trades (Consumer)
int process_trades(const char *trade_file, const char *cand_file, const char *mov_file) {
    json_t *root, *trade, *data_array, *cand_root, *mov_root;
    json_error_t error;

    long long current_time = current_time_ms();
    long long time_threshold_1 = current_time - 60 * 1000;  // 1 minute ago
    long long time_threshold_15 = current_time - 15 * 60 * 1000;  // 15 minutes ago

    root = json_load_file(trade_file, 0, &error);
    if (!root) {
        fprintf(stderr, "Error loading %s JSON: %s\n", trade_file, error.text);
        exit(1);
    }

    data_array = json_object_get(root, "data");

    double prices_1[BUFFER_SIZE], volumes_1[BUFFER_SIZE], prices_15[BUFFER_SIZE], volumes_15[BUFFER_SIZE];
    size_t index_1 = 0, index_15 = 0;

    size_t index;
    json_array_foreach(data_array, index, trade) {
        long long t = json_integer_value(json_object_get(trade, "t"));
        double p = json_real_value(json_object_get(trade, "p"));
        double v = json_real_value(json_object_get(trade, "v"));
        if (index_1 < BUFFER_SIZE && t >= time_threshold_1 && t <= current_time) {
            prices_1[index_1] = p;
            volumes_1[index_1] = v;
            index_1++;
        }
        if (index_15 < BUFFER_SIZE && t >= time_threshold_15 && t <= current_time) {
            prices_15[index_15] = p;
            volumes_15[index_15] = v;
            index_15++;
        }
    }

    if (index_1 > 0) { // Process candlestick data
        // Compute high, low, and volume for the last minute
        double high_price = prices_1[0];
        double low_price = prices_1[0];
        double total_volume = volumes_1[0];
        for (size_t i = 1; i < index_1; i++) {
            if (prices_1[i] > high_price) {
                high_price = prices_1[i];
            }
            if (prices_1[i] < low_price) {
                low_price = prices_1[i];
            }
            total_volume += volumes_1[i];
        }

        cand_root = json_load_file(cand_file, 0, &error);
        if (!cand_root) {
            fprintf(stderr, "Error loading candlestick JSON: %s\n", error.text);
            json_decref(root);
            exit(1);
        }

        json_t *cand_data = json_object_get(cand_root, "data");
        json_t *cand_entry = json_object();
        json_object_set_new(cand_entry, "open", json_real(prices_1[0]));
        json_object_set_new(cand_entry, "close", json_real(prices_1[index_1 - 1]));
        json_object_set_new(cand_entry, "high", json_real(high_price));
        json_object_set_new(cand_entry, "low", json_real(low_price));
        json_object_set_new(cand_entry, "v", json_real(total_volume));
        json_object_set_new(cand_entry, "t", json_integer(current_time));
        json_array_append_new(cand_data, cand_entry);

        FILE *file = fopen(cand_file, "w");
        if (file != NULL) {
            json_dumpf(cand_root, file, JSON_INDENT(4));
            fclose(file);
        }

        json_decref(cand_root);
    }

    if (index_15 > 0) { // Process moving average data
    	double total_volume = volumes_15[0];
        for (size_t i = 1; i < index_15; i++) {
            total_volume += volumes_15[i];
        }
        
        mov_root = json_load_file(mov_file, 0, &error);
        if (!mov_root) {
            fprintf(stderr, "Error loading moving average JSON: %s\n", error.text);
            json_decref(root);
            exit(1);
        }

        json_t *mov_data = json_object_get(mov_root, "data");
        double sum_prices = 0;
        for (size_t i = 0; i < index_15; i++) {
            sum_prices += prices_15[i];
        }
        json_t *mov_entry = json_object();
        json_object_set_new(mov_entry, "p", json_real(sum_prices / index_15));
        json_object_set_new(mov_entry, "v", json_real(total_volume));
        json_object_set_new(mov_entry, "t", json_integer(current_time));
        json_object_set_new(mov_entry, "d", json_integer(current_time_ms() - current_time));
        json_array_append_new(mov_data, mov_entry);

        FILE *file = fopen(mov_file, "w");
        if (file != NULL) {
            json_dumpf(mov_root, file, JSON_INDENT(4));
            fclose(file);
        }

        json_decref(mov_root);
    }
    
    json_decref(root);
    return index_1;
}
