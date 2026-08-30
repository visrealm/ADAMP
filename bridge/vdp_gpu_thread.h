#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The PICO9918's GPU core, on a thread, paced by the emulated beam. On the board the
   GPU has an RP2040 core to itself; stepping it from the emulation thread instead
   deadlocks any game whose engine lives in the GPU (Retroplex): the program spins for
   a value only the Z80 can write, and the Z80 cannot run until it returns. Pacing
   rides on the scanline rather than the wall clock, so the GPU inherits pause,
   single-step and fast-forward. A budget of 0 runs unpaced but still stoppable. */
void vdp_gpu_thread_start(uint32_t instructionsPerScanline);
void vdp_gpu_thread_stop(void);

/* One emulated scanline has passed. Grants the GPU its next budget. */
void vdp_gpu_thread_scanline(void);

#ifdef __cplusplus
}
#endif
