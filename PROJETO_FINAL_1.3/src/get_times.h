#include <time.h>
#include <zephyr/sys/timeutil.h>
#include <zephyr/kernel.h>

enum {
    OK_TIME,
    ERR_GET_TIME,
    ERR_INIT_SNT,
};

int initialize_sntp(void);

void ON_OFF_SNTP(bool modo_SNTP);

int get_timedate(char *timedate_buffer, size_t bufsize);

time_t compare_timedate(time_t datetime_to_compare);