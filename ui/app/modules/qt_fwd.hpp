// Qt 6.12 is header-based. These globally attached forward declarations are
// the narrow ABI bridge for C++26 named-module interfaces. Implementation
// units include the actual Qt headers in their private global module fragment.
// Do not add implementations, Qt header bodies, or exported definitions here.
class QString;
class QStringView;
template <class T> class QList;
using QStringList = QList<QString>;
class QPoint;
class QPointF;
class QQuickWindow;
