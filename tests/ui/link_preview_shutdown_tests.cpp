#include "link_preview_resolver.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

#include <cstdlib>
#include <iostream>

int main(int argc, char** argv)
{
    QCoreApplication application{argc, argv};

    QElapsedTimer timer{};
    timer.start();
    {
        LinkPreviewResolver resolver{true};
        resolver.requestPreview(QStringLiteral("https://example.com/"));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
    }

    const qint64 elapsedMs = timer.elapsed();
    if (elapsedMs > 3'500) {
        std::cerr << "Link preview shutdown exceeded bounded teardown budget: "
                  << elapsedMs << " ms\n";
        return EXIT_FAILURE;
    }

    std::cout << "Link preview shutdown completed in " << elapsedMs << " ms\n";
    return EXIT_SUCCESS;
}
