#include "main.h"
#include "indicator_task.h"
#include "log_task.h"
#include "drv_fill_light.h"
int main(void) {
    board_init();
    drv_fill_light_init();
    log_task_init();
    indicator_task_init();
    for (;;) {
        indicator_task_step();
        log_task_step();
        board_delay_ms(1);
    }
}
