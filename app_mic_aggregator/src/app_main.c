#include <stdio.h>
#include <xcore/channel.h>
#include <xcore/parallel.h>

DECLARE_JOB(pdm_mic_16, (void));
void pdm_mic_16(void) {
    printf("pdm_mic_16\n");
}

DECLARE_JOB(hub, (void));
void hub(void) {
    printf("hub\n");
}

DECLARE_JOB(tdm16, (void));
void tdm16(void) {
    printf("tdm16\n");
}

DECLARE_JOB(tdm_master_emulator, (void));
void tdm_master_emulator(void) {
    printf("tdm_master_emulator\n");
}




///////// Tile main functions ///////////

void main_tile_0(chanend_t c_cross_tile){
    printf("Hello world tile[0]\n");

    PAR_JOBS(
        PJOB(pdm_mic_16, ())
    );
}

void main_tile_1(chanend_t c_cross_tile){
    printf("Hello world tile[1]\n");

    PAR_JOBS(
        PJOB(hub, ()),
        PJOB(tdm16, ()),
        PJOB(tdm_master_emulator, ())
    );
}