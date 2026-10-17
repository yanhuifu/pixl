#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef unsigned int ret_code_t;
typedef struct { uint8_t *data; int type; bool read_only; } ntag_t;
typedef void (*ntag_update_cb_t)(int,void*,ntag_t*);
typedef int ntag_event_type_t;
#define NTAG_EVENT_TYPE_WRITTEN 1
#define NTAG_EVENT_TYPE_READ 2
#define NTAG_215 1
#define NTAG_I2C_PLUS_2K 2
#define NRF_SUCCESS 0
#define NRF_LOG_INFO(...) ((void)0)
#define APP_ERROR_CHECK(x) do { if ((x)!=0) abort(); } while(0)
#define BSP_BOARD_LED_0 0
#define HAL_NFC_EVENT_FIELD_ON 1
#define HAL_NFC_EVENT_FIELD_OFF 2
#define HAL_NFC_EVENT_COMMAND 3
#define HAL_NFC_EVENT_DATA_TRANSMITTED 4
typedef int hal_nfc_event_t;
typedef void (*hal_nfc_callback_t)(void*,hal_nfc_event_t,const uint8_t*,size_t);
static size_t active_size;
static size_t _ntag_data_size(const ntag_t *n) { (void)n; return active_size; }
static unsigned long tx_count, nack_count, ack_count, max_tx;
static ret_code_t hal_nfc_send(const uint8_t *p,size_t n) { assert(p && n>0 && n<=255); if(n>max_tx) max_tx=n; tx_count++; return 0; }
static ret_code_t hal_send_ack_nack(uint8_t a) { if(a==0) nack_count++; else ack_count++; return 0; }
static ret_code_t hal_nfc_passive_ack(void) { return 0; }
static ret_code_t hal_nfc_setup(hal_nfc_callback_t cb,void *p) {(void)cb;(void)p;return 0;}
static ret_code_t hal_nfc_start(void) {return 0;}
static ret_code_t hal_nfc_parameter_set(int id,void *p,size_t n) {(void)id;(void)p;(void)n;return 0;}
static void nrf_pwr_mgmt_feed(void) {}
static void bsp_board_led_on(int x) {(void)x;}
static void bsp_board_led_off(int x) {(void)x;}
static void app_sched_event_put(void *p,size_t n,void (*cb)(void*,uint16_t)) {(void)p;(void)n;(void)cb;}
static void ntag_emu_set_tag(const ntag_t *n);
static void ntag_emu_set_uuid_only(const ntag_t *n);
/* Actual command parser from the user's file, not a rewritten simulation. */
#include "parser_under_test.inc"
static uint32_t rng=0x928d1375u;
static uint32_t next_random(void){ rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng; }
int main(int argc,char **argv){
    unsigned long iterations = argc>1 ? strtoul(argv[1],NULL,10) : 1000000UL;
    uint8_t *tag540 = malloc(540), *tag2048 = malloc(2048); assert(tag540 && tag2048); memset(tag540,0x5a,540); memset(tag2048,0x5a,2048); uint8_t *tag=tag540;
    ntag_emu.ntag.data=tag; ntag_emu.ntag.type=NTAG_215;
    const uint8_t cmds[]={0x30,0xa2,0x60,0x3c,0x1b,0x3a,0xa6,0xc2,0x55,0x43,0x3b,0xa5,0xa7,0xae,0xff};
    for(unsigned long i=0;i<iterations;i++){
        uint8_t input[80]={0};
        size_t len=next_random()%sizeof(input);
        for(size_t j=0;j<len;j++) input[j]=(uint8_t)next_random();
        input[0]=cmds[next_random()%(sizeof(cmds)/sizeof(cmds[0]))];
        active_size=(next_random()%2) ? 540 : 2048;
        tag = active_size==540 ? tag540 : tag2048;
        ntag_emu.ntag.data=tag;
        ntag_emu.ntag.type=active_size==540 ? NTAG_215 : NTAG_I2C_PLUS_2K;
        ntag_emu.ntag.read_only=(next_random()%7==0);
        ntag_emu.sector=(int)(next_random()%5);
        ntag_emu.is_selecting_sector=false;
        nfc_received_process(input,len,tag);
        if(i%100000==0 && i) fprintf(stderr,"%lu cases tested\n",i);
    }
    printf("PASS: %lu parser cases; tx=%lu ack=%lu nack=%lu max_tx=%lu\n",iterations,tx_count,ack_count,nack_count,max_tx);
    free(tag540); free(tag2048);
    return 0;
}
