#include "vehiclebridge.h"
#include "vehiclestate.h"
#include <QDebug>
#include <QMetaObject>


/// @brief Constructor of the VHAL Bridge
/// @param serverAddress: VHAL server address <localhost>:50051
/// @param parent: the default argument, nullptr 
VehicleBridge::VehicleBridge(const QString &serverAddress, QObject *parent)
    : QObject(parent)
    , m_grpcClient(std::make_unique<VhalGrpcClient>(serverAddress.toStdString()))
{
    const std::vector<int32_t> props = {
        VehicleProperty::PERF_VEHICLE_SPEED,
        VehicleProperty::ENGINE_RPM,
        VehicleProperty::FUEL_LEVEL,
        VehicleProperty::GEAR_SELECTION,
        VehicleProperty::ENGINE_COOLANT_TEMP,
        VehicleProperty::PARKING_BRAKE_ON,
    };

    // Poll all properties on a background thread at ~60 Hz (16 ms).
    // The callback fires on the poll thread; we marshal to the Qt main thread
    // via Qt::QueuedConnection so the event loop is never blocked.
    m_grpcClient->startPolling(props, [this](const vhal::VehiclePropValue &value) {
        float floatVal = value.float_values_size() > 0 ? value.float_values(0) : 0.0f;
        int   intVal   = value.int32_values_size()  > 0 ? value.int32_values(0) : 0;

        QMetaObject::invokeMethod(this, "onPropertyUpdate",
                                  Qt::QueuedConnection,
                                  Q_ARG(int,   static_cast<int>(value.prop())),
                                  Q_ARG(float, floatVal),
                                  Q_ARG(int,   intVal));
    }, /*intervalMs=*/16);

    qDebug() << "[cluster-ui::VehicleBridge] polling vhal-core at" << serverAddress << "@ 60 Hz";
}


VehicleBridge::~VehicleBridge()
{
    m_grpcClient->stopPolling();
}


void VehicleBridge::computeDriveMode(void) {
    if (m_state.gear == static_cast<int8_t>(aidl_vhal::VehicleGear::GEAR_REVERSE)) {
        // Camera Overlay happens in reverse mode
        m_state.drive_mode = DriveMode::REVERSE;
    }
    else if (m_state.gear == static_cast<int8_t>(aidl_vhal::VehicleGear::GEAR_DRIVE)) {
        m_state.drive_mode = DriveMode::SPORTS;
    }
    else {
        m_state.drive_mode = DriveMode::NORMAL;
    }
}



// ── Slot: runs on Qt main thread ──────────────────────────────────────────────
void VehicleBridge::onPropertyUpdate(int propId, float floatVal, int intVal)
{
    switch (propId)
    {
    case VehicleProperty::PERF_VEHICLE_SPEED:
        m_state.speed_kmh = floatVal * 3.6f; // VHAL unit is m/s -> (3600 / 1000) km/h
        break;

    case VehicleProperty::ENGINE_RPM:
        m_state.rpm = floatVal; // 1 count == 1 RPM
        break;

    case VehicleProperty::FUEL_LEVEL: {
        constexpr float FUEL_TANK_CAPACITY_LITERS = 50.0f;
        m_state.fuel_pct = (floatVal / FUEL_TANK_CAPACITY_LITERS) / 10.0f; // 1 count == 1 milliliter
        break;
    } // this brace is needed due to constexpr above.

    case VehicleProperty::GEAR_SELECTION:
        m_state.gear = static_cast<int32_t>(intVal);
        computeDriveMode(); // check the enum class aidl_vhal::VehicleGear
        break;

    case VehicleProperty::ENGINE_COOLANT_TEMP:
        m_state.temp_c = floatVal; // 1 count == 1 deg C
        break;

    case VehicleProperty::PARKING_BRAKE_ON:
        m_state.park_brake = (intVal != 0); // False == dis-engaged
        break;

    default:
        return;
    }

    emit stateChanged();
}
