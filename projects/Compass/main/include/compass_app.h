#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

struct CompassState {
    bool sensorReady = false;
    bool calibrating = false;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float roll = 0.0f;
    float accX = 0.0f, accY = 0.0f, accZ = 0.0f;
    float gyrX = 0.0f, gyrY = 0.0f, gyrZ = 0.0f;
    float magX = 0.0f, magY = 0.0f, magZ = 0.0f;
    float temp = 0.0f;
    std::string statusText;
};

class ICompassView {
public:
    virtual ~ICompassView() = default;
    virtual void update(const CompassState& state) = 0;
    virtual void setActionHandler(std::function<void(const std::string&)> handler) = 0;
};

class CompassApp {
public:
    CompassApp();
    ~CompassApp();

    bool init();
    void deinit();

    void setView(ICompassView* view);

    // Unified action entry point
    void onAction(const std::string& action);

    // Call from main loop
    void poll();

    // Build current state snapshot
    CompassState getState() const;

private:
    void notifyView();
    void sensorThreadFunc();
    bool initSensors();
    void deinitSensors();

    ICompassView* view_ = nullptr;

    mutable std::mutex stateMutex_;
    CompassState state_;

    std::thread sensorThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stateDirty_{false};
    std::atomic<bool> requestCalibrate_{false};

    // BMI270 / BMM150 handles (opaque pointers to keep header clean)
    void* bmiHandle_ = nullptr;
    void* bmmHandle_ = nullptr;
};
