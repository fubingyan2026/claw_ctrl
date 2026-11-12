#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "srv_board_protocol.h"
#include "srv_juxie.h"
#include "drv_fill_light.h"
#include "srv_fill_light.h"

static uint16_t level;
static unsigned set_calls, attempts;
static bool accept_tx = true;
static drv_can_msg_t last;
uint16_t drv_fill_light_init(void) { level = 0; return 0; }
void drv_fill_light_set(uint16_t value) { level = value; ++set_calls; }
uint16_t drv_fill_light_get(void) { return level; }
bool drv_can_send(const drv_can_msg_t *msg) { last = *msg; ++attempts; return accept_tx; }
static drv_can_msg_t command(unsigned value) {
    drv_can_msg_t msg = {.id=BOARD_CAN_LIGHT_COMMAND_ID,.fd=true,.dlc=8,
        .data={1,1,(uint8_t)value,(uint8_t)(value>>8),0,0,0,0}};
    return msg;
}
static void reset(uint32_t now) {
    srv_fill_light_init(now, drv_fill_light_init()); attempts=set_calls=0; accept_tx=true;
    srv_juxie_init(now); srv_board_protocol_init(now);
}
static void check_status(unsigned expected_level,unsigned state,uint32_t now) {
    assert(last.id==0x221 && last.fd && !last.extended && last.dlc==8);
    assert(last.data[0]==1 && last.data[1]==state);
    assert(last.data[2]==(uint8_t)expected_level && last.data[3]==(uint8_t)(expected_level>>8));
    for(unsigned i=0;i<4;++i) assert(last.data[4+i]==(uint8_t)(now>>(8*i)));
}
static void rejected(drv_can_msg_t *msg) {
    unsigned old_set=set_calls,old_attempts=attempts; uint16_t old_level=level;
    assert(!srv_board_protocol_on_frame(msg,123));
    assert(set_calls==old_set && attempts==old_attempts && level==old_level);
}
static void feedback(unsigned id,unsigned status,unsigned error,uint32_t now) {
    drv_can_msg_t msg={.id=0x300+id,.fd=true,.dlc=12};
    msg.data[6]=error>>8;msg.data[7]=error;msg.data[11]=status;
    srv_juxie_on_frame(&msg,now);
}
int main(void) {
    drv_can_msg_t msg=command(500);
    rejected(&msg); srv_board_protocol_step(1000); assert(attempts==0);
    reset(0); srv_board_protocol_step(499); assert(attempts==0);
    srv_board_protocol_step(500); assert(attempts==1); check_status(0,1,500);
    unsigned values[]={0,1,250,500,999,1000};
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i) {
        msg=command(values[i]); unsigned before=attempts;
        assert(srv_board_protocol_on_frame(&msg,700+i)); assert(attempts==before+1);
        check_status(values[i],1,700+i);
        assert(srv_board_protocol_on_frame(&msg,700+i)); assert(srv_fill_light_get_target()==values[i]);
    }
    msg=command(1001);rejected(&msg);msg=command(65535);rejected(&msg);rejected(NULL);
    msg=command(500);msg.id=0x221;rejected(&msg);
    msg=command(500);msg.extended=true;rejected(&msg);
    msg=command(500);msg.fd=false;rejected(&msg);
    for(unsigned n=0;n<=64;++n) if(n!=8) {msg=command(500);msg.dlc=n;rejected(&msg);}
    msg=command(500);msg.data[0]=2;rejected(&msg);
    msg=command(500);msg.data[1]=2;rejected(&msg);
    for(unsigned i=4;i<8;++i) {msg=command(500);msg.data[i]=1;rejected(&msg);}
    unsigned before=attempts; srv_board_protocol_step(999);assert(attempts==before);
    srv_board_protocol_step(1000);assert(attempts==before+1); // Immediate reports did not shift schedule.

    const unsigned motor_ids[] = {JUXIE_DEV_ID_PRIMARY, JUXIE_DEV_ID_MIRROR};
    for (unsigned i = 0; i < JUXIE_MOTOR_COUNT; ++i) {
        reset(0);
        feedback(motor_ids[i], 0, 0, 490);
        srv_board_protocol_step(500);
        check_status(0, 4, 500); // Either motor alone is a fault, even when disabled.

        feedback(motor_ids[1U - i], 0, 0, 510);
        srv_board_protocol_step(1000);
        check_status(0, 1, 1000); // Both detected and disabled.
    }

    reset(0);msg=command(500);assert(srv_board_protocol_on_frame(&msg,490));
    assert(srv_juxie_should_poll(500)); // Lighting does not suppress motor idle polling.
    feedback(1,0x80,0,510);feedback(2,0x80,0,510);
    srv_board_protocol_step(1000);check_status(500,2,1000);
    feedback(2,0x80,1,1010);msg=command(250);assert(srv_board_protocol_on_frame(&msg,1010));check_status(250,4,1010);
    feedback(2,0,0,1020);assert(srv_board_protocol_on_frame(&msg,1020));check_status(250,1,1020);
    drv_can_msg_t boot={.id=0x701,.fd=true,.dlc=1,.data={1}};
    srv_juxie_on_frame(&boot,1100);srv_board_protocol_step(1500);check_status(250,3,1500);
    srv_board_protocol_step(8000);check_status(250,4,8000); // Heartbeat survives motor/upgrade timeout.

    reset(0);
    feedback(JUXIE_DEV_ID_PRIMARY, JUXIE_STATUS_ENABLED, 0, 0);
    feedback(JUXIE_DEV_ID_MIRROR, JUXIE_STATUS_ENABLED, 0, 1000);
    srv_board_protocol_step(1000);
    check_status(0, 2, 1000);
    msg = command(250);
    assert(srv_board_protocol_on_frame(&msg, 1001));
    check_status(250, 4, 1001);

    reset(0);accept_tx=false;srv_board_protocol_step(500);assert(attempts==1);
    srv_board_protocol_step(501);assert(attempts==1);srv_board_protocol_step(1000);assert(attempts==2);
    msg=command(1000);assert(srv_board_protocol_on_frame(&msg,1100));assert(srv_fill_light_get_target()==1000 && attempts==3);
    accept_tx=true;srv_board_protocol_step(1500);check_status(1000,1,1500);
    reset(UINT32_MAX-250U);srv_board_protocol_step(248);assert(attempts==0);
    srv_board_protocol_step(249);assert(attempts==1);check_status(0,1,249);
    srv_board_protocol_step(5000);assert(attempts==2);srv_board_protocol_step(5000);assert(attempts==2);
    reset(0); srv_fill_light_set_target(1000,0);
    assert(level==0 && srv_fill_light_get_target()==1000);
    srv_fill_light_step(9); assert(level==0);
    srv_fill_light_step(10); assert(level==91);
    unsigned previous=level;
    for(unsigned t=20;t<=300;t+=10) {srv_fill_light_step(t);assert(level>=previous && level<=1000);previous=level;}
    assert(level>=930 && level<=960);
    for(unsigned t=310;t<=1000;t+=10) srv_fill_light_step(t);
    assert(level==1000);
    srv_fill_light_set_target(200,1000);assert(level==1000);
    previous=level;
    for(unsigned t=1010;t<=2000;t+=10) {srv_fill_light_step(t);assert(level<=previous && level>=200);previous=level;}
    assert(level==200);
    srv_fill_light_set_target(0,2000);assert(level==0 && srv_fill_light_get_target()==0);
    srv_fill_light_set_target(1000,2000);srv_fill_light_step(2010);assert(level==91);
    unsigned snapshot=level;srv_fill_light_set_target(500,2010);assert(level==snapshot);
    reset(UINT32_MAX-5U);srv_fill_light_set_target(1000,UINT32_MAX-5U);
    srv_fill_light_step(3);assert(level==0);srv_fill_light_step(4);assert(level==91);
    reset(0);srv_fill_light_set_target(1000,0);srv_fill_light_step(500);assert(level==833);
    drv_fill_light_set(350);
    unsigned before_resume = set_calls;
    srv_fill_light_init(1000, 900);
    assert(level == 350 && srv_fill_light_get_target() == 900 && set_calls == before_resume);
    srv_fill_light_step(1010);
    assert(level == 400);
    puts("PASS: light validation, immediate/periodic status, motor integration, queue-full and time rollover");
    return 0;
}
