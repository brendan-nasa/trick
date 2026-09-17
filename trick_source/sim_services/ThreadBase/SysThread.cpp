
#include <iostream>
#include <sstream>
#include <stdio.h>
#if __linux__
#include <sys/syscall.h>
#include <sys/types.h>
#include <sched.h>
#endif
#include <signal.h>
#include <algorithm>
#include <time.h>

#include "trick/SysThread.hh"

bool Trick::SysThread::shutdown_finished = false;

// Construct On First Use to avoid the Static Initialization Fiasco
std::mutex& Trick::SysThread::list_mutex() {
    static std::mutex list_mutex;
    return list_mutex;
}

std::condition_variable& Trick::SysThread::list_empty_cv() {
    static std::condition_variable list_empty_cv;
    return list_empty_cv;
}

std::vector<Trick::SysThread *>& Trick::SysThread::all_sys_threads() {
    static std::vector<SysThread *> all_sys_threads;
    return all_sys_threads;
}

Trick::SysThread::SysThread(std::string in_name) : ThreadBase(in_name) {
    _thread_has_paused = true;
    _thread_should_pause = false;
    _thread_has_exited   = false;

    std::lock_guard<std::mutex> lock(list_mutex());
    all_sys_threads().push_back(this);
}


Trick::SysThread::~SysThread() {
    std::lock_guard<std::mutex> lock(list_mutex());
    if (!shutdown_finished) {
        all_sys_threads().erase(std::remove(all_sys_threads().begin(), all_sys_threads().end(), this), all_sys_threads().end());
    }
}

int Trick::SysThread::ensureAllShutdown() {
    std::lock_guard<std::mutex> lock(list_mutex());

    // Cancel all threads
    for (SysThread * thread : all_sys_threads()) {
        thread->cancel_thread();
    }

    // Join all threads
    for (SysThread * thread : all_sys_threads()) {
        thread->join_thread();
    }

    // Success!
    shutdown_finished = true;

    return 0;
}

// To be called from main thread
bool Trick::SysThread::force_thread_to_pause()
{
    std::unique_lock<std::mutex> lock(_restart_pause_mutex);
    // Tell thread to pause, and wait for it to signal that it has.
    //
    // A thread that has already exited can never call test_pause() again, so it can never
    // acknowledge. Waiting only on _thread_has_paused would hang forever against a session
    // that disconnected, hit an exit command, or failed a write after its last test_pause().
    _thread_should_pause = true;
    _thread_has_paused_cv.wait(lock, [this] { return _thread_has_paused || _thread_has_exited; });
    return !_thread_has_exited;
}

// To be called from the sys_thread as it leaves for good
void Trick::SysThread::thread_shutdown() { thread_shutdown(NULL, NULL); }

void Trick::SysThread::thread_shutdown(void (*exit_handler)(void*), void* exit_arg)
{
    // Run teardown first, then publish. Observing _thread_has_exited has to mean this
    // thread is already deregistered and cleaned up.
    //
    // Publishing first left a window where a checkpoint suspension skipped a session as
    // "exited" while that session was still in the variable server's maps, so the resume
    // phase could enumerate it and restart() it with pause state suspension never saved.
    // The pause wait is outside map_mutex, so ordering teardown first does not reintroduce
    // the lock inversion that VariableServer_restart.cpp fixed.
    if (exit_handler != NULL)
    {
        exit_handler(exit_arg);
    }

    {
        std::lock_guard<std::mutex> lock(_restart_pause_mutex);
        _thread_has_exited = true;
    }
    _thread_has_paused_cv.notify_all();

    // Call the two-argument base directly. ThreadBase::thread_shutdown() forwards to the
    // two-argument form through virtual dispatch, which would land back in this override.
    Trick::ThreadBase::thread_shutdown(NULL, NULL);
}

bool Trick::SysThread::thread_has_exited()
{
    std::lock_guard<std::mutex> lock(_restart_pause_mutex);
    return _thread_has_exited;
}

// To be called from main thread
void Trick::SysThread::unpause_thread() {
    {
        std::lock_guard<std::mutex> lock(_restart_pause_mutex);
        // Tell thread to wake up
        _thread_should_pause = false;
    }
    _thread_wakeup_cv.notify_all();
}


// To be called from this thread
void Trick::SysThread::test_pause() {
    std::unique_lock<std::mutex> lock(_restart_pause_mutex) ;
    if (_thread_should_pause) {
        // Tell main thread that we're pausing
        _thread_has_paused = true;
        _thread_has_paused_cv.notify_all();

        // Wait until we're told to wake up
        _thread_wakeup_cv.wait(lock, [this] { return !_thread_should_pause; });
    }

    _thread_has_paused = false;
}