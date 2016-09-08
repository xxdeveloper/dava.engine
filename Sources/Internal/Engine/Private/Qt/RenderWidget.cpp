#include "Engine/Public/Qt/RenderWidget.h"

#if defined(__DAVAENGINE_COREV2__)

#include "Base/BaseTypes.h"

#if defined(__DAVAENGINE_QT__)

#include "Debug/DVAssert.h"
#include "Logger/Logger.h"

#include <QQuickWindow>
#include <QOpenGLContext>
#include <QQuickItem>

namespace DAVA
{
RenderWidget::RenderWidget(RenderWidget::WindowDelegate* widgetDelegate_, uint32 width, uint32 height)
    : widgetDelegate(widgetDelegate_)
{
    setAcceptDrops(true);
    setMouseTracking(true);

    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    setMinimumSize(QSize(width, height));
    setResizeMode(QQuickWidget::SizeViewToRootObject);

    QQuickWindow* window = quickWindow();
    window->installEventFilter(this);
    window->setClearBeforeRendering(false);
    connect(window, &QQuickWindow::beforeRendering, this, &RenderWidget::OnFrame, Qt::DirectConnection);
    connect(window, &QQuickWindow::activeFocusItemChanged, this, &RenderWidget::OnActiveFocusItemChanged, Qt::DirectConnection);
}

RenderWidget::~RenderWidget()
{
    widgetDelegate->OnDestroyed();
}

void RenderWidget::SetClientDelegate(ClientDelegate* delegate)
{
    DVASSERT(nullptr == clientDelegate);
    clientDelegate = delegate;
    QObject* qobjectDelegate = dynamic_cast<QObject*>(delegate);
    if (qobjectDelegate != nullptr)
    {
        QObject::connect(qobjectDelegate, &QObject::destroyed, this, &RenderWidget::OnClientDelegateDestroyed);
    }
}

void RenderWidget::OnFrame()
{
    QVariant nativeHandle = quickWindow()->openglContext()->nativeHandle();
    if (!nativeHandle.isValid())
    {
        DAVA::Logger::Error("GL context is not valid!");
        throw std::runtime_error("GL context is not valid!");
    }

    if (initialized == false)
    {
        widgetDelegate->OnCreated();
        initialized = true;
    }

    widgetDelegate->OnFrame();
    quickWindow()->resetOpenGLState();
}

void RenderWidget::OnActiveFocusItemChanged()
{
    QQuickItem* item = quickWindow()->activeFocusItem();
    if (item != nullptr)
    {
        item->installEventFilter(this);
    }
}

void RenderWidget::resizeEvent(QResizeEvent* e)
{
    QQuickWidget::resizeEvent(e);
    float32 dpi = devicePixelRatioF();
    QSize size = e->size();
    widgetDelegate->OnResized(size.width(), size.height(), dpi);
    emit Resized(size.width(), size.height());
}

void RenderWidget::showEvent(QShowEvent* e)
{
    QQuickWidget::showEvent(e);
    widgetDelegate->OnVisibilityChanged(true);
}

void RenderWidget::hideEvent(QHideEvent* e)
{
    widgetDelegate->OnVisibilityChanged(false);
    QQuickWidget::hideEvent(e);
}

void RenderWidget::timerEvent(QTimerEvent* e)
{
    if (!quickWindow()->isVisible())
    {
        e->ignore();
        return;
    }

    QQuickWidget::timerEvent(e);
}

void RenderWidget::dragEnterEvent(QDragEnterEvent* e)
{
    if (clientDelegate != nullptr)
    {
        clientDelegate->OnDragEntered(e);
    }
}

void RenderWidget::dragMoveEvent(QDragMoveEvent* e)
{
    if (clientDelegate != nullptr)
    {
        clientDelegate->OnDragMoved(e);
    }
}

void RenderWidget::dragLeaveEvent(QDragLeaveEvent* e)
{
    if (clientDelegate != nullptr)
    {
        clientDelegate->OnDragLeaved(e);
    }
}

void RenderWidget::dropEvent(QDropEvent* e)
{
    if (clientDelegate != nullptr)
    {
        clientDelegate->OnDrop(e);
    }
}

void RenderWidget::mousePressEvent(QMouseEvent* e)
{
    QQuickWidget::mousePressEvent(e);
    widgetDelegate->OnMousePressed(e);

    if (clientDelegate != nullptr)
    {
        clientDelegate->OnMousePressed(e);
    }
}

void RenderWidget::mouseReleaseEvent(QMouseEvent* e)
{
    QQuickWidget::mouseReleaseEvent(e);
    widgetDelegate->OnMouseReleased(e);

    if (clientDelegate != nullptr)
    {
        clientDelegate->OnMouseReleased(e);
    }
}

void RenderWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    QQuickWidget::mouseDoubleClickEvent(e);
    widgetDelegate->OnMouseDBClick(e);

    if (clientDelegate != nullptr)
    {
        clientDelegate->OnMouseDBClick(e);
    }
}

void RenderWidget::mouseMoveEvent(QMouseEvent* e)
{
    QQuickWidget::mouseMoveEvent(e);
    widgetDelegate->OnMouseMove(e);

    if (clientDelegate != nullptr)
    {
        clientDelegate->OnMouseMove(e);
    }
}

void RenderWidget::wheelEvent(QWheelEvent* e)
{
    QQuickWidget::wheelEvent(e);
    widgetDelegate->OnWheel(e);

    if (clientDelegate != nullptr)
    {
        clientDelegate->OnWheel(e);
    }
}

void RenderWidget::keyPressEvent(QKeyEvent* e)
{
    QQuickWidget::keyPressEvent(e);
    widgetDelegate->OnKeyPressed(e);

    if (clientDelegate != nullptr)
    {
        clientDelegate->OnKeyPressed(e);
    }
}

void RenderWidget::keyReleaseEvent(QKeyEvent* e)
{
    QQuickWidget::keyReleaseEvent(e);
    widgetDelegate->OnKeyReleased(e);
    if (clientDelegate != nullptr)
    {
        clientDelegate->OnKeyReleased(e);
    }
}

bool RenderWidget::event(QEvent* e)
{
    if (e->type() == QEvent::NativeGesture && clientDelegate != nullptr)
    {
        QNativeGestureEvent* gestureEvent = static_cast<QNativeGestureEvent*>(e);
        clientDelegate->OnNativeGuesture(gestureEvent);
    }
    return QQuickWidget::event(e);
}

bool RenderWidget::eventFilter(QObject* object, QEvent* e)
{
    QEvent::Type t = e->type();
    if ((t == QEvent::KeyPress || t == QEvent::KeyRelease) && keyEventRecursiveGuard == false)
    {
        QQuickItem* focusObject = quickWindow()->activeFocusItem();
        if (object == quickWindow() || object == focusObject)
        {
            keyEventRecursiveGuard = true;
            SCOPE_EXIT
            {
                keyEventRecursiveGuard = false;
            };
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
            if (t == QEvent::KeyPress)
            {
                keyPressEvent(keyEvent);
            }
            else
            {
                keyReleaseEvent(keyEvent);
            }
            return true;
        }
    }

    return false;
}

void RenderWidget::OnClientDelegateDestroyed()
{
    clientDelegate = nullptr;
}

} // namespace DAVA

#endif // __DAVAENGINE_QT__
#endif // __DAVAENGINE_COREV2__