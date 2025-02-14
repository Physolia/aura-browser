/*
    SPDX-FileCopyrightText: 2022 Aditya Mehra <aix.m@outlook.com>
    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QGuiApplication>
#include <QQmlApplicationEngine>
//#include <qtwebengineglobal.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWebEngine/QQuickWebEngineProfile>
#else
#include <QtWebEngineQuick/QQuickWebEngineProfile>
#endif
#include <QtWebEngineCore/qwebengineurlrequestinterceptor.h>
#include "plugins/virtualMouse.h"
#include "plugins/virtualKeypress.h"
#include "plugins/globalSettings.h"
#include "plugins/audiorecorder.h"
#include <QQmlContext>
#include <QCommandLineParser>
#include <QCommandLineOption>

#include <KLocalizedContext>
#include <KLocalizedString>


// Add Adblock Implementation
#include <QThread>
#include <QFile>
#include <QDebug>
#include "third-party/ad-block/ad_block_client.h"

class WebIntercept : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT
public:
    WebIntercept(QObject *parent = nullptr) : QWebEngineUrlRequestInterceptor(parent)
    {
        QThread *thread = QThread::create([this]{
            QFile file(":/third-party/easylist.txt");
            QString easyListTxt;

            if(!file.exists()) {
                qDebug() << "No easylist.txt file found.";
            } else {
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)){
                    easyListTxt = file.readAll();
                }
                file.close();
                client.parse(easyListTxt.toStdString().c_str());
            }
        });
        thread->start();
    }

    void interceptRequest(QWebEngineUrlRequestInfo &info) override
    {
        if (client.matches(info.requestUrl().toString().toStdString().c_str(),
                           FONoFilterOption, info.requestUrl().host().toStdString().c_str())) {
            qDebug() << "Blocked: " << info.requestUrl();
            info.block(true);
        }
    }

private:
    AdBlockClient client;
};

static QObject *globalSettingsSingletonProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return new GlobalSettings;
}

static QObject *audioRecorderSingletonProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return new AudioRecorder;
}

int main(int argc, char *argv[])
{
    QStringList arguments;
    for (int a = 0; a < argc; ++a) {
        arguments << QString::fromLocal8Bit(argv[a]);
    }

    QCommandLineParser parser;
    auto urlOption = QCommandLineOption(QStringLiteral("url"), QStringLiteral("Single url to load in sandbox"), QStringLiteral("url"));
    auto sandboxOption = QCommandLineOption(QStringLiteral("sandbox"), QStringLiteral("Sandbox Mode"));
    auto helpOption = QCommandLineOption(QStringLiteral("help"), QStringLiteral("Show this help message"));
    parser.addOptions({urlOption, sandboxOption, helpOption});
    parser.process(arguments);

    qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE", QByteArray("0"));
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    QCoreApplication::setOrganizationName("AuraBrowser");
    QCoreApplication::setApplicationName("AuraBrowser");
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QtWebEngine::initialize();
#else
    QtWebEngineQuick::initialize();
#endif
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/qml/images/logo-small.png"));
    KLocalizedString::setApplicationDomain("aura-browser");

    if (parser.isSet(helpOption)) {
        parser.showHelp();
        return 0;
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

    // Adblock Implementation
    WebIntercept interceptor;
    QQuickWebEngineProfile adblockProfile;
    adblockProfile.setUrlRequestInterceptor(&interceptor);
    adblockProfile.setHttpUserAgent("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/79.0.3945.117 Safari/537.36");
    adblockProfile.setStorageName("Profile");
    adblockProfile.setOffTheRecord(false);
    engine.rootContext()->setContextProperty("adblockProfile", &adblockProfile);

    FakeCursor fakeCursor;
    engine.rootContext()->setContextProperty("Cursor", &fakeCursor);
    QQmlContext* ctx = engine.rootContext();
    VirtualKeyPress virtualKeyPress;
    ctx->setContextProperty("keyEmitter", &virtualKeyPress);
    auto offlineStoragePath = QUrl::fromLocalFile(engine.offlineStoragePath());
    engine.rootContext()->setContextProperty("offlineStoragePath", offlineStoragePath);
    qmlRegisterSingletonType<GlobalSettings>("Aura", 1, 0, "GlobalSettings", globalSettingsSingletonProvider);
    qmlRegisterSingletonType<AudioRecorder>("Aura", 1, 0, "AudioRecorder", audioRecorderSingletonProvider);
    qmlRegisterSingletonType(QUrl(QStringLiteral("qrc:/qml/NavigationSoundEffects.qml")), "Aura", 1, 0, "NavigationSoundEffects");

    QString sandboxURL = parser.value(urlOption);
    bool sandboxMode = parser.isSet(sandboxOption);
    
    if (arguments.count() > 1) {
        QUrl url = QUrl::fromUserInput(arguments.at(1));
        if (url.isValid()) {
            sandboxURL = url.toString();
            sandboxMode = true;
        }
    }

    engine.rootContext()->setContextProperty(QStringLiteral("sandboxURL"), sandboxURL);
    engine.rootContext()->setContextProperty(QStringLiteral("sandboxMode"), sandboxMode);

    // Define const QUrl url here, if the user is in sandbox mode, we want to load the sandbox qml file, if not, we want to load the main qml file
    QUrl url;
    if (sandboxMode) {
        url = QUrl(QStringLiteral("qrc:/qml/mainSandbox.qml"));
    } else {
        url = QUrl(QStringLiteral("qrc:/qml/main.qml"));
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}

#include "main.moc"
