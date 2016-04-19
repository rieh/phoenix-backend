#include "gameconsoleproxy.h"
#include "gameconsole.h"
#include "logging.h"

#include <type_traits>

#include <QThread>
#include <QCoreApplication>
#include <QQmlApplicationEngine>

using namespace Input;

GameConsoleProxy::GameConsoleProxy( QObject *parent ) : QObject( parent ),
    m_gameConsole( new GameConsole ),
    gameThread( new QThread )
{

    // Set up GameConsole
    m_gameConsole->setObjectName( "GameConsole" );
    m_gameConsole->moveToThread( gameThread );
    connect( gameThread, &QThread::finished,  m_gameConsole, &QObject::deleteLater );
    connect( this, &GameConsoleProxy::shutdown,  m_gameConsole, &GameConsole::shutdown );

    connectGameConsoleProxy();

    gameThread->setObjectName( "Game thread" );
    gameThread->start( QThread::HighestPriority );

    connect( QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [ = ]() {
        qDebug() << "";
        qCInfo( phxControlProxy ) << ">>>>>>>> User requested app to close, shutting down (waiting up to 30 seconds)...";
        qDebug() << "";

        // Shut down gameConsole (calls gameThread->exit() too)
        emit shutdown();

        // Shut down thread, block until it finishes
        gameThread->wait( 30 * 1000 );
        gameThread->deleteLater();

        qDebug() << "";
        qCInfo( phxControlProxy ) << ">>>>>>>> Fully unloaded, quitting!";
        qDebug() << "";
    } );


}


// Safe to call from QML

void GameConsoleProxy::load() {
    QMetaObject::invokeMethod( m_gameConsole, "load" );
}

void GameConsoleProxy::play() {
    QMetaObject::invokeMethod( m_gameConsole, "play" );
}

void GameConsoleProxy::pause() {
    QMetaObject::invokeMethod( m_gameConsole, "pause" );
}

void GameConsoleProxy::stop() {
    QMetaObject::invokeMethod( m_gameConsole, "stop" );
}

void GameConsoleProxy::reset() {
    QMetaObject::invokeMethod( m_gameConsole, "reset" );
}

void GameConsoleProxy::componentComplete() {

    auto srcMap = src().toMap();
    if ( srcMap.contains( QStringLiteral( "--libretro") ) ) {
        const QString core = srcMap[ QStringLiteral( "-c" ) ].toString() ;
        const QString game = srcMap[ QStringLiteral( "-g" ) ].toString();
        if ( !core.isEmpty()
             && !game.isEmpty()
             && QFile::exists( core )
             && QFile::exists( game ) ) {
            qDebug() << "is valid";

            setSrc( QVariantMap{
                        { QStringLiteral( "core" ), core },
                        { QStringLiteral( "game" ), game },
                    } );
            load();
        }

    }

    else {
        if ( !srcMap.isEmpty() ) {
            load();
        }
    }
}

void GameConsoleProxy::classBegin() {
//    emit videoOutputChanged( videoOutput );
//    emit pausableChanged( pausable );
//    emit playbackSpeedChanged( playbackSpeed );
//    emit resettableChanged( resettable );
//    emit rewindableChanged( rewindable );
//    emit sourceChanged( getSource() );
//    emit stateChanged( state );
//    emit volumeChanged( volume );
//    emit vsyncChanged( vsync );
}

QVariant GameConsoleProxy::src() const
{
    return m_src;
}

void GameConsoleProxy::setSrc(QVariant _src) {
    if ( _src != m_src ) {
        m_src = _src;
        emit srcChanged( _src );
    }
}

// Private slots, cannot be called from QML. Use the respective properties instead

void GameConsoleProxy::setVideoOutput( VideoOutput *videoOutput ) {
    emit videoOutputChangedProxy( videoOutput );
}

void GameConsoleProxy::setPlaybackSpeed( qreal playbackSpeed ) {
    emit playbackSpeedChangedProxy( playbackSpeed );
}

void GameConsoleProxy::setVolume( qreal volume ) {
    QMetaObject::invokeMethod( m_gameConsole, "setVolume", Q_ARG( qreal, volume ) );
    //emit volumeChangedProxy( volume );
}

void GameConsoleProxy::setVsync( bool vsync ) {
    QMetaObject::invokeMethod( m_gameConsole, "setVsync", Q_ARG( bool, vsync ) );
    //emit vsyncChangedProxy( vsync );
}

