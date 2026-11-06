module;

class QString;
class QQuickWindow;
#include <functional>
#include <memory>

export module genycaster.ui.desktop.instance;

export namespace genycaster::ui {
class DesktopSingleInstance final {
public:
    enum class Result { Primary, ActivatedExisting, Error };
    DesktopSingleInstance();
    ~DesktopSingleInstance();
    DesktopSingleInstance(const DesktopSingleInstance&) = delete;
    DesktopSingleInstance& operator=(const DesktopSingleInstance&) = delete;

    [[nodiscard]] Result acquire(const QString& requestedUrl);
    void setWindow(QQuickWindow* window);
    void setOpenUrlHandler(std::function<void(const QString&)> handler);
    [[nodiscard]] QString errorString() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
