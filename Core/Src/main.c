#include "main.h"
#include "indicator_task.h"
#include "log_task.h"
#include "drv_fill_light.h"
#include "srv_fill_light.h"
int main(void) {
    board_init();
    uint16_t light_target = drv_fill_light_init();
    srv_fill_light_init(board_millis(), light_target);
    log_task_init();
    indicator_task_init();
    for (;;) {
        srv_fill_light_step(board_millis());
        indicator_task_step();
        log_task_step();
        board_delay_ms(1);
    }
}
