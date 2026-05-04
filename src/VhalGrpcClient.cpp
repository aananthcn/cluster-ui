#include "VhalGrpcClient.h"

#include <grpcpp/grpcpp.h>
#include <QDebug>

#include <chrono>
#include <thread>

// ── Constructor / Destructor ───────────────────────────────────────────────────

VhalGrpcClient::VhalGrpcClient(const std::string &serverAddress)
{
    m_channel = grpc::CreateChannel(serverAddress, grpc::InsecureChannelCredentials());
    m_stub    = vhal::VehicleServer::NewStub(m_channel);

    auto state = m_channel->GetState(/*try_to_connect=*/true);
    m_connected.store(state != GRPC_CHANNEL_SHUTDOWN);

    qDebug() << "[cluster-ui::VhalGrpcClient] connecting to" << QString::fromStdString(serverAddress);
}

VhalGrpcClient::~VhalGrpcClient()
{
    stopPolling();
}

// ── getProperty ───────────────────────────────────────────────────────────────

bool VhalGrpcClient::getProperty(int32_t propId, vhal::VehiclePropValue &outValue)
{
    vhal::VehiclePropValueRequests request;
    auto *req = request.add_requests();
    req->set_request_id(1);
    req->mutable_value()->set_prop(propId);
    req->mutable_value()->set_area_id(0);

    grpc::ClientContext ctx;
    vhal::GetValueResults results;

    auto status = m_stub->GetValues(&ctx, request, &results);
    if (!status.ok()) {
        m_connected.store(false);
        qWarning() << "[cluster-ui::VhalGrpcClient] GetValues failed:"
                   << QString::fromStdString(status.error_message());
        return false;
    }

    m_connected.store(true);
    for (const auto &res : results.results()) {
        if (res.value().prop() == propId) {
            outValue = res.value();
            return true;
        }
    }
    return false;
}

// ── getProperties (batch) ─────────────────────────────────────────────────────

std::vector<vhal::VehiclePropValue>
VhalGrpcClient::getProperties(const std::vector<int32_t> &propIds)
{
    vhal::VehiclePropValueRequests request;
    for (int i = 0; i < static_cast<int>(propIds.size()); ++i) {
        auto *req = request.add_requests();
        req->set_request_id(i);
        req->mutable_value()->set_prop(propIds[i]);
        req->mutable_value()->set_area_id(0);
    }

    grpc::ClientContext ctx;
    vhal::GetValueResults results;

    auto status = m_stub->GetValues(&ctx, request, &results);
    if (!status.ok()) {
        m_connected.store(false);
        qWarning() << "[cluster-ui::VhalGrpcClient] GetValues (batch) failed:"
                   << QString::fromStdString(status.error_message());
        return {};
    }

    m_connected.store(true);
    std::vector<vhal::VehiclePropValue> out;
    out.reserve(results.results_size());
    for (const auto &res : results.results()) {
        if (res.has_value())
            out.push_back(res.value());
    }
    return out;
}

// ── startPolling / stopPolling ────────────────────────────────────────────────

void VhalGrpcClient::startPolling(const std::vector<int32_t> &propIds,
                                   VhalPropertyCallback callback,
                                   int intervalMs)
{
    stopPolling();

    m_polledProps = propIds;
    m_callback    = std::move(callback);
    m_intervalMs  = intervalMs;
    m_running.store(true);
    m_pollThread  = std::thread(&VhalGrpcClient::pollLoop, this);
}

void VhalGrpcClient::stopPolling()
{
    m_running.store(false);
    if (m_pollThread.joinable())
        m_pollThread.join();
}

// ── pollLoop (background thread) ──────────────────────────────────────────────

void VhalGrpcClient::pollLoop()
{
    qDebug() << "[cluster-ui::VhalGrpcClient] poll thread started, interval =" << m_intervalMs << "ms";

    while (m_running.load()) {
        auto wakeAt = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(m_intervalMs);

        // Fetch all properties in one RPC call, then dispatch each via callback.
        auto values = getProperties(m_polledProps);
        if (m_callback) {
            for (const auto &val : values)
                m_callback(val);
        }

        std::this_thread::sleep_until(wakeAt);
    }

    qDebug() << "[cluster-ui::VhalGrpcClient] poll thread exiting";
}
