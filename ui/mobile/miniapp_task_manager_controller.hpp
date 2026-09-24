// Textual compatibility boundary: Qt MOC requires a QObject declaration.
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

/**
 * @brief Presents live Mini App session telemetry and user task-management intents.
 *
 * Runtime code owns browsers and session lifecycle. This controller owns only
 * presentation-safe telemetry snapshots plus optional platform sampling when a
 * public renderer-process identifier is available. It never owns or
 * exposes wallet credentials, WebKit objects, or process handles.
 */
class MiniAppTaskManagerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY tasksChanged FINAL)
    Q_PROPERTY(int taskCount READ taskCount NOTIFY tasksChanged FINAL)
    Q_PROPERTY(int launchingCount READ launchingCount NOTIFY tasksChanged FINAL)
    Q_PROPERTY(double totalCpuPercent READ totalCpuPercent NOTIFY tasksChanged FINAL)
    Q_PROPERTY(qulonglong totalMemoryBytes READ totalMemoryBytes NOTIFY tasksChanged FINAL)
    Q_PROPERTY(qulonglong totalRpcPerSecond READ totalRpcPerSecond NOTIFY tasksChanged FINAL)
    Q_PROPERTY(bool hasProcessTelemetry READ hasProcessTelemetry NOTIFY tasksChanged FINAL)
    Q_PROPERTY(qulonglong revision READ revision NOTIFY tasksChanged FINAL)

public:
    explicit MiniAppTaskManagerController(QObject* parent = nullptr);
    ~MiniAppTaskManagerController() override;

    [[nodiscard]] QVariantList tasks() const;
    [[nodiscard]] int taskCount() const noexcept;
    [[nodiscard]] int launchingCount() const noexcept;
    [[nodiscard]] double totalCpuPercent() const noexcept;
    [[nodiscard]] qulonglong totalMemoryBytes() const noexcept;
    [[nodiscard]] qulonglong totalRpcPerSecond() const noexcept;
    [[nodiscard]] bool hasProcessTelemetry() const noexcept;
    [[nodiscard]] qulonglong revision() const noexcept;

    /** @brief Creates or refreshes a runtime-owned task without fabricating telemetry. */
    void upsertTask(const QString& taskId,
                    const QString& title,
                    const QString& requestedUrl,
                    const QString& url,
                    const QString& iconSource,
                    const QString& state,
                    bool foreground,
                    qlonglong processId);
    /** @brief Changes lifecycle state for an existing task. */
    void setTaskState(const QString& taskId, const QString& state, bool foreground);
    /** @brief Updates the WebContent PID after WebKit creates or replaces its renderer. */
    void setProcessId(const QString& taskId, qlonglong processId);
    /** @brief Records one SDK/RPC operation for the per-second activity meter. */
    void recordRpc(const QString& taskId);
    /** @brief Removes a task after deterministic runtime teardown. */
    void removeTask(const QString& taskId);

    /** @brief Returns true while a matching URL is still resolving/loading. */
    Q_INVOKABLE bool isLaunchingUrl(const QString& url) const;
    /** @brief Returns the current lifecycle state for a matching requested/canonical URL. */
    Q_INVOKABLE QString stateForUrl(const QString& url) const;

    Q_INVOKABLE void focusTask(const QString& taskId);
    Q_INVOKABLE void minimizeTask(const QString& taskId);
    Q_INVOKABLE void terminateTask(const QString& taskId);

signals:
    void tasksChanged();
    void focusRequested(const QString& taskId);
    void minimizeRequested(const QString& taskId);
    void terminateRequested(const QString& taskId);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
