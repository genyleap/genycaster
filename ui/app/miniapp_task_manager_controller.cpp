#include "miniapp_task_manager_controller.hpp"

#include <QDateTime>
#include <QElapsedTimer>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <ranges>
#include <utility>
#include <vector>

#if defined(Q_OS_MACOS)
#include <libproc.h>
#endif

namespace {

constexpr int sampleIntervalMs = 1000;

[[nodiscard]] QString normalizedComparableUrl(const QString& raw)
{
    const QUrl parsed{raw, QUrl::StrictMode};
    if (!parsed.isValid() || parsed.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0) {
        return {};
    }
    QUrl normalized = parsed;
    normalized.setFragment(QString{});
    return normalized.toString(QUrl::FullyEncoded);
}

struct Task final {
    QString taskId{};
    QString title{};
    QString requestedUrl{};
    QString url{};
    QString domain{};
    QString iconSource{};
    QString state{};
    qlonglong processId{};
    qlonglong startedAtMs{};
    double cpuPercent{};
    qulonglong memoryBytes{};
    quint64 rpcCount{};
    quint64 rpcPrevious{};
    quint64 rpcPerSecond{};
    std::uint64_t previousCpuNanoseconds{};
    bool foreground{};
    bool hasCpuSample{};
};

#if defined(Q_OS_MACOS)
[[nodiscard]] bool sampleProcess(const qlonglong pid,
                                 std::uint64_t& cpuNanoseconds,
                                 qulonglong& residentBytes)
{
    if (pid <= 0 || pid > std::numeric_limits<int>::max()) {
        return false;
    }
    proc_taskinfo info{};
    const int bytes = proc_pidinfo(static_cast<int>(pid), PROC_PIDTASKINFO, 0, &info, sizeof(info));
    if (bytes != static_cast<int>(sizeof(info))) {
        return false;
    }
    cpuNanoseconds = static_cast<std::uint64_t>(info.pti_total_user)
        + static_cast<std::uint64_t>(info.pti_total_system);
    residentBytes = static_cast<qulonglong>(info.pti_resident_size);
    return true;
}
#else
[[nodiscard]] bool sampleProcess(const qlonglong,
                                 std::uint64_t&,
                                 qulonglong&)
{
    return false;
}
#endif

[[nodiscard]] QVariantMap presentation(const Task& task)
{
    return {
        {QStringLiteral("taskId"), task.taskId},
        {QStringLiteral("title"), task.title},
        {QStringLiteral("requestedUrl"), task.requestedUrl},
        {QStringLiteral("url"), task.url},
        {QStringLiteral("domain"), task.domain},
        {QStringLiteral("iconSource"), task.iconSource},
        {QStringLiteral("state"), task.state},
        {QStringLiteral("foreground"), task.foreground},
        {QStringLiteral("processId"), task.processId},
        {QStringLiteral("startedAtMs"), task.startedAtMs},
        {QStringLiteral("cpuPercent"), task.cpuPercent},
        {QStringLiteral("memoryBytes"), QVariant::fromValue(task.memoryBytes)},
        {QStringLiteral("rpcPerSecond"), QVariant::fromValue(task.rpcPerSecond)},
        {QStringLiteral("resourceTelemetryAvailable"), task.processId > 0},
    };
}

} // namespace

class MiniAppTaskManagerController::Impl final {
public:
    explicit Impl(MiniAppTaskManagerController& owner)
        : q(owner)
    {
        sampler.setInterval(sampleIntervalMs);
        sampler.setTimerType(Qt::CoarseTimer);
        sampleClock.start();
        QObject::connect(&sampler, &QTimer::timeout, &q, [this] { sample(); });
        sampler.start();
    }

    [[nodiscard]] Task* find(const QString& taskId)
    {
        const auto iterator = std::ranges::find_if(tasks, [&taskId](const Task& task) {
            return task.taskId == taskId;
        });
        return iterator == tasks.end() ? nullptr : &*iterator;
    }

    [[nodiscard]] const Task* find(const QString& taskId) const
    {
        const auto iterator = std::ranges::find_if(tasks, [&taskId](const Task& task) {
            return task.taskId == taskId;
        });
        return iterator == tasks.end() ? nullptr : &*iterator;
    }

