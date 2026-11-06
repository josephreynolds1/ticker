#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "config.h"

#define VERSION "1.0.0"

// Exit Code Definitions
#define EXIT_SUCCESS_CODE 0
#define EXIT_INIT_ERROR   1
#define EXIT_NETWORK_ERR  2

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int fetch_url(const char *url, const char *cookie_file, struct MemoryStruct *chunk) {
    CURL *curl_handle;
    CURLcode res;

    chunk->memory = malloc(1);
    if (!chunk->memory) return -1;
    chunk->memory[0] = '\0';
    chunk->size = 0;

    curl_handle = curl_easy_init();
    if(!curl_handle) {
        free(chunk->memory);
        chunk->memory = NULL;
        return -1;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.5");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);

    if (cookie_file) {
        curl_easy_setopt(curl_handle, CURLOPT_COOKIEJAR, cookie_file);
        curl_easy_setopt(curl_handle, CURLOPT_COOKIEFILE, cookie_file);
    }

    res = curl_easy_perform(curl_handle);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        free(chunk->memory);
        chunk->memory = NULL;
        chunk->size = 0;
        return -1;
    }

    return 0;
}

double get_symbol_val(const char *json, const char *symbol, const char *key) {
    const char *result_pos = strstr(json, "\"quoteResponse\":{\"result\":[");
    if (!result_pos) result_pos = strstr(json, "\"result\":[");
    const char *search_start = result_pos ? result_pos : json;

    char sym_pattern[128];
    snprintf(sym_pattern, sizeof(sym_pattern), "\"symbol\":\"%s\"", symbol);
    
    const char *sym_pos = strstr(search_start, sym_pattern);
    if (!sym_pos) return 0.0;

    const char *obj_start = sym_pos;
    while (obj_start > search_start && *obj_start != '{') {
        obj_start--;
    }

    const char *obj_end = obj_start;
    int brace_depth = 0;
    while (*obj_end != '\0') {
        if (*obj_end == '{') brace_depth++;
        else if (*obj_end == '}') {
            brace_depth--;
            if (brace_depth == 0) {
                obj_end++;
                break;
            }
        }
        obj_end++;
    }

    size_t len = obj_end - obj_start;
    if (len >= 8192) len = 8191;
    char obj_buf[8192];
    memcpy(obj_buf, obj_start, len);
    obj_buf[len] = '\0';

    char key_pattern[128];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\":", key);
    
    const char *key_pos = strstr(obj_buf, key_pattern);
    if (!key_pos) return 0.0;

    const char *raw_pos = strstr(key_pos, "{\"raw\":");
    if (raw_pos && raw_pos - key_pos < 30) {
        return atof(raw_pos + 7);
    }

    return atof(key_pos + strlen(key_pattern));
}

