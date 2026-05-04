#pragma once
#include <QObject>
#include <QTimer>
#include <QVideoSink>
#include <atomic>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

// RvcStreamHandler manages the GStreamer RTP→H.264 decode pipeline for the
// Rear View Camera (RVC) and feeds decoded RGBA frames into a QVideoSink that
// is owned by the QML VideoOutput in PipOverlay.
//
// Lifecycle:
//   1. QML PipOverlay calls setVideoSink(videoOutput.videoSink) on startup.
//   2. VehicleBridge::stateChanged triggers start() when DriveMode::REVERSE is
//      entered, and stop() when any other mode is entered.
//   3. The GStreamer pipeline runs entirely on GStreamer-internal threads.
//      Frames are pushed to QVideoSink on the Qt main thread via
//      Qt::QueuedConnection.
//
// RTP stream expected at UDP port 5004 (default), H.264 payload 96.
// To use hardware decode on RPi replace avdec_h264 with v4l2h264dec.
class RvcStreamHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)

public:
    explicit RvcStreamHandler(quint16 rtpPort = 5004, QObject *parent = nullptr);
    ~RvcStreamHandler();

    bool isActive() const { return m_active.load(); }

    // Called from QML: Component.onCompleted: rvcStream.setVideoSink(videoOutput.videoSink)
    // Must be called before start().
    Q_INVOKABLE void setVideoSink(QVideoSink *sink);

public slots:
    void start();
    void stop();

signals:
    void activeChanged(bool active);

private slots:
    // Polls the GStreamer bus once per second for errors / EOS.
    void checkBus();

private:
    // GStreamer appsink callback — fires on GStreamer's streaming thread.
    static GstFlowReturn staticNewSample(GstAppSink *sink, gpointer userData);
    GstFlowReturn handleNewSample(GstAppSink *sink);

    quint16             m_rtpPort;
    QVideoSink         *m_videoSink = nullptr;
    GstElement         *m_pipeline  = nullptr;
    GstElement         *m_appsink   = nullptr;
    QTimer             *m_busTimer  = nullptr;
    std::atomic<bool>   m_active{false};
};