    void sample()
    {
        const qint64 elapsedMs = std::max<qint64>(1, sampleClock.restart());
        const double elapsedSeconds = static_cast<double>(elapsedMs) / 1000.0;
        bool changed = false;
        for (Task& task : tasks) {
            const quint64 nextRpcRate = task.rpcCount - task.rpcPrevious;
            task.rpcPrevious = task.rpcCount;
            if (task.rpcPerSecond != nextRpcRate) {
                task.rpcPerSecond = nextRpcRate;
                changed = true;
            }

            std::uint64_t cpuNanoseconds{};
            qulonglong residentBytes{};
            if (!sampleProcess(task.processId, cpuNanoseconds, residentBytes)) {
                if (task.cpuPercent != 0.0 || task.memoryBytes != 0) {
                    task.cpuPercent = 0.0;
                    task.memoryBytes = 0;
                    changed = true;
                }
                task.hasCpuSample = false;
                continue;
            }

            if (task.hasCpuSample && cpuNanoseconds >= task.previousCpuNanoseconds) {
                const auto delta = cpuNanoseconds - task.previousCpuNanoseconds;
                const double nextCpu = std::clamp(
                    static_cast<double>(delta) / 1'000'000'000.0 / elapsedSeconds * 100.0,
                    0.0,
                    999.0);
                if (std::abs(nextCpu - task.cpuPercent) >= 0.05) {
                    task.cpuPercent = nextCpu;
                    changed = true;
                }
            }
            task.previousCpuNanoseconds = cpuNanoseconds;
            task.hasCpuSample = true;
            if (task.memoryBytes != residentBytes) {
                task.memoryBytes = residentBytes;
                changed = true;
            }
        }
        if (changed) {
            ++revision;
            emit q.tasksChanged();
        }
    }

    [[nodiscard]] QVariantList presentationList() const
    {
        std::vector<const Task*> ordered{};
        ordered.reserve(tasks.size());
        for (const Task& task : tasks) {
            ordered.push_back(&task);
        }
        std::ranges::stable_sort(ordered, [](const Task* left, const Task* right) {
            if (left->foreground != right->foreground) {
                return left->foreground;
            }
            if (left->cpuPercent != right->cpuPercent) {
                return left->cpuPercent > right->cpuPercent;
            }
            if (left->rpcPerSecond != right->rpcPerSecond) {
                return left->rpcPerSecond > right->rpcPerSecond;
            }
            return left->startedAtMs > right->startedAtMs;
        });

        QVariantList result{};
        result.reserve(static_cast<qsizetype>(ordered.size()));
        for (const Task* task : ordered) {
            result.push_back(presentation(*task));
        }
        return result;
    }

    MiniAppTaskManagerController& q;
    QTimer sampler{};
    QElapsedTimer sampleClock{};
    std::vector<Task> tasks{};
    qulonglong revision{};
};

MiniAppTaskManagerController::MiniAppTaskManagerController(QObject* parent)
    : QObject(parent), m_impl(std::make_unique<Impl>(*this))
{}

MiniAppTaskManagerController::~MiniAppTaskManagerController() = default;

QVariantList MiniAppTaskManagerController::tasks() const { return m_impl->presentationList(); }
int MiniAppTaskManagerController::taskCount() const noexcept
{
    return static_cast<int>(m_impl->tasks.size());
}

int MiniAppTaskManagerController::launchingCount() const noexcept
{
    return static_cast<int>(std::ranges::count_if(m_impl->tasks, [](const Task& task) {
        return task.state == QStringLiteral("resolving")
            || task.state == QStringLiteral("loading");
    }));
}

double MiniAppTaskManagerController::totalCpuPercent() const noexcept
{
    double total = 0.0;
    for (const Task& task : m_impl->tasks) {
        total += task.cpuPercent;
    }
    return total;
}

qulonglong MiniAppTaskManagerController::totalMemoryBytes() const noexcept
{
    qulonglong total = 0;
    for (const Task& task : m_impl->tasks) {
        total += task.memoryBytes;
    }
    return total;
}

qulonglong MiniAppTaskManagerController::totalRpcPerSecond() const noexcept
{
    qulonglong total = 0;
    for (const Task& task : m_impl->tasks) {
        total += static_cast<qulonglong>(task.rpcPerSecond);
    }
    return total;
}

bool MiniAppTaskManagerController::hasProcessTelemetry() const noexcept
{
    return std::ranges::any_of(m_impl->tasks, [](const Task& task) { return task.processId > 0; });
}