void get_symbol_str(const char *json, const char *symbol, const char *key, char *dest, size_t dest_size) {
    dest[0] = '\0';
    const char *result_pos = strstr(json, "\"quoteResponse\":{\"result\":[");
    if (!result_pos) result_pos = strstr(json, "\"result\":[");
    const char *search_start = result_pos ? result_pos : json;

    char sym_pattern[128];
    snprintf(sym_pattern, sizeof(sym_pattern), "\"symbol\":\"%s\"", symbol);
    
    const char *sym_pos = strstr(search_start, sym_pattern);
    if (!sym_pos) return;

    const char *obj_start = sym_pos;
    while (obj_start > search_start && *obj_start != '{') {
        obj_start--;
    }

    const char *obj_end = obj_start;
    int brace_depth = 0;
    while (*obj_end != '\0') {
        if (*obj_end == '{') brace_depth++;
        else if (*obj_end == '}') {
            brace_depth--;
            if (brace_depth == 0) {
                obj_end++;
                break;
            }
        }
        obj_end++;
    }

    size_t len = obj_end - obj_start;
    if (len >= 8192) len = 8191;
    char obj_buf[8192];
    memcpy(obj_buf, obj_start, len);
    obj_buf[len] = '\0';

    char key_pattern[128];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\":", key);
    
    const char *key_pos = strstr(obj_buf, key_pattern);
    if (!key_pos) return;

    const char *val_start = strchr(key_pos, '\"');
    if (!val_start) return;
    val_start++;

    const char *val_end = strchr(val_start, '\"');
    if (!val_end) return;

    size_t len_str = val_end - val_start;
    if (len_str >= dest_size) len_str = dest_size - 1;
    memcpy(dest, val_start, len_str);
    dest[len_str] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            printf("ticker %s - C stock ticker\n", VERSION);
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -h, --help     Display this help message and exit\n");
            printf("  -v, --version  Display version information and exit\n");
            printf("Exit Codes:\n");
            printf("  0              Success\n");
            printf("  1              Initialization or curl setup error\n");
            printf("  2              Network communication or API fetch failure\n");
            return EXIT_SUCCESS_CODE;
        }
        if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0) {
            printf("ticker %s\n", VERSION);
            return EXIT_SUCCESS_CODE;
        }
    }

    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
        fprintf(stderr, "Error: Failed to initialize libcurl globally\n");
        return EXIT_INIT_ERROR;
    }

    char cookie_path[512];
    char crumb_path[512];
    const char *tmpdir = getenv("TMPDIR");
    if (!tmpdir) tmpdir = "/tmp";
    snprintf(cookie_path, sizeof(cookie_path), "%s/ticker_cookies.txt", tmpdir);
    snprintf(crumb_path, sizeof(crumb_path), "%s/ticker_crumb.txt", tmpdir);

    char crumb[64] = {0};
    FILE *f_crumb = fopen(crumb_path, "r");
    if (f_crumb) {
        if (fgets(crumb, sizeof(crumb), f_crumb)) {
            size_t clen = strlen(crumb);
            if (clen > 0 && (crumb[clen-1] == '\n' || crumb[clen-1] == '\r')) {
                crumb[clen-1] = '\0';
            }
        }
        fclose(f_crumb);
    }

    if (strlen(crumb) == 0) {
        struct MemoryStruct chunk_init = {0};
        if (fetch_url("https://finance.yahoo.com/quote/AAPL", cookie_path, &chunk_init) == 0) {
            free(chunk_init.memory);
        }

        struct MemoryStruct chunk_crumb = {0};
        if (fetch_url("https://query1.finance.yahoo.com/v1/test/getcrumb", cookie_path, &chunk_crumb) == 0) {
            if (chunk_crumb.memory && chunk_crumb.size > 0 && chunk_crumb.size < sizeof(crumb)) {
                memcpy(crumb, chunk_crumb.memory, chunk_crumb.size);
                for (size_t k = 0; k < chunk_crumb.size; k++) {
                    if (crumb[k] == '\r' || crumb[k] == '\n') crumb[k] = '\0';
                }
                FILE *f_out = fopen(crumb_path, "w");
                if (f_out) {
                    fputs(crumb, f_out);
                    fclose(f_out);
                }
            }
            free(chunk_crumb.memory);
        }
    }

    size_t num_symbols = sizeof(symbols) / sizeof(symbols[0]);

    char symbols_buf[4096] = {0};
    for (size_t i = 0; i < num_symbols; i++) {
        CURL *esc_curl = curl_easy_init();
        if (esc_curl) {
            char *escaped = curl_easy_escape(esc_curl, symbols[i], 0);
            if (escaped) {
                strcat(symbols_buf, escaped);
                curl_free(escaped);
            } else {
                strcat(symbols_buf, symbols[i]);
            }
            curl_easy_cleanup(esc_curl);
        } else {
            strcat(symbols_buf, symbols[i]);
        }
        if (i < num_symbols - 1) {
            strcat(symbols_buf, ",");
        }
    }

    char url[8192];
    if (strlen(crumb) > 0) {
        snprintf(url, sizeof(url), 
            "https://query1.finance.yahoo.com/v7/finance/quote?lang=en-US&region=US&corsDomain=finance.yahoo.com&fields=symbol,marketState,regularMarketPrice,regularMarketChange,regularMarketChangePercent,preMarketPrice,preMarketChange,preMarketChangePercent,postMarketPrice,postMarketChange,postMarketChangePercent,fiftyTwoWeekHigh,fiftyTwoWeekLow&symbols=%s&crumb=%s",
            symbols_buf, crumb);
    } else {
        snprintf(url, sizeof(url), 
            "https://query1.finance.yahoo.com/v7/finance/quote?lang=en-US&region=US&corsDomain=finance.yahoo.com&fields=symbol,marketState,regularMarketPrice,regularMarketChange,regularMarketChangePercent,preMarketPrice,preMarketChange,preMarketChangePercent,postMarketPrice,postMarketChange,postMarketChangePercent,fiftyTwoWeekHigh,fiftyTwoWeekLow&symbols=%s",
            symbols_buf);
    }

    struct MemoryStruct chunk_data = {0};
    if (fetch_url(url, cookie_path, &chunk_data) != 0 || !chunk_data.memory) {
        fprintf(stderr, "Error: Failed to fetch quotes data from Yahoo Finance\n");
        curl_global_cleanup();
        return EXIT_NETWORK_ERR;
    }

    // Clear screen and reset cursor for watch compatibility
    printf("\033[H\033[J");

    printf("------------------------------------------------------------------------------------------\n");
    printf("%-10s %8s %10s %7s %13s %9s %12s %9s %3s\n", "Ticker", "Price", "Diff", "%", "1 Yr High", "%", "1 Yr Low", "%", "AH");
    printf("------------------------------------------------------------------------------------------\n");

    for (size_t i = 0; i < num_symbols; i++) {
        const char *sym = symbols[i];
        
        double regPrice = get_symbol_val(chunk_data.memory, sym, "regularMarketPrice");
        if (regPrice == 0.0) {
            printf("%-10s [No data available]\n", sym);
            continue;
        }

        double regDiff = get_symbol_val(chunk_data.memory, sym, "regularMarketChange");
        double regPct = get_symbol_val(chunk_data.memory, sym, "regularMarketChangePercent");

        double prePrice = get_symbol_val(chunk_data.memory, sym, "preMarketPrice");
        double preDiff = get_symbol_val(chunk_data.memory, sym, "preMarketChange");
        double prePct = get_symbol_val(chunk_data.memory, sym, "preMarketChangePercent");

        double postPrice = get_symbol_val(chunk_data.memory, sym, "postMarketPrice");
        double postDiff = get_symbol_val(chunk_data.memory, sym, "postMarketChange");
        double postPct = get_symbol_val(chunk_data.memory, sym, "postMarketChangePercent");

        char marketState[32];
        get_symbol_str(chunk_data.memory, sym, "marketState", marketState, sizeof(marketState));

        double price = regPrice, diff = regDiff, pct = regPct;
        const char *sign = "";

        if (strcmp(marketState, "PRE") == 0 && preDiff != 0) {
            sign = "*"; price = prePrice; diff = preDiff; pct = prePct;
        } else if (strlen(marketState) > 0 && strcmp(marketState, "REGULAR") != 0 && postDiff != 0) {
            sign = "*"; price = postPrice; diff = postDiff; pct = postPct;
        }

        double high = get_symbol_val(chunk_data.memory, sym, "fiftyTwoWeekHigh");
        double low = get_symbol_val(chunk_data.memory, sym, "fiftyTwoWeekLow");

        double high_pct = (high > 0) ? ((price - high) / high * 100) : 0;
        double low_pct = (low > 0) ? ((price - low) / low * 100) : 0;

        char diff_raw[32], pct_raw[32];
        snprintf(diff_raw, sizeof(diff_raw), "%.2f", diff);
        snprintf(pct_raw, sizeof(pct_raw), "%.2f%%", pct);

        const char *color_diff_start = "";
        const char *color_diff_end = "";
        if (diff < 0) {
            color_diff_start = "\033[1;31m";
            color_diff_end = "\033[00m";
        } else if (diff > 0) {
            color_diff_start = "\033[1;32m";
            color_diff_end = "\033[00m";
        }

        const char *color_pct_start = "";
        const char *color_pct_end = "";
        if (pct < 0) {
            color_pct_start = "\033[1;31m";
            color_pct_end = "\033[00m";
        } else if (pct > 0) {
            color_pct_start = "\033[1;32m";
            color_pct_end = "\033[00m";
        }

        char fmt_high_pct[32], fmt_low_pct[32];
        snprintf(fmt_high_pct, sizeof(fmt_high_pct), "(%.2f%%)", high_pct);
        snprintf(fmt_low_pct, sizeof(fmt_low_pct), "(%.2f%%)", low_pct);

        printf("%-10s \033[1;39m%8.2f\033[00m %s%10s%s %s%7s%s %13.2f %9s %12.2f %9s %3s\n",
            sym,
            price,
            color_diff_start, diff_raw, color_diff_end,
            color_pct_start, pct_raw, color_pct_end,
            high,
            fmt_high_pct,
            low,
            fmt_low_pct,
            sign
        );
    }

    free(chunk_data.memory);
    curl_global_cleanup();
    return EXIT_SUCCESS_CODE;
}