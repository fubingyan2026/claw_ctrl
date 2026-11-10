#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
void _putchar(char ch) { (void)ch; }
int main(void) {
    log_config_t cfg={.name="port",.format_buffer_size=256,.default_level=LOG_LEVEL_DEBUG};
    assert(log_init(&cfg)==LOG_OK);
    assert(log_tx_avail()==4096);
    LOG_I("test","hello %u",42U);
    unsigned char out[512]={0};
    unsigned n=log_tx_get(out,sizeof(out)-1);assert(n>0);assert(strstr((char*)out,"hello 42"));
    assert(!strstr((char*)out,"\033["));
    assert(log_init(&cfg)==LOG_OK);assert(log_tx_avail()==4096);
    assert(log_hold_output(true)==LOG_OK);LOG_I("test","held");assert(log_tx_len()==0);
    assert(log_hold_output(false)==LOG_OK);LOG_I("test","released");assert(log_tx_len()>0);
    unsigned char external[128];cfg.tx_buffer=external;cfg.tx_buffer_size=128;
    assert(log_init(&cfg)==LOG_OK);assert(log_tx_avail()==128);
    cfg.tx_buffer_size=100;assert(log_init(&cfg)==LOG_ERROR_INVALID_PARAM);
    puts("PASS: imported logger config/reinit/buffers");return 0;
}
