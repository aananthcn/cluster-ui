#pragma once
#include <cstdint>

// the following headerfile comes from VHAL
#include <aidl/android/hardware/automotive/vehicle/VehicleGear.h> 

namespace aidl_vhal = aidl::android::hardware::automotive::vehicle;

enum class DriveMode : uint8_t {
    NORMAL = 0,
    SPORTS = 1,
    REVERSE = 2,
    ECO = 3,
    MAX
};


// Plain struct representing one vehicle state snapshot.
// Values are populated from vhal-core gRPC property updates.
struct VehicleState {
    float     speed_kmh         = 0.0f;   // 0 – 240
    float     rpm               = 0.0f;   // 0 – 8000
    int32_t   gear              = 0;      // raw VehicleGear bitmask (0x0001=N, 0x0002=R, 0x0004=P, 0x0008=D, 0x0010–0x1000=1–9)
    float     fuel_pct          = 0.0f;   // 0 – 100
    float     temp_c            = 0.0f;   // engine coolant °C
    bool      warn_engine       = false;
    bool      warn_battery      = false;
    bool      park_brake        = false;
    DriveMode drive_mode        = DriveMode::NORMAL;  // 0=normal 1=sport 2=eco
    int32_t   voice_assist_state = 0;     // VoiceAssistantState: 0=IDLE 1=LISTENING 2=RECORDING 3=THINKING 4=TALKING
};
