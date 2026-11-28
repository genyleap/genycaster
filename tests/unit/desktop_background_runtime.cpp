// Actual Qt event-loop test. Linux/X11 with optional simulated tray owner.
// This does not claim Cocoa, Windows Explorer or Android device coverage.
#include "desktop_background.hpp"
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWindow>
#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <cstring>
#ifdef GENYCASTER_TEST_X11
#include <X11/Xlib.h>
#endif

int main(int argc, char** argv)
{
    const bool tray = argc > 1 && std::strcmp(argv[1], "--tray") == 0;
#ifdef GENYCASTER_TEST_X11
    Display* display = nullptr;
    Window owner = 0;
    if (tray) {
        display = XOpenDisplay(nullptr);
        assert(display);
        owner = XCreateSimpleWindow(display, DefaultRootWindow(display),0,0,1,1,0,0,0);
        const auto selection = XInternAtom(display, "_NET_SYSTEM_TRAY_S0", False);
        XSetSelectionOwner(display, selection, owner, CurrentTime);
        XSync(display, False);
        assert(XGetSelectionOwner(display, selection) == owner);
    }
#else
    assert(!tray);
#endif
    int result = 0;
    {
        QApplication app(argc,argv);
        QPixmap pix(24,24); pix.fill(Qt::blue);
        app.setWindowIcon(QIcon(pix));
        QWindow window;
        window.resize(320,240);
        window.show();
        genycaster::ui::DesktopBackgroundController controller(app,window);
        assert(!QApplication::quitOnLastWindowClosed());
        assert(QSystemTrayIcon::isSystemTrayAvailable() == tray);
        int phase = 0;
        int ticks = 0;
        QTimer timer;
        QObject::connect(&timer,&QTimer::timeout,&app,[&]{
            ++ticks;
            if (phase == 0) {
                window.showMaximized();
                phase = 1;
                return;
            }
            if (phase == 1) {
                assert(window.visibility()==QWindow::Maximized);
                assert(!window.close());
                // Close must be deferred (not a native reentrancy).
                assert(window.visibility()==QWindow::Maximized);
                phase = 2;
                return;
            }
            if (phase == 2) {
                assert(window.visibility()==(tray?QWindow::Hidden:QWindow::Minimized));
                controller.showMainWindow();
                assert(window.visibility()==QWindow::Maximized);
                phase = 3;
                return;
            }
            if (phase == 3) {
                assert(!window.close());
                phase = 4;
                return;
            }
            if (phase == 4) {
                assert(window.visibility()==(tray?QWindow::Hidden:QWindow::Minimized));
                controller.showMainWindow();
                phase = 5;
                // Normal application quit, not an unconditional exit().
                QCoreApplication::quit();
            }
        });
        timer.start(80);
        QTimer::singleShot(5000,&app,[&]{std::fprintf(stderr,"quit watchdog\n");app.exit(9);});
        result = app.exec();
        assert(result==0 && phase==5 && ticks>=5);
        std::printf("PASS close/background/restore/maximized/explicit quit; tray=%d\n",tray);
    }
#ifdef GENYCASTER_TEST_X11
    if(display){XDestroyWindow(display,owner);XCloseDisplay(display);}
#endif
    return result;
}
