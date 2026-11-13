/* GD32F503 CAN0, ISO CAN FD 1Mbps/1Mbps, no BRS. */
#include "drv_can.h"
#include "board_protocol.h"
#include "log.h"
#include "main.h"
#include "msg_fifo.h"
#include <string.h>
static uint8_t rx_storage[DRV_CAN_RX_QUEUE_BYTES], tx_storage[DRV_CAN_TX_QUEUE_BYTES];
static msg_fifo_t rx_fifo, tx_fifo;
static bool initialized, bus_off, passive;
static volatile uint32_t rx_dropped;
static uint32_t tx_dropped, last_status;
static const uint16_t filters[][2] = {
    { 0x080, 0x7ff }, { 0x100, 0x780 }, { 0x200, 0x7ff }, { 0x210, 0x7ff },
    { 0x300, 0x780 }, { 0x600, 0x780 }, { 0x701, 0x7ff },
    { BOARD_CAN_REQUEST_ID, 0x7ff }, { BOARD_CAN_UPGRADE_HOST_ID, 0x7ff }
};
_Static_assert(sizeof(filters) / sizeof(filters[0]) <= 14U, "CAN0 filter bank capacity exceeded");
_Static_assert((DRV_CAN_RX_QUEUE_DEPTH & (DRV_CAN_RX_QUEUE_DEPTH - 1U)) == 0, "RX depth");
_Static_assert((DRV_CAN_TX_QUEUE_DEPTH & (DRV_CAN_TX_QUEUE_DEPTH - 1U)) == 0, "TX depth");
static bool valid_length(uint8_t n)
{
    return n <= 8 || n == 12 || n == 16 || n == 20 || n == 24 || n == 32 || n == 48 || n == 64;
}

