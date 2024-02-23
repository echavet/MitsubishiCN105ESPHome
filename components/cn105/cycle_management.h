#pragma once

struct cycleManagement {

    bool cycleRunning = false;
    unsigned long lastCycleStartMs = 0;
    unsigned long lastCompleteCycleMs = 0;

    void init();
    void cycleStarted();
    void cycleEnded(bool timedOut = false);
    bool hasUpdateIntervalPassed(unsigned int update_interval);
    bool doesCycleTimeOut(unsigned int update_interval);
    bool isCycleRunning();
    void deferCycle();
};