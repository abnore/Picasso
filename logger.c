// Standard library includes for system calls and I/O
#include <stdlib.h>     // getenv, strdup, malloc, free
#include <string.h>     // strtok, strcasecmp
#include <stdio.h>      // fprintf, fopen, stdout, stderr
#include <stdarg.h>     // va_list, va_start, va_end, vsnprintf
#include <stdbool.h>    // bool type
#include <time.h>       // time, localtime, strftime
#include <unistd.h>     // isatty, fileno
#include <pthread.h>    // pthread_mutex_t for thread safety

#include "logger.h"

//-------------------------------------------------------------
// Internal state & synchronization
//-------------------------------------------------------------

static pthread_mutex_t logger_mutex = PTHREAD_MUTEX_INITIALIZER; // Ensures thread-safe logging
static bool logger_initialized = false;                          // Prevent multiple init_log() calls

static void configure_log_levels_from_env(void); // Internal only — parsed by init_log()

FILE* log_output_stream = NULL;                  // Destination: stdout, stderr, or file
uint32_t log_levels_enabled = LOG_LEVEL_ALL;     // Bitmask for enabled log levels

static bool log_colors_enabled = true;           // Whether colors are active
static bool log_color_auto = true;               // True = auto-detect from TTY, false = forced

// String labels and color codes for each level
static const char* level_strings[] = { "FATAL", "ERROR", "WARN", "INFO", "DEBUG", "TRACE" };
static const char* level_colors[] = {
    "\033[1;31m", // Bright red
    "\033[0;31m", // Red
    "\033[0;33m", // Yellow
    "\033[0;32m", // Green
    "\033[0;36m", // Cyan
    "\033[0;90m"  // Gray
};
static const char* color_reset = "\033[0m";

//-------------------------------------------------------------
// Public API
//-------------------------------------------------------------

// Initializes the logger system.
// Only runs once safely even across threads.
log_type init_log(const char* filepath, bool enable_colors)
{
    pthread_mutex_lock(&logger_mutex);

    if (logger_initialized) {
        pthread_mutex_unlock(&logger_mutex);
        fprintf(stderr, "[LOGGER] init_log() called more than once — skipping\n");
        return LOG_ALREADY_INIT;
    }

    logger_initialized = true;

    // Setup initial state
    log_levels_enabled = LOG_LEVEL_ALL;
    configure_log_levels_from_env(); // Parse LOG_LEVELS=... if present

    log_output_stream = NULL;
    log_color_auto = true;

    // Use stdout if no file is specified
    if (!filepath) {
        log_output_stream = stdout;
        log_colors_enabled = enable_colors && isatty(fileno(stdout));
        pthread_mutex_unlock(&logger_mutex);
        return LOG_STDOUT;
    }

    // Try opening log file
    log_output_stream = fopen(filepath, "w");
    if (!log_output_stream) {
        fprintf(stderr, "[LOGGER ERROR] Failed to open log file: %s\n", filepath);
        pthread_mutex_unlock(&logger_mutex);
        return LOG_ERROR;
    }

    log_colors_enabled = false; // Never color file output
    pthread_mutex_unlock(&logger_mutex);
    return LOG_FILE;
}

// Gracefully shuts down logger and closes any file
void shutdown_log(void)
{
    pthread_mutex_lock(&logger_mutex);

    if (log_output_stream) {
        fflush(log_output_stream);

        if (log_output_stream != stdout &&
            log_output_stream != stderr) {
            fclose(log_output_stream);
        }

        log_output_stream = NULL;
    }

    pthread_mutex_unlock(&logger_mutex);
}

// Manually enable/disable color output
void log_set_color_output(bool enabled)
{
    log_color_auto = false;
    log_colors_enabled = enabled;
}

//-------------------------------------------------------------
// Core logging function
//-------------------------------------------------------------