qulonglong MiniAppTaskManagerController::revision() const noexcept
{
    return m_impl->revision;
}

void MiniAppTaskManagerController::upsertTask(const QString& taskId,
                                              const QString& title,
                                              const QString& requestedUrl,
                                              const QString& url,
                                              const QString& iconSource,
                                              const QString& state,
                                              const bool foreground,
                                              const qlonglong processId)
{
    if (taskId.isEmpty()) {
        return;
    }
    Task* task = m_impl->find(taskId);
    if (task == nullptr) {
        Task created{};
        created.taskId = taskId;
        created.startedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_impl->tasks.push_back(std::move(created));
        task = &m_impl->tasks.back();
    }
    task->title = title.left(96);
    task->requestedUrl = normalizedComparableUrl(requestedUrl).left(4096);
    task->url = normalizedComparableUrl(url).left(4096);
    if (task->url.isEmpty()) {
        task->url = task->requestedUrl;
    }
    task->domain = QUrl{task->url}.host().toLower().left(255);
    task->iconSource = iconSource.left(4096);
    task->state = state.left(32);
    task->foreground = foreground;
    if (task->processId != processId) {
        task->processId = processId;
        task->hasCpuSample = false;
        task->previousCpuNanoseconds = 0;
        task->cpuPercent = 0.0;
        task->memoryBytes = 0;
    }
    ++m_impl->revision;
    emit tasksChanged();
}

void MiniAppTaskManagerController::setTaskState(const QString& taskId,
                                                const QString& state,
                                                const bool foreground)
{
    Task* task = m_impl->find(taskId);
    if (task == nullptr) {
        return;
    }
    if (task->state == state && task->foreground == foreground) {
        return;
    }
    task->state = state.left(32);
    task->foreground = foreground;
    ++m_impl->revision;
    emit tasksChanged();
}

void MiniAppTaskManagerController::setProcessId(const QString& taskId, const qlonglong processId)
{
    Task* task = m_impl->find(taskId);
    if (task == nullptr || task->processId == processId) {
        return;
    }
    task->processId = processId;
    task->hasCpuSample = false;
    task->previousCpuNanoseconds = 0;
    task->cpuPercent = 0.0;
    task->memoryBytes = 0;
    ++m_impl->revision;
    emit tasksChanged();
}

void MiniAppTaskManagerController::recordRpc(const QString& taskId)
{
    Task* task = m_impl->find(taskId);
    if (task != nullptr) {
        ++task->rpcCount;
    }
}

void MiniAppTaskManagerController::removeTask(const QString& taskId)
{
    const auto oldSize = m_impl->tasks.size();
    std::erase_if(m_impl->tasks, [&taskId](const Task& task) { return task.taskId == taskId; });
    if (m_impl->tasks.size() != oldSize) {
        ++m_impl->revision;
        emit tasksChanged();
    }
}

bool MiniAppTaskManagerController::isLaunchingUrl(const QString& url) const
{
    const QString key = normalizedComparableUrl(url);
    if (key.isEmpty()) {
        return false;
    }
    return std::ranges::any_of(m_impl->tasks, [&key](const Task& task) {
        const bool matches = task.requestedUrl == key || task.url == key;
        return matches && (task.state == QStringLiteral("resolving")
            || task.state == QStringLiteral("loading"));
    });
}

QString MiniAppTaskManagerController::stateForUrl(const QString& url) const
{
    const QString key = normalizedComparableUrl(url);
    if (key.isEmpty()) {
        return {};
    }
    const auto iterator = std::ranges::find_if(m_impl->tasks, [&key](const Task& task) {
        return task.requestedUrl == key || task.url == key;
    });
    return iterator == m_impl->tasks.end() ? QString{} : iterator->state;
}

void MiniAppTaskManagerController::focusTask(const QString& taskId)
{
    if (m_impl->find(taskId) != nullptr) {
        emit focusRequested(taskId);
    }
}

void MiniAppTaskManagerController::minimizeTask(const QString& taskId)
{
    if (m_impl->find(taskId) != nullptr) {
        emit minimizeRequested(taskId);
    }
}

void MiniAppTaskManagerController::terminateTask(const QString& taskId)
{
    if (m_impl->find(taskId) != nullptr) {
        emit terminateRequested(taskId);
    }
}
