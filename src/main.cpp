#include "clockwidget.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QSettings>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>
#include <QWindow>

#include <X11/Xlib.h>

#include <cstdlib>
#include <iostream>
#include <optional>

namespace
{
QString configuredWallpaperPath()
{
    const QString configRoot = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation);
    const QStringList candidates{
        QDir(configRoot).filePath(QStringLiteral("pcmanfm-qt/lxqt/settings.conf")),
        QDir(configRoot).filePath(QStringLiteral("pcmanfm-qt/default/settings.conf")),
        QStringLiteral("/etc/xdg/pcmanfm-qt/lxqt/settings.conf"),
        QStringLiteral("/etc/xdg/pcmanfm-qt/default/settings.conf")
    };

    for (const QString &configPath : candidates) {
        if (!QFileInfo(configPath).isFile())
            continue;

        QSettings settings(configPath, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Desktop"));
        QString wallpaper = settings.value(QStringLiteral("Wallpaper")).toString().trimmed();
        settings.endGroup();
        if (wallpaper.isEmpty())
            continue;

        const QUrl wallpaperUrl(wallpaper);
        if (wallpaperUrl.isLocalFile())
            wallpaper = wallpaperUrl.toLocalFile();
        else if (wallpaperUrl.isValid() && !wallpaperUrl.scheme().isEmpty())
            continue;
        else if (QDir::isRelativePath(wallpaper))
            wallpaper = QFileInfo(configPath).dir().absoluteFilePath(wallpaper);

        return QDir::cleanPath(wallpaper);
    }

    return {};
}

QImage loadWallpaper(const QString &path)
{
    if (path.isEmpty())
        return {};

    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull()) {
        std::cerr << "No se pudo cargar el fondo de pantalla \"" << path.toStdString()
                  << "\": " << reader.errorString().toStdString()
                  << ". Se usara un fondo oscuro.\n";
        return {};
    }

    return image;
}

void printUsage(const char *program)
{
    std::cout
        << "Uso: " << program << " [opciones]\n"
        << "  --root, -root             Mostrar en pantalla completa\n"
        << "  --window-id ID            Dibujar dentro de una ventana X11\n"
        << "  --background ARCHIVO      Usar una imagen de fondo especifica\n"
        << "  --no-background           No cargar el fondo configurado en LXQt\n"
        << "  --accent COLOR            Color de manecillas y marcas (#RRGGBB)\n"
        << "  --text COLOR              Color de fecha y hora\n"
        << "  --face COLOR              Color de la esfera\n"
        << "  --face-opacity 0..100     Opacidad de la esfera (predeterminado: 55)\n"
        << "  --12-hour                 Usar formato de 12 horas\n"
        << "  --no-seconds              Ocultar segundos digitales y segundero\n"
        << "  --help                    Mostrar esta ayuda\n";
}