bool drv_can_init(void)
{
    if (initialized)
        return true;
    if (!msg_fifo_init(&rx_fifo, rx_storage, DRV_CAN_RX_QUEUE_DEPTH, sizeof(drv_can_msg_t)) || !msg_fifo_init(&tx_fifo, tx_storage, DRV_CAN_TX_QUEUE_DEPTH, sizeof(drv_can_msg_t)))
        return false;
    rcu_periph_clock_enable(RCU_CAN0);
    gpio_af_set(GPIOA, GPIO_AF_1, GPIO_PIN_11 | GPIO_PIN_12);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11 | GPIO_PIN_12);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_LEVEL3, GPIO_PIN_12);
    can_deinit(CAN0);
    can_parameter_struct cfg;
    can_struct_para_init(CAN_INIT_STRUCT, &cfg);
    cfg.time_triggered = DISABLE;
    cfg.auto_bus_off_recovery = ENABLE;
    cfg.auto_wake_up = DISABLE;
    cfg.auto_retrans = DISABLE;
    cfg.rec_fifo_overwrite = DISABLE;
    cfg.trans_fifo_order = ENABLE;
    cfg.working_mode = CAN_NORMAL_MODE;
    /* APB2=252MHz / 9 / (1+21+6) = 1Mbps, sample point 78.57%. */
    if (rcu_clock_freq_get(CK_APB2) != 252000000U)
        return false;
    cfg.resync_jump_width = CAN_BT_SJW_4TQ;
    cfg.time_segment_1 = CAN_BT_BS1_21TQ;
    cfg.time_segment_2 = CAN_BT_BS2_6TQ;
    cfg.prescaler = 9;
    if (can_init(CAN0, &cfg) != SUCCESS)
        return false;
    can_fdframe_struct fd;
    can_struct_para_init(CAN_FD_FRAME_STRUCT, &fd);
    fd.fd_frame = ENABLE;
    fd.excp_event_detect = ENABLE;
    fd.delay_compensation = DISABLE;
    fd.iso_bosch = CAN_FDMOD_ISO;
    fd.esi_mode = CAN_ESIMOD_HARDWARE;
    fd.data_resync_jump_width = CAN_BT_SJW_4TQ;
    fd.data_time_segment_1 = CAN_BT_BS1_21TQ;
    fd.data_time_segment_2 = CAN_BT_BS2_6TQ;
    fd.data_prescaler = 9;
    if (can_fd_init(CAN0, &fd) != SUCCESS)
        return false;
    can1_filter_start_bank(14);
    for (unsigned i = 0; i < sizeof(filters) / sizeof(filters[0]); ++i) {
        can_filter_parameter_struct f;
        can_struct_para_init(CAN_FILTER_STRUCT, &f);
        f.filter_number = i;
        f.filter_mode = CAN_FILTERMODE_MASK;
        f.filter_bits = CAN_FILTERBITS_32BIT;
        f.filter_enable = ENABLE;
        f.filter_fifo_number = CAN_FIFO0;
        f.filter_list_high = filters[i][0] << 5;
        f.filter_list_low = 0;
        f.filter_mask_high = filters[i][1] << 5;
        f.filter_mask_low = 6; /* Match IDE=0 and RTR=0 (data frames only). */
        can_filter_init(&f);
    }
    initialized = true;
    nvic_irq_enable(CAN0_RX0_IRQn, 1, 0);
    can_interrupt_enable(CAN0, CAN_INTEN_RFNEIE0);
    gpio_bit_reset(GPIOA, GPIO_PIN_10);
    last_status = board_millis();
    return true;
}
bool drv_can_send(const drv_can_msg_t* msg)
{
    if (!initialized || !msg || msg->id > (msg->extended ? 0x1fffffffU : 0x7ffU) || !valid_length(msg->dlc) || (!msg->fd && msg->dlc > 8U))
        return false;
    return msg_fifo_push(&tx_fifo, msg);
}
bool drv_can_send_sync(void)
{
    const drv_can_msg_t msg = { .id = 0x80U, .fd = true, .dlc = 0 };
    return drv_can_send(&msg);
}
bool drv_can_recv(drv_can_msg_t* msg)
{
    return initialized && msg && msg_fifo_pop(&rx_fifo, msg);
}
void CAN0_RX0_IRQHandler(void)
{
    while (can_receive_message_length_get(CAN0, CAN_FIFO0) > 0U) {
        can_receive_message_struct rx;
        can_message_receive(CAN0, CAN_FIFO0, &rx);
        if (rx.rx_ft != CAN_FT_DATA || rx.rx_dlen > 64U)
            continue;
        drv_can_msg_t msg = { 0 };
        msg.extended = rx.rx_ff == CAN_FF_EXTENDED;
        msg.id = msg.extended ? rx.rx_efid : rx.rx_sfid;
        msg.fd = rx.fd_flag != 0;
        msg.dlc = rx.rx_dlen; /* GD library returns byte count, not encoded DLC. */
        memcpy(msg.data, rx.rx_data, msg.dlc);
        if (!msg_fifo_push(&rx_fifo, &msg))
            ++rx_dropped;
    }
    if (can_fifo_overrun_flag_get(CAN0, CAN_FIFO0) == SET) {
        ++rx_dropped;
        can_flag_clear(CAN0, CAN_FLAG_RFO0);
    }
}
void drv_can_poll(void)
{
    if (!initialized)
        return;
    drv_can_msg_t msg;
    while ((CAN_TSTAT(CAN0) & (CAN_TSTAT_TME0 | CAN_TSTAT_TME1 | CAN_TSTAT_TME2)) && msg_fifo_pop(&tx_fifo, &msg)) {
        can_transmit_message_struct tx;
        can_struct_para_init(CAN_TX_MESSAGE_STRUCT, &tx);
        tx.tx_ff = msg.extended ? CAN_FF_EXTENDED : CAN_FF_STANDARD;
        tx.tx_sfid = msg.id;
        tx.tx_efid = msg.id;
        tx.tx_ft = CAN_FT_DATA;
        tx.tx_dlen = msg.dlc;
        tx.fd_flag = msg.fd;
        tx.fd_brs = 0;
        tx.fd_esi = 0;
        memcpy(tx.tx_data, msg.data, msg.dlc);
        if (can_message_transmit(CAN0, &tx) == CAN_NOMAILBOX)
            ++tx_dropped;
    }
    uint32_t now = board_millis();
    if (now - last_status < 100U)
        return;
    last_status = now;
    bool bo = (CAN_ERR(CAN0) & CAN_ERR_BOERR) != 0;
    bool ep = (CAN_ERR(CAN0) & CAN_ERR_PERR) != 0;
    if (bo != bus_off) {
        LOG_W("can", "bus-off=%u (automatic recovery)", (unsigned)bo);
        bus_off = bo;
    }
    if (ep != passive) {
        LOG_W("can", "error-passive=%u", (unsigned)ep);
        passive = ep;
    }
}
uint32_t drv_can_get_rx_dropped(void) { return rx_dropped; }
uint32_t drv_can_get_tx_dropped(void) { return tx_dropped; }
bool drv_can_tx_idle(void)
{
    const uint32_t mailboxes = CAN_TSTAT_TME0 | CAN_TSTAT_TME1 | CAN_TSTAT_TME2;
    return msg_fifo_is_empty(&tx_fifo) && (CAN_TSTAT(CAN0) & mailboxes) == mailboxes;
}
