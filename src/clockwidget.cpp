#include "clockwidget.h"

#include <QDateTime>
#include <QFont>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLocale>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
unsigned long channelPixel(unsigned char channel, unsigned long mask)
{
    if (!mask)
        return 0;

    unsigned int shift = 0;
    while (((mask >> shift) & 1UL) == 0)
        ++shift;
    const unsigned long channelMaximum = mask >> shift;
    return (((channel * channelMaximum + 127) / 255) << shift) & mask;
}
}

namespace
{
constexpr qreal Pi = 3.14159265358979323846;

QString spanishDate()
{
    QString date = QLocale(QLocale::Spanish).toString(
        QDate::currentDate(), QStringLiteral("dddd, d 'de' MMMM 'de' yyyy"));
    if (!date.isEmpty())
        date[0] = date[0].toUpper();
    return date;
}

QPointF clockPoint(const QPointF &center, qreal radius, qreal degrees)
{
    const qreal angle = degrees * Pi / 180.0;
    return {center.x() + radius * std::cos(angle),
            center.y() + radius * std::sin(angle)};
}
}

ClockWidget::ClockWidget(ClockOptions options, QImage background,
                         WId parentWindow, Display *display, int targetDepth)
    : options_(std::move(options)),
      background_(std::move(background)),
      parentWindow_(parentWindow),
      display_(display),
      targetImage_(nullptr),
      targetRedMask_(0),
      targetGreenMask_(0),
      targetBlueMask_(0),
      targetDepth_(targetDepth),
      timer_(new QTimer(this))
{
    setWindowTitle(QStringLiteral("Reloj Rey"));
    setWindowFlags(Qt::FramelessWindowHint
                   | (parentWindow_ ? Qt::Widget
                                    : Qt::Window | Qt::WindowStaysOnTopHint));
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);

    connect(timer_, &QTimer::timeout, this, [this] {
        if (parentWindow_) {
            updateEmbeddedGeometry();
            renderToTarget();
        } else {
            update();
        }
    });
    timer_->start(1000);
}

ClockWidget::~ClockWidget()
{
    if (targetImage_)
        XDestroyImage(targetImage_);
}

void ClockWidget::renderToTarget()
{
    if (!parentWindow_ || !display_ || width() < 1 || height() < 1)
        return;

    QImage frame(size(), QImage::Format_ARGB32_Premultiplied);
    frame.fill(QColor("#141414"));
    {
        QPainter painter(&frame);
        render(&painter, QPoint(), QRegion(),
               QWidget::DrawWindowBackground | QWidget::DrawChildren);
    }

    XWindowAttributes attributes{};
    if (!XGetWindowAttributes(display_, static_cast<Window>(parentWindow_), &attributes))
        return;

    if (!targetImage_ || targetImage_->width != width()
        || targetImage_->height != height() || targetDepth_ != attributes.depth
        || targetRedMask_ != attributes.visual->red_mask
        || targetGreenMask_ != attributes.visual->green_mask
        || targetBlueMask_ != attributes.visual->blue_mask) {
        if (targetImage_) {
            XDestroyImage(targetImage_);
            targetImage_ = nullptr;
        }
        targetImage_ = XCreateImage(display_, attributes.visual,
                                    static_cast<unsigned int>(attributes.depth),
                                    ZPixmap, 0, nullptr,
                                    static_cast<unsigned int>(width()),
                                    static_cast<unsigned int>(height()), 32, 0);
        if (!targetImage_) {
            std::cerr << "No se pudo crear la imagen X11 para el salvapantallas.\n";
            return;
        }
        targetImage_->data = static_cast<char *>(
            std::calloc(static_cast<size_t>(targetImage_->bytes_per_line),
                        static_cast<size_t>(height())));
        if (!targetImage_->data) {
            XDestroyImage(targetImage_);
            targetImage_ = nullptr;
            std::cerr << "No se pudo reservar memoria para la imagen del reloj.\n";
            return;
        }
        targetRedMask_ = attributes.visual->red_mask;
        targetGreenMask_ = attributes.visual->green_mask;
        targetBlueMask_ = attributes.visual->blue_mask;
        targetDepth_ = attributes.depth;
    }
    if (!targetImage_)
        return;

    for (int y = 0; y < frame.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(frame.constScanLine(y));
        for (int x = 0; x < frame.width(); ++x) {
            const QRgb color = line[x];
            const unsigned long pixel = channelPixel(qRed(color), targetRedMask_)
                | channelPixel(qGreen(color), targetGreenMask_)
                | channelPixel(qBlue(color), targetBlueMask_);
            XPutPixel(targetImage_, x, y, pixel);
        }
    }

    GC gc = XCreateGC(display_, static_cast<Window>(parentWindow_), 0, nullptr);
    XPutImage(display_, static_cast<Drawable>(parentWindow_), gc, targetImage_,
              0, 0, 0, 0, static_cast<unsigned int>(width()),
              static_cast<unsigned int>(height()));
    XFreeGC(display_, gc);
    XFlush(display_);
}

void ClockWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    painter.fillRect(rect(), QColor("#141414"));
    if (!background_.isNull()) {
        const QSizeF targetSize(size());
        const QSizeF imageSize(background_.size());
        const qreal scale = qMax(targetSize.width() / imageSize.width(),
                                 targetSize.height() / imageSize.height());
        const QSizeF scaledSize = imageSize * scale;
        const QRectF targetRect((targetSize.width() - scaledSize.width()) / 2.0,
                                (targetSize.height() - scaledSize.height()) / 2.0,
                                scaledSize.width(), scaledSize.height());
        painter.drawImage(targetRect, background_);
        painter.fillRect(rect(), QColor(0, 0, 0, 90));
    }

    const qreal diameter = qMin(width(), height()) * 0.88;
    if (diameter < 1)
        return;

    const qreal radius = diameter / 2.0;
    const QPointF center(width() / 2.0, height() / 2.0);
    const qreal scale = diameter / 600.0;

    QColor faceColor = options_.face;
    faceColor.setAlpha(qBound(0, options_.faceOpacity, 100) * 255 / 100);

    painter.setPen(QPen(QColor(options_.accent.red(), options_.accent.green(),
                               options_.accent.blue(), 120), 2.0 * scale));
    painter.setBrush(faceColor);
    painter.drawEllipse(center, radius - scale, radius - scale);

    for (int tick = 0; tick < 60; ++tick) {
        const bool hourMark = tick % 5 == 0;
        const qreal outerRadius = radius * 0.91;
        const qreal innerRadius = radius * (hourMark ? 0.82 : 0.87);
        const qreal angle = tick * 6.0 - 90.0;

        QColor tickColor = options_.accent;
        tickColor.setAlpha(hourMark ? 220 : 105);
        painter.setPen(QPen(tickColor, (hourMark ? 2.2 : 1.0) * scale,
                            Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(clockPoint(center, innerRadius, angle),
                         clockPoint(center, outerRadius, angle));
    }

    const QTime now = QTime::currentTime();
    const qreal hourAngle = ((now.hour() % 12) + now.minute() / 60.0) * 30.0 - 90.0;
    const qreal minuteAngle = (now.minute() + now.second() / 60.0) * 6.0 - 90.0;

    painter.setPen(QPen(options_.accent, 7.0 * scale, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(clockPoint(center, -radius * 0.06, hourAngle),
                     clockPoint(center, radius * 0.49, hourAngle));
    painter.setPen(QPen(options_.accent, 4.0 * scale, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(clockPoint(center, -radius * 0.07, minuteAngle),
                     clockPoint(center, radius * 0.69, minuteAngle));

    if (options_.showSeconds) {
        const qreal secondAngle = now.second() * 6.0 - 90.0;
        QColor secondColor = options_.text;
        secondColor.setAlpha(210);
        painter.setPen(QPen(secondColor, 1.5 * scale, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(clockPoint(center, -radius * 0.12, secondAngle),
                         clockPoint(center, radius * 0.78, secondAngle));
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(options_.accent);
    painter.drawEllipse(center, 6.0 * scale, 6.0 * scale);
    painter.setBrush(options_.text);
    painter.drawEllipse(center, 2.5 * scale, 2.5 * scale);

    const qreal dateWidth = radius * 1.30;
    const QString date = spanishDate();
    QFont dateFont = painter.font();
    dateFont.setBold(true);
    dateFont.setPixelSize(qMax(10, qRound(radius * 0.105)));
    const int minDateFontSize = qMax(9, qRound(radius * 0.045));
    while (dateFont.pixelSize() > minDateFontSize
           && QFontMetricsF(dateFont).horizontalAdvance(date) > dateWidth) {
        dateFont.setPixelSize(dateFont.pixelSize() - 1);
    }
    painter.setFont(dateFont);
    const QString fittedDate = QFontMetricsF(dateFont).elidedText(
        date, Qt::ElideRight, qRound(dateWidth));
    painter.setPen(options_.text);
    painter.drawText(QRectF(center.x() - dateWidth / 2.0,
                            center.y() - radius * 0.64,
                            dateWidth, radius * 0.18),
                     Qt::AlignCenter, fittedDate);

    const QString timeFormat = options_.use24HourTime
        ? (options_.showSeconds ? QStringLiteral("HH:mm:ss")
                                : QStringLiteral("HH:mm"))
        : (options_.showSeconds ? QStringLiteral("h:mm:ss AP")
                                : QStringLiteral("h:mm AP"));
    const qreal timeWidth = radius * 1.58;
    QFont digitalFont = painter.font();
    digitalFont.setBold(true);
    digitalFont.setPixelSize(qMax(12, qRound(radius * 0.17)));
    painter.setFont(digitalFont);
    painter.setPen(options_.text);
    painter.drawText(QRectF(center.x() - timeWidth / 2.0,
                            center.y() + radius * 0.27,
                            timeWidth, radius * 0.28),
                     Qt::AlignCenter, now.toString(timeFormat));
}

void ClockWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void ClockWidget::keyPressEvent(QKeyEvent *event)
{
    if (!parentWindow_ && event->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(event);
}

void ClockWidget::updateEmbeddedGeometry()
{
    if (!parentWindow_ || !display_)
        return;

    XWindowAttributes attributes{};
    if (XGetWindowAttributes(display_, static_cast<Window>(parentWindow_), &attributes))
        setGeometry(0, 0, attributes.width, attributes.height);
}
