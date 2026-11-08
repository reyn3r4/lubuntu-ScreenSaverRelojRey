#pragma once

#include <QColor>
#include <QImage>
#include <QWidget>

typedef struct _XDisplay Display;
typedef struct _XImage XImage;

class QTimer;

struct ClockOptions
{
    QColor accent = QColor("#dc2626");
    QColor text = QColor("#e2e8f0");
    QColor face = QColor("#280505");
    int faceOpacity = 55;
    bool use24HourTime = true;
    bool showSeconds = true;
};

class ClockWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ClockWidget(ClockOptions options, QImage background = {},
                         WId parentWindow = 0, Display *display = nullptr,
                         int targetDepth = 0);
    ~ClockWidget() override;

    void renderToTarget();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void updateEmbeddedGeometry();

    ClockOptions options_;
    QImage background_;
    WId parentWindow_;
    Display *display_;
    XImage *targetImage_;
    unsigned long targetRedMask_;
    unsigned long targetGreenMask_;
    unsigned long targetBlueMask_;
    int targetDepth_;
    QTimer *timer_;
};
