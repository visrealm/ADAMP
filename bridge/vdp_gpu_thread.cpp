#include "vdp_gpu_thread.h"
#include "vdp_bridge.h"

#include "gpu/gpu.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace {

std::thread             g_thread;
std::mutex              g_mutex;
std::condition_variable g_wake;
bool                    g_running = false;
bool                    g_paced   = true;

/* The emulated clock, in the two units the two waits need. g_tick counts scanlines,
   so an idle thread polls for a trigger once a line. g_credit is instructions: each
   scanline grants a budget, each yield spends one. Banking the credit rather than
   re-arming a flag absorbs host scheduling jitter - descheduled for three lines, the
   GPU comes back owed three lines of work. Capped, or a GPU idle for a second would
   come back owed a second and burst. All of it under g_mutex: the waits need the lock
   anyway, and taking it to signal closes the window where a stop lands between a
   waiter's predicate and its sleep. */
uint64_t g_tick      = 0;
uint64_t g_seen      = 0;
int64_t  g_credit    = 0;
int64_t  g_budget    = 0;
int64_t  g_creditCap = 0;

/* Unspent work the GPU may bank, in scanlines. */
const int64_t kCreditCapLines = 4;

/* Stop-flag check rate when pacing is off: prompt, but invisible in a profile. */
const uint32_t kUnpacedCheckEvery = 4096u;

/* Wait for the beam to move on. False means give up. */
bool waitForTick()
{
    std::unique_lock<std::mutex> lock(g_mutex);
    g_wake.wait(lock, [] { return g_tick != g_seen || !g_running; });
    g_seen = g_tick;
    return g_running;
}

void gpuThreadBody()
{
    while (waitForTick()) {
        /* Returns at once when nothing is pending; a running program blocks in the
           yield below instead, a scanline's work at a time. */
        vdp_bridge_gpu_step_once();
    }
}

/* Joins the thread at teardown, so a joinable std::thread cannot call terminate. */
struct Reaper {
    ~Reaper()
    {
        vdp_gpu_thread_stop();

        /* Safe once joined: clear the run flag so nothing is saved mid-program. */
        vdp_bridge_gpu_halt();
    }
};
Reaper g_reaper;

} // namespace

/* The library's pacing hook, called every budget instructions. Blocking here holds
   the GPU to the emulated clock. Returning false stops the program, which is how a
   stop reaches one that would otherwise never return - so the hook stays registered
   even unpaced. */
extern "C" bool vdp_gpu_thread_yield(void)
{
    std::unique_lock<std::mutex> lock(g_mutex);

    if (!g_paced) {
        return g_running;
    }

    g_credit -= g_budget;
    g_wake.wait(lock, [] { return g_credit > 0 || !g_running; });
    return g_running;
}

void vdp_gpu_thread_start(uint32_t instructionsPerScanline)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_running) {
            return;
        }
        g_running = true;
        g_paced   = (instructionsPerScanline != 0u);
        g_seen    = g_tick;

        g_budget    = (int64_t)instructionsPerScanline;
        g_creditCap = g_budget * kCreditCapLines;
        g_credit    = g_budget; /* the first line, so a trigger starts work at once */
    }

    pico9918_gpu_set_yield(
        instructionsPerScanline ? instructionsPerScanline : kUnpacedCheckEvery,
        vdp_gpu_thread_yield);

    g_thread = std::thread(gpuThreadBody);
}

void vdp_gpu_thread_stop(void)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_running) {
            return;
        }
        g_running = false;
    }

    g_wake.notify_all();

    if (g_thread.joinable()) {
        g_thread.join();
    }

    pico9918_gpu_set_yield(0, nullptr);
}

void vdp_gpu_thread_scanline(void)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_running) {
            return;
        }
        ++g_tick;

        g_credit += g_budget;
        if (g_credit > g_creditCap) {
            g_credit = g_creditCap;
        }
    }

    /* Only ever one waiter - the GPU is one thread, idle on the tick or paused on
       the credit, never both. */
    g_wake.notify_one();
}