void GameConsoleProxy::setVideoOutputProxy( VideoOutput *videoOutput ) {
    this->videoOutput = videoOutput;
    emit videoOutputChanged( videoOutput );
}

void GameConsoleProxy::setPausableProxy( bool pausable ) {
    this->pausable = pausable;
    emit pausableChanged( pausable );
}

void GameConsoleProxy::setPlaybackSpeedProxy( qreal playbackSpeed ) {
    this->playbackSpeed = playbackSpeed;
    emit playbackSpeedChanged( playbackSpeed );
}

void GameConsoleProxy::setResettableProxy( bool resettable ) {
    this->resettable = resettable;
    emit resettableChanged( resettable );
}

void GameConsoleProxy::setRewindableProxy( bool rewindable ) {
    this->rewindable = rewindable;
    emit rewindableChanged( rewindable );
}

void GameConsoleProxy::setStateProxy( Control::State state ) {
    this->state = ( ControlHelper::State )state;
    emit stateChanged( ( ControlHelper::State )state );
}

void GameConsoleProxy::setVolumeProxy( qreal volume ) {
    this->volume = volume;
    emit volumeChanged( volume );
}

void GameConsoleProxy::setVsyncProxy( bool vsync ) {
    this->vsync = vsync;
    emit vsyncChanged( vsync );
}

void GameConsoleProxy::connectGameConsoleProxy() {

    // Step 2 (our proxy to GameConsole's setter)
    connect( this, &GameConsoleProxy::videoOutputChangedProxy, m_gameConsole, &GameConsole::setVideoOutput );
    connect( this, &GameConsoleProxy::playbackSpeedChangedProxy, m_gameConsole, &GameConsole::setPlaybackSpeed );
    connect( this, &GameConsoleProxy::volumeChangedProxy, m_gameConsole, &GameConsole::setVolume );
    connect( this, &GameConsoleProxy::vsyncChangedProxy, m_gameConsole, &GameConsole::setVsync );

    connect( this, &GameConsoleProxy::srcChanged, m_gameConsole, &GameConsole::setSrc );
    // Step 3 (GameConsole change notifier to our proxy)
    connect( m_gameConsole, &GameConsole::videoOutputChanged, this, &GameConsoleProxy::setVideoOutputProxy );
    connect( m_gameConsole, &GameConsole::pausableChanged, this, &GameConsoleProxy::setPausableProxy );
    connect( m_gameConsole, &GameConsole::playbackSpeedChanged, this, &GameConsoleProxy::setPlaybackSpeedProxy );
    connect( m_gameConsole, &GameConsole::resettableChanged, this, &GameConsoleProxy::setResettableProxy );
    connect( m_gameConsole, &GameConsole::rewindableChanged, this, &GameConsoleProxy::setRewindableProxy );
    //connect( m_gameConsole, &GameConsole::sourceChanged, this, &GameConsoleProxy::setSourceProxy );
    connect( m_gameConsole, &GameConsole::volumeChanged, this, &GameConsoleProxy::setVolumeProxy );
    connect( m_gameConsole, &GameConsole::stateChanged, this, &GameConsoleProxy::setStateProxy );
    connect( m_gameConsole, &GameConsole::vsyncChanged, this, &GameConsoleProxy::setVsyncProxy );

    connect( m_gameConsole, &GameConsole::gamepadAdded, this, [this]( const Gamepad *t_gamepad ) {
        m_gamepadsConnected.append( t_gamepad );
        emit gamepadsConnectedChanged();
    });
    connect( m_gameConsole, &GameConsole::gamepadRemoved, this, [this]( const Gamepad *t_gamepad ) {
        int i = 0;
        for ( const auto *gPad : m_gamepadsConnected ) {
            if ( gPad == t_gamepad ) {
                m_gamepadsConnected.removeAt( i );
                emit gamepadsConnectedChanged();
                break;
            }
            ++i;
        }
    });

}

VideoOutput *GameConsoleProxy::getVideoOutput() const {
    return videoOutput;
}

bool GameConsoleProxy::getPausable() const {
    return pausable;
}

qreal GameConsoleProxy::getPlaybackSpeed() const {
    return playbackSpeed;
}

bool GameConsoleProxy::getResettable() const {
    return resettable;
}

bool GameConsoleProxy::getRewindable() const {
    return rewindable;
}

ControlHelper::State GameConsoleProxy::getState() const {
    return ( ControlHelper::State )state;
}

qreal GameConsoleProxy::getVolume() const {
    return volume;
}

bool GameConsoleProxy::getVsync() const {
    return vsync;
}
