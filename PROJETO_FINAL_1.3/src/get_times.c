#include <stdio.h>
#include <string.h>
#include <zephyr/logging/log.h>

#include <zephyr/net/socket.h>
#include <zephyr/net/socket_service.h>
#include <zephyr/net/sntp.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>
#include <time.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>

#include "get_times.h"
#include "defines.h"

LOG_MODULE_REGISTER(get_time, LOG_LEVEL_DBG);

struct sntp_time ts;
bool SNTP_ligado = false, SNTP_inicializado = false;

char server_sntp[] = "pool.ntp.org";

void ON_OFF_SNTP(bool modo_SNTP){
    SNTP_ligado = modo_SNTP;
}

int get_timedate(char *timedate_buffer) {
    struct tm timeinfo;
    time_t now;
    struct timespec ts;
    
    clock_gettime(CLOCK_REALTIME, &ts);
    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    strftime(timedate_buffer, 21, "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    char milisec_p[6];
    snprintf(milisec_p, sizeof(milisec_p), ".%03luZ", ts.tv_nsec / 1000000);
    strcat(timedate_buffer, milisec_p);

    if (timeinfo.tm_hour < 12) {
        return -1;
    }

    return timeinfo.tm_sec;
}

time_t compare_timedate(time_t datetime_to_compare) {
    time_t current_time;
    time(&current_time);
    return datetime_to_compare - current_time;
}

int initialize_sntp(void) {
    struct sntp_time ntp;
    int ret = sntp_simple(server_sntp, 3000, &ntp);
    if (ret < 0) {
        LOG_ERR("SNTP failed: %d", ret);
        return ERR_INIT_SNT;
    }

    struct timespec tv = {
        .tv_sec = (time_t)(ntp.seconds - NTP_UNIX_EPOCH_OFFSET),
        .tv_nsec = (long)((((uint64_t)ntp.fraction) * 1000000000ULL) >> 32),
    };
    
    ret = clock_settime(CLOCK_REALTIME, &tv);
    if (ret < 0) {
        LOG_ERR("clock_settime failed: %d", ret);
        return ERR_INIT_SNT;
    }

    SNTP_inicializado = true;
    LOG_INF("SNTP set time ok");
    LOG_INF("NTP seconds: %u", (unsigned)ntp.seconds);
    return OK_TIME;
}

// Thread corrigida (se ainda for necessária)
void get_timedate_thread(void *arg1, void *arg2, void *arg3) {
    char buffer[32];

    while (1) {

        if (SNTP_ligado && SNTP_inicializado) {
            get_timedate(buffer);
            LOG_DBG("Current time: %s", buffer);
            k_sleep(K_SECONDS(1));       
        }

        k_msleep(100);
    }
}

K_THREAD_DEFINE(get_timedate_thread_id, STACKSIZE, get_timedate_thread, NULL, NULL, NULL, PRIORITY, 0, 0);