#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "VehicleServer.grpc.pb.h"
#include "android/hardware/automotive/vehicle/VehiclePropValue.pb.h"

namespace vhal = android::hardware::automotive::vehicle::proto;

// VHAL property IDs (VehicleProperty.aidl)
namespace VehicleProperty {
    constexpr int32_t PERF_VEHICLE_SPEED     = 0x11600207; // float, m/s
    constexpr int32_t ENGINE_RPM             = 0x11600305; // float, rpm
    constexpr int32_t FUEL_LEVEL             = 0x11600307; // float, litres
    constexpr int32_t GEAR_SELECTION         = 0x11400400; // int32, VehicleGear enum
    constexpr int32_t ENGINE_COOLANT_TEMP    = 0x11600301; // float, °C
    constexpr int32_t PARKING_BRAKE_ON       = 0x11200402; // int32, Boolean
    constexpr int32_t VOICE_ASSIST_STATE     = 0x21400002; // int32, VoiceAssistantState enum
}

// Callback invoked on the background poll thread for each property value.
// Implementations must marshal to the Qt main thread via
// QMetaObject::invokeMethod(Qt::QueuedConnection).
using VhalPropertyCallback =
    std::function<void(const vhal::VehiclePropValue &value)>;

// VhalGrpcClient connects to a vhal-core VehicleServer gRPC server and provides:
//  - getProperty()   : one-shot synchronous read
//  - startPolling()  : polls a set of properties on a background thread at a
//                      fixed interval and delivers each value via callback
class VhalGrpcClient
{
public:
    explicit VhalGrpcClient(const std::string &serverAddress = "localhost:50051");
    ~VhalGrpcClient();

    VhalGrpcClient(const VhalGrpcClient &) = delete;
    VhalGrpcClient &operator=(const VhalGrpcClient &) = delete;

    // Blocking read of a single property. Returns false on error.
    bool getProperty(int32_t propId, vhal::VehiclePropValue &outValue);

    // Blocking batch read — fetches all propIds in one RPC call.
    // Returns the values that were successfully returned by the server.
    std::vector<vhal::VehiclePropValue> getProperties(const std::vector<int32_t> &propIds);

    // Start polling the given properties every intervalMs milliseconds.
    // Callback fires on the background thread — caller must marshal to Qt main thread.
    void startPolling(const std::vector<int32_t> &propIds,
                      VhalPropertyCallback callback,
                      int intervalMs = 16);

    // Stop the background poll thread.
    void stopPolling();

    bool isConnected() const { return m_connected.load(); }

private:
    void pollLoop();

    std::shared_ptr<grpc::Channel>              m_channel;
    std::unique_ptr<vhal::VehicleServer::Stub>  m_stub;
    std::atomic<bool>                           m_connected{false};
    std::atomic<bool>                           m_running{false};
    std::thread                                 m_pollThread;
    VhalPropertyCallback                        m_callback;
    std::vector<int32_t>                        m_polledProps;
    int                                         m_intervalMs{16};
};
