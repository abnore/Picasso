#include "../../logger.h"
#define LOG_DEFAULT NO_LOG, LOG_COLORS, STDERR_TO_LOG

int main(void)
{
    init_log(LOG_DEFAULT);

    INFO("Hello world!");

    shutdown_log();
    return 0;
}
