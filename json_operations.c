#include <stdio.h>
#include <stdlib.h>
#include <jansson.h>
#include <time.h>
#include <unistd.h>

long long current_time_ms() {
    struct timeval time_now;
    gettimeofday(&time_now, NULL);
    return (time_now.tv_sec * 1000LL + time_now.tv_usec / 1000); // current time in ms
}

// Function to initialize and overwrite the three JSON files
void initialize_json(const char *name, char *file_paths[3]) {
    const char *types[] = {"trade", "candlestick", "moving_average"};
    json_t *root;

    for (int i = 0; i < 3; i++) {
        root = json_object();
        json_object_set_new(root, "data", json_array());
        json_object_set_new(root, "type", json_string(types[i]));

        // Create filename (e.g., "AAPL.json", "AAPL_cand.json", "AAPL_mov.json")
        sprintf(file_paths[i], "%s%s", name, (i == 0) ? ".json" : (i == 1) ? "_cand.json" : "_mov.json");

        FILE *file = fopen(file_paths[i], "w");
        if (file == NULL) {
            fprintf(stderr, "Error opening file: %s\n", file_paths[i]);
            json_decref(root);
            return;
        }

        // Write JSON data to file
        json_dumpf(root, file, JSON_INDENT(4));
        fclose(file);
        json_decref(root);

        printf("Initialized file: %s\n", file_paths[i]);
    }
}

// Function to add a new trade sample to the JSON file
void add_trade_sample(const char *file_path, double price, const char *symbol, long long timestamp, double volume) {
    json_t *root, *trade;
    json_error_t error;

    // Load the JSON file
    root = json_load_file(file_path, 0, &error);
    if (!root) {
        fprintf(stderr, "Error loading JSON: %s\n", error.text);
        return;
    }

    // Create a new trade object
    trade = json_object();
    json_object_set_new(trade, "p", json_real(price));
    json_object_set_new(trade, "s", json_string(symbol));
    json_object_set_new(trade, "t", json_integer(timestamp));
    json_object_set_new(trade, "v", json_real(volume));

    // Append to the "data" array
    json_t *data_array = json_object_get(root, "data");
    json_array_append_new(data_array, trade);

    // Write updated JSON back to file
    FILE *file = fopen(file_path, "w");
    if (file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", file_path);
        json_decref(root);
        return;
    }

    json_dumpf(root, file, JSON_INDENT(4));
    fclose(file);
    json_decref(root);

    printf("Added trade sample to %s\n", file_path);
}

// Function to process trades and generate candlestick and moving average data
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
        json_object_set_new(mov_entry, "d", json_integer(current_time_ms() -current_time));
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

// Main function
int main() {
    // Define the symbol (e.g., AAPL)
    const char *symbol = "AAPL";
    char *file_paths[3];
    file_paths[0] = malloc(100);
    file_paths[1] = malloc(100);
    file_paths[2] = malloc(100);

    // Initialize the JSON files
    initialize_json(symbol, file_paths);

    // Add some trade samples
    add_trade_sample(file_paths[0], 7296.89, "AAPL", current_time_ms(), 0.011467);
    sleep(1);
    add_trade_sample(file_paths[0], 7300.12, "AAPL", current_time_ms(), 0.010500);
    sleep(2);
    add_trade_sample(file_paths[0], 7280.47, "AAPL", current_time_ms(), 0.015000);

    // Wait for a few seconds to simulate time passage
    sleep(3);

    // Process trades to update candlestick and moving average files
    process_trades(file_paths[0], file_paths[1], file_paths[2]);
    process_trades(file_paths[0], file_paths[1], file_paths[2]);
    process_trades(file_paths[0], file_paths[1], file_paths[2]);

    // Clean up memory
    free(file_paths[0]);
    free(file_paths[1]);
    free(file_paths[2]);

    return 0;
}

