#include "RvcStreamHandler.h"

#include <QMetaObject>
#include <QSize>
#include <QVideoFrame>
#include <QVideoFrameFormat>
#include <QDebug>


RvcStreamHandler::RvcStreamHandler(quint16 rtpPort, QObject *parent)
    : QObject(parent)
    , m_rtpPort(rtpPort)
    , m_busTimer(new QTimer(this))
{
    connect(m_busTimer, &QTimer::timeout, this, &RvcStreamHandler::checkBus);
}


RvcStreamHandler::~RvcStreamHandler()
{
    stop();
}


void RvcStreamHandler::setVideoSink(QVideoSink *sink)
{
    m_videoSink = sink;
    qDebug() << "[cluster-ui::RvcStreamHandler] Video sink registered:" << sink;
}


void RvcStreamHandler::start()
{
    if (m_active.load()) return;

    if (!m_videoSink) {
        qWarning() << "[cluster-ui::RvcStreamHandler] start() called before setVideoSink() — aborting";
        return;
    }

    // ── Build GStreamer pipeline ──────────────────────────────────────────────
    // This mirrors the gst-launch-1.0 pipeline that was verified on the device,
    // with autovideosink replaced by appsink so frames can be pulled into Qt.
    //
    // To use RPi hardware H.264 decode, replace avdec_h264 with v4l2h264dec.
    //
    // Note: gst_parse_launch receives the string after shell processing, so no
    // extra shell-style quoting is needed around the caps values.
    const QString pipe = QStringLiteral(
        "udpsrc port=%1 "
        "caps=application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96 "
        "! rtph264depay "
        "! video/x-h264,stream-format=byte-stream,alignment=au "
        "! h264parse "
        "! avdec_h264 "
        "! videoconvert "
        "! video/x-raw,format=RGBA "
        "! appsink name=rvc_appsink sync=false max-buffers=2 drop=true"
    ).arg(m_rtpPort);

    GError *parseErr = nullptr;
    m_pipeline = gst_parse_launch(pipe.toUtf8().constData(), &parseErr);
    if (parseErr) {
        qWarning() << "[cluster-ui::RvcStreamHandler] Pipeline parse error:" << parseErr->message;
        g_error_free(parseErr);
        m_pipeline = nullptr;
        return;
    }

    m_appsink = gst_bin_get_by_name(GST_BIN(m_pipeline), "rvc_appsink");
    if (!m_appsink) {
        qWarning() << "[cluster-ui::RvcStreamHandler] appsink element not found in pipeline";
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return;
    }

    // Use GstAppSinkCallbacks (no GLib signal overhead, no emit-signals needed)
    GstAppSinkCallbacks callbacks {};
    callbacks.new_sample = &RvcStreamHandler::staticNewSample;
    gst_app_sink_set_callbacks(GST_APP_SINK(m_appsink), &callbacks, this, nullptr);

    if (gst_element_set_state(m_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        qWarning() << "[cluster-ui::RvcStreamHandler] Failed to set pipeline to PLAYING";
        stop();
        return;
    }

    m_active.store(true);
    m_busTimer->start(1000);
    emit activeChanged(true);
    qDebug() << "[cluster-ui::RvcStreamHandler] RTP stream started on UDP port" << m_rtpPort;
}


void RvcStreamHandler::stop()
{
    m_busTimer->stop();

    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);

        if (m_appsink) {
            gst_object_unref(m_appsink);
            m_appsink = nullptr;
        }
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }

    if (m_active.exchange(false)) {
        emit activeChanged(false);
        qDebug() << "[cluster-ui::RvcStreamHandler] RTP stream stopped";
    }
}


// ── Bus polling — runs on Qt main thread via QTimer ──────────────────────────

void RvcStreamHandler::checkBus()
{
    if (!m_pipeline) return;

    GstBus *bus = gst_element_get_bus(m_pipeline);
    GstMessage *msg;

    while ((msg = gst_bus_pop(bus)) != nullptr) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *gerr = nullptr;
            gchar  *dbg  = nullptr;
            gst_message_parse_error(msg, &gerr, &dbg);
            qWarning() << "[cluster-ui::RvcStreamHandler] GStreamer error:" << gerr->message;
            if (dbg) {
                qDebug() << "[cluster-ui::RvcStreamHandler] debug info:" << dbg;
                g_free(dbg);
            }
            g_error_free(gerr);
            gst_message_unref(msg);
            gst_object_unref(bus);
            stop();
            return;
        }
        case GST_MESSAGE_EOS:
            qDebug() << "[cluster-ui::RvcStreamHandler] End of stream";
            gst_message_unref(msg);
            gst_object_unref(bus);
            stop();
            return;
        default:
            break;
        }
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
}


// ── Frame delivery — called on GStreamer's streaming thread ──────────────────

GstFlowReturn RvcStreamHandler::staticNewSample(GstAppSink *sink, gpointer userData)
{
    return static_cast<RvcStreamHandler *>(userData)->handleNewSample(sink);
}


GstFlowReturn RvcStreamHandler::handleNewSample(GstAppSink *sink)
{
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample)
        return GST_FLOW_ERROR;

    GstCaps *caps = gst_sample_get_caps(sample);
    GstVideoInfo vinfo;
    if (!gst_video_info_from_caps(&vinfo, caps)) {
        gst_sample_unref(sample);
        return GST_FLOW_OK;
    }

    const int width  = GST_VIDEO_INFO_WIDTH(&vinfo);
    const int height = GST_VIDEO_INFO_HEIGHT(&vinfo);

    // Map the frame using GstVideoFrame for correct stride handling
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstVideoFrame gstFrame;
    if (!gst_video_frame_map(&gstFrame, &vinfo, buffer, GST_MAP_READ)) {
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // RGBA is a single-plane format; stride may differ from width*4 due to alignment
    const int     gstStride = GST_VIDEO_FRAME_PLANE_STRIDE(&gstFrame, 0);
    const quint8 *gstData   = static_cast<const quint8 *>(
                                  GST_VIDEO_FRAME_PLANE_DATA(&gstFrame, 0));

    // Deep-copy into a QByteArray before releasing the GStreamer buffer.
    // For PiP resolutions (≤720p) this is fast enough on RPi.
    QByteArray frameBytes(height * gstStride, Qt::Uninitialized);
    memcpy(frameBytes.data(), gstData, static_cast<size_t>(height * gstStride));

    gst_video_frame_unmap(&gstFrame);
    gst_sample_unref(sample);

    // Marshal frame upload to the Qt main thread
    QMetaObject::invokeMethod(this, [this, frameBytes, width, height, gstStride]() {
        if (!m_videoSink) return;

        QVideoFrameFormat fmt(QSize(width, height),
                              QVideoFrameFormat::Format_RGBA8888);
        QVideoFrame frame(fmt);

        if (frame.map(QVideoFrame::WriteOnly)) {
            const int qtStride = frame.bytesPerLine(0);
            if (qtStride == gstStride) {
                memcpy(frame.bits(0), frameBytes.constData(), height * gstStride);
            } else {
                // Stride mismatch: copy row-by-row
                const int rowBytes = qMin(qtStride, gstStride);
                for (int row = 0; row < height; ++row) {
                    memcpy(frame.bits(0) + row * qtStride,
                           frameBytes.constData() + row * gstStride,
                           rowBytes);
                }
            }
            frame.unmap();
        }
        m_videoSink->setVideoFrame(frame);
    }, Qt::QueuedConnection);

    return GST_FLOW_OK;
}