bool readColor(const char *program, const QString &option, const QString &value,
               QColor &destination)
{
    const QColor color(value);
    if (!color.isValid()) {
        std::cerr << program << ": color invalido para " << option.toStdString()
                  << ": " << value.toStdString() << '\n';
        return false;
    }
    destination = color;
    return true;
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Reloj Rey"));

    ClockOptions options;
    std::optional<WId> parentWindow;
    XWindowAttributes parentAttributes{};
    QString wallpaperPath = configuredWallpaperPath();

    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        auto valueFor = [&](const QString &option) -> std::optional<QString> {
            if (i + 1 >= argc) {
                std::cerr << argv[0] << ": falta un valor para "
                          << option.toStdString() << '\n';
                return std::nullopt;
            }
            return QString::fromLocal8Bit(argv[++i]);
        };

        if (argument == QStringLiteral("--help") || argument == QStringLiteral("-help")) {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        }
        if (argument == QStringLiteral("--root") || argument == QStringLiteral("-root")
            || argument == QStringLiteral("--window")) {
            continue;
        }
        if (argument == QStringLiteral("--no-background")) {
            wallpaperPath.clear();
            continue;
        }
        if (argument == QStringLiteral("--background")) {
            const auto value = valueFor(argument);
            if (!value)
                return EXIT_FAILURE;
            wallpaperPath = *value;
            continue;
        }
        if (argument == QStringLiteral("--12-hour")) {
            options.use24HourTime = false;
            continue;
        }
        if (argument == QStringLiteral("--24-hour")) {
            options.use24HourTime = true;
            continue;
        }
        if (argument == QStringLiteral("--no-seconds")) {
            options.showSeconds = false;
            continue;
        }
        if (argument == QStringLiteral("--seconds")) {
            options.showSeconds = true;
            continue;
        }
        if (argument == QStringLiteral("--window-id")
            || argument == QStringLiteral("-window-id")) {
            const auto value = valueFor(argument);
            if (!value)
                return EXIT_FAILURE;
            bool ok = false;
            const qulonglong id = value->toULongLong(&ok, 0);
            if (!ok || id == 0) {
                std::cerr << argv[0] << ": identificador de ventana invalido\n";
                return EXIT_FAILURE;
            }
            parentWindow = static_cast<WId>(id);
            continue;
        }
        if (argument == QStringLiteral("--accent")
            || argument == QStringLiteral("--text")
            || argument == QStringLiteral("--face")) {
            const auto value = valueFor(argument);
            if (!value)
                return EXIT_FAILURE;
            QColor *destination = argument == QStringLiteral("--accent")
                ? &options.accent
                : argument == QStringLiteral("--text") ? &options.text : &options.face;
            if (!readColor(argv[0], argument, *value, *destination))
                return EXIT_FAILURE;
            continue;
        }
        if (argument == QStringLiteral("--face-opacity")) {
            const auto value = valueFor(argument);
            if (!value)
                return EXIT_FAILURE;
            bool ok = false;
            const int opacity = value->toInt(&ok);
            if (!ok || opacity < 0 || opacity > 100) {
                std::cerr << argv[0] << ": --face-opacity debe estar entre 0 y 100\n";
                return EXIT_FAILURE;
            }
            options.faceOpacity = opacity;
            continue;
        }
        if (argument == QStringLiteral("-install")
            || argument == QStringLiteral("-noinstall")
            || argument == QStringLiteral("-verbose")
            || argument == QStringLiteral("-debug")) {
            continue;
        }
        std::cerr << argv[0] << ": opcion desconocida: "
                  << argument.toStdString() << '\n';
        return EXIT_FAILURE;
    }

    if (!parentWindow) {
        const char *targetWindow = std::getenv("XSCREENSAVER_WINDOW");
        if (targetWindow && *targetWindow) {
            bool ok = false;
            const qulonglong id = QString::fromLocal8Bit(targetWindow).toULongLong(&ok, 0);
            if (!ok || id == 0) {
                std::cerr << argv[0]
                          << ": XSCREENSAVER_WINDOW contiene un identificador invalido\n";
                return EXIT_FAILURE;
            }
            parentWindow = static_cast<WId>(id);
        }
    }

    Display *display = nullptr;
    if (parentWindow) {
        display = XOpenDisplay(nullptr);
        if (!display) {
            std::cerr << argv[0] << ": no se pudo abrir la pantalla X11\n";
            return EXIT_FAILURE;
        }

        if (!XGetWindowAttributes(display, static_cast<Window>(*parentWindow),
                                  &parentAttributes)) {
            std::cerr << argv[0] << ": no se pudo consultar la ventana X11\n";
            XCloseDisplay(display);
            return EXIT_FAILURE;
        }
    }

    ClockWidget clock(options, loadWallpaper(wallpaperPath),
                      parentWindow.value_or(0), display,
                      parentWindow ? parentAttributes.depth : 0);
    if (parentWindow) {
        clock.setGeometry(0, 0, parentAttributes.width, parentAttributes.height);
        clock.renderToTarget();
    } else {
        const QScreen *screen = QGuiApplication::primaryScreen();
        if (screen)
            clock.setGeometry(screen->geometry());
        clock.showFullScreen();
    }

    const int result = app.exec();
    if (display)
        XCloseDisplay(display);
    return result;
}