void log_output_ext(log_level level, const char* file, int line, const char* func, const char* msg, ...)
{
    // Quick filter to skip if this level is off
    if (!(log_levels_enabled & level)) return;

    pthread_mutex_lock(&logger_mutex);

    if (!log_output_stream)
        log_output_stream = stdout;

    // Timestamp (HH:MM:SS)
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    char timestamp[16];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", t);

    // Format the user message
    char user_msg[32000];
    va_list args;
    va_start(args, msg);
    vsnprintf(user_msg, sizeof(user_msg), msg, args);
    va_end(args);

    // Map log level to array index
    int index = 0;
    switch (level) {
        case LOG_LEVEL_FATAL: index = 0; break;
        case LOG_LEVEL_ERROR: index = 1; break;
        case LOG_LEVEL_WARN:  index = 2; break;
        case LOG_LEVEL_INFO:  index = 3; break;
        case LOG_LEVEL_DEBUG: index = 4; break;
        case LOG_LEVEL_TRACE: index = 5; break;
        default:              index = 0; break;
    }

    // Print final formatted message with optional color
    fprintf(log_output_stream,
        "%s[%s] [%s] %s:%d (%s): %s%s\n",
        log_colors_enabled ? level_colors[index] : "",
        timestamp,
        level_strings[index],
        file,
        line,
        func,
        user_msg,
        log_colors_enabled ? color_reset : "");

    fflush(log_output_stream);
    pthread_mutex_unlock(&logger_mutex);
}

//-------------------------------------------------------------
// Assertion support (non-crashing)
//-------------------------------------------------------------

void report_assertion_failure(const char* expression, const char* message, const char* file, int line)
{
    log_output_ext(LOG_LEVEL_FATAL, file, line, "ASSERT", "Assertion failed: %s — %s", expression, message);
}

//-------------------------------------------------------------
// Environment variable parsing: LOG_LEVELS=+INFO,-TRACE,...
//-------------------------------------------------------------

void configure_log_levels_from_env(void)
{
    const char* env = getenv("LOG_LEVELS");
    if (!env) return;

    char* input = strdup(env); // Make writable copy
    char* token = strtok(input, ",");

    bool cleared = false; // Have we wiped previous mask?

    while (token) {
        while (*token == ' ') token++; // Trim space

        bool disable = false;
        if (*token == '-') {
            disable = true;
            token++;
        } else if (*token == '+') {
            token++;
        }

        // Special values
        if (strcasecmp(token, "ALL") == 0) {
            log_levels_enabled = LOG_LEVEL_ALL;
            cleared = true;
        } else if (strcasecmp(token, "NONE") == 0) {
            log_levels_enabled = LOG_LEVEL_NONE;
            cleared = true;
        } else {
            // Lookup matching log level
            uint32_t level_bit = 0;
            if      (strcasecmp(token, "FATAL") == 0)  level_bit = LOG_LEVEL_FATAL;
            else if (strcasecmp(token, "ERROR") == 0)  level_bit = LOG_LEVEL_ERROR;
            else if (strcasecmp(token, "WARN")  == 0)  level_bit = LOG_LEVEL_WARN;
            else if (strcasecmp(token, "INFO")  == 0)  level_bit = LOG_LEVEL_INFO;
            else if (strcasecmp(token, "DEBUG") == 0)  level_bit = LOG_LEVEL_DEBUG;
            else if (strcasecmp(token, "TRACE") == 0)  level_bit = LOG_LEVEL_TRACE;
            else {
                fprintf(stderr, "[LOGGER] Unknown log level: '%s'\n", token);
                token = strtok(NULL, ",");
                continue;
            }

            // If this is the first +LEVEL and no reset happened, override mask
            if (!disable && !cleared) {
                log_levels_enabled = 0;
                cleared = true;
            }

            // Toggle the flag
            if (disable) log_levels_enabled &= ~level_bit;
            else         log_levels_enabled |=  level_bit;
        }

        token = strtok(NULL, ",");
    }

    free(input);
}
