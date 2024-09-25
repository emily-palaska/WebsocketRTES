#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <jansson.h>
#include <unistd.h>

#define NUM_SYMBOLS 3
#define BUFFER_SIZE 1024

// Structure to hold symbol-specific file paths
typedef struct {
	char symbol[BUFFER_SIZE];
    char trade_file[BUFFER_SIZE];
    char cand_file[BUFFER_SIZE];
    char mov_file[BUFFER_SIZE];
} SymbolData;

// FUnction for the current time
long long current_time_ms() {
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return (time_now.tv_sec * 1000LL + time_now.tv_usec / 1000); // current time in ms
}

// Initialize JSON files for each symbol
void initialize_json(const char* symbol, SymbolData* data) {
    snprintf(data->symbol, BUFFER_SIZE, "%s", symbol);
    snprintf(data->trade_file, BUFFER_SIZE, "%s.json", symbol);
    snprintf(data->cand_file, BUFFER_SIZE, "%s_cand.json", symbol);
    snprintf(data->mov_file, BUFFER_SIZE, "%s_mov.json", symbol);

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
    printf("Main: Initialized %s JSON files\n", symbol);
}

// Add trade sample to the JSON file (Producer)
void add_trade_sample(const char* file_path, double price, const char* symbol, long long timestamp, double volume) {
    FILE* file = fopen(file_path, "r+");
    if (!file) return;

    json_error_t error;
    json_t* root = json_loadf(file, 0, &error);
    if (!root) {
        printf("Error loading JSON: %s\n", error.text);
        fclose(file);
        return;
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
void process_trades(const char *trade_file, const char *cand_file, const char *mov_file) {
    json_t *root, *trade, *data_array, *cand_root, *mov_root;
    json_error_t error;

    long long current_time = current_time_ms();
    long long time_threshold_1 = current_time - 60 * 1000;  // 1 minute ago
    long long time_threshold_15 = current_time - 15 * 60 * 1000;  // 15 minutes ago

    root = json_load_file(trade_file, 0, &error);
    if (!root) {
        fprintf(stderr, "Error loading JSON: %s\n", error.text);
        return;
    }

    data_array = json_object_get(root, "data");

    double prices_1[100], volumes_1[100], prices_15[100], volumes_15[100];
    size_t index_1 = 0, index_15 = 0;

    size_t index;
    json_array_foreach(data_array, index, trade) {
        long long t = json_integer_value(json_object_get(trade, "t"));
        double p = json_real_value(json_object_get(trade, "p"));
        double v = json_real_value(json_object_get(trade, "v"));
		printf("t=%lld current_time=%lld\n", t, current_time);
        if (t >= time_threshold_1 && t <= current_time) {
            prices_1[index_1] = p;
            volumes_1[index_1] = v;
            index_1++;
        }
        if (t >= time_threshold_15 && t <= current_time) {
            prices_15[index_15] = p;
            volumes_15[index_15] = v;
            index_15++;
        }
    }
	printf("Found sizes %zu and %zu\n", index_1, index_15);
    if (index_1 > 0) {
        // Process candlestick data
        cand_root = json_load_file(cand_file, 0, &error);
        if (!cand_root) {
            fprintf(stderr, "Error loading candlestick JSON: %s\n", error.text);
            json_decref(root);
            return;
        }

        json_t *cand_data = json_object_get(cand_root, "data");
        json_t *cand_entry = json_object();
        json_object_set_new(cand_entry, "open", json_real(prices_1[0]));
        json_object_set_new(cand_entry, "close", json_real(prices_1[index_1 - 1]));
        json_object_set_new(cand_entry, "high", json_real(prices_1[0]));
        json_object_set_new(cand_entry, "low", json_real(prices_1[0]));
        json_object_set_new(cand_entry, "v", json_real(volumes_1[0]));
        json_object_set_new(cand_entry, "t", json_integer(current_time));
        json_array_append_new(cand_data, cand_entry);

        FILE *file = fopen(cand_file, "w");
        if (file != NULL) {
            json_dumpf(cand_root, file, JSON_INDENT(4));
            fclose(file);
        }

        json_decref(cand_root);
    }

    if (index_15 > 0) {
        // Process moving average data
        mov_root = json_load_file(mov_file, 0, &error);
        if (!mov_root) {
            fprintf(stderr, "Error loading moving average JSON: %s\n", error.text);
            json_decref(root);
            return;
        }

        json_t *mov_data = json_object_get(mov_root, "data");
        double sum_prices = 0;
        for (size_t i = 0; i < index_15; i++) {
            sum_prices += prices_15[i];
        }
        json_t *mov_entry = json_object();
        json_object_set_new(mov_entry, "p", json_real(sum_prices / index_15));
        json_object_set_new(mov_entry, "v", json_real(volumes_15[0]));
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
}

// Producer thread function
void* producer_thread(void* arg) {
    SymbolData* data = (SymbolData*)arg;

    while (1) {
        long long timestamp = current_time_ms();
        add_trade_sample(data->trade_file, 7296.89, data->symbol, timestamp, 0.011467);
		printf("%s producer: Added trade to file\n", data->symbol);
		sleep(1);
    }
    return NULL;
}

// Consumer thread function
void* consumer_thread(void* arg) {
    SymbolData* data = (SymbolData*)arg;

    while (1) {
        process_trades(data->trade_file, data->cand_file, data->mov_file);
        printf("%s consumer: Processed the last trades\n", data->symbol);
        sleep(3); // Processing trades every minute
    }
    return NULL;
}

int main() {
    pthread_t producers[NUM_SYMBOLS], consumers[NUM_SYMBOLS];
    const char *symbol_names[3] = {"AAPL", "GOOG", "MSFT"};
    SymbolData symbols[NUM_SYMBOLS] = {
        {"", "", "", ""},
        {"", "", "", ""},
        {"", "", "", ""}
    };
    // Initialize JSON files for each symbol
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        initialize_json(symbol_names[i], &symbols[i]);   
    }
    
    
    // Start threads
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        pthread_create(&producers[i], NULL, producer_thread, &symbols[i]);
        pthread_create(&consumers[i], NULL, consumer_thread, &symbols[i]);
    }

    // Wait for threads to complete (they won't, in this example)
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        pthread_join(producers[i], NULL);
        pthread_join(consumers[i], NULL);
    }
	return 0;
}
