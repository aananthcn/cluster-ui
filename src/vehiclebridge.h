#pragma once
#include <QObject>
#include <QString>
#include <memory>
#include "vehiclestate.h"
#include "VhalGrpcClient.h"

// VehicleBridge exposes live vehicle state to QML as Q_PROPERTYs.
// It owns a VhalGrpcClient that polls vhal-core over gRPC on a background
// thread at ~60 Hz. Updates are marshalled to the Qt main thread via
// Qt::QueuedConnection before emitting stateChanged().
class VehicleBridge : public QObject
{
    Q_OBJECT

    Q_PROPERTY(float   speed      READ speed      NOTIFY stateChanged)
    Q_PROPERTY(float   rpm        READ rpm        NOTIFY stateChanged)
    Q_PROPERTY(int     gear       READ gear       NOTIFY stateChanged)
    Q_PROPERTY(float   fuel       READ fuel       NOTIFY stateChanged)
    Q_PROPERTY(float   temp       READ temp       NOTIFY stateChanged)
    Q_PROPERTY(bool    warnEngine READ warnEngine NOTIFY stateChanged)
    Q_PROPERTY(bool    warnBatt   READ warnBatt   NOTIFY stateChanged)
    Q_PROPERTY(bool    warnBrake  READ warnBrake  NOTIFY stateChanged)
    Q_PROPERTY(int     driveMode       READ driveMode       NOTIFY stateChanged)
    Q_PROPERTY(int     voiceAssistState READ voiceAssistState NOTIFY stateChanged)

public:
    explicit VehicleBridge(const QString &serverAddress = QStringLiteral("localhost:50051"),
                           QObject *parent = nullptr);
    ~VehicleBridge();

    float speed()      const { return m_state.speed_kmh; }
    float rpm()        const { return m_state.rpm; }
    int   gear()       const { return m_state.gear; }
    float fuel()       const { return m_state.fuel_pct; }
    float temp()       const { return m_state.temp_c; }
    bool  warnEngine() const { return m_state.warn_engine; }
    bool  warnBatt()   const { return m_state.warn_battery; }
    bool  warnBrake()  const { return m_state.park_brake; }
    int   driveMode()        const { return static_cast<int>(m_state.drive_mode); }
    int   voiceAssistState() const { return m_state.voice_assist_state; }

signals:
    void stateChanged();

private slots:
    // Marshalled from the poll thread to the Qt main thread.
    void onPropertyUpdate(int propId, float floatVal, int intVal);

private:
    // methods
    void computeDriveMode();

    // members
    VehicleState                    m_state;
    std::unique_ptr<VhalGrpcClient> m_grpcClient;
};
