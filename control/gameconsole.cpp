#include "gameconsole.h"
#include "mousemanager.h"
#include "logging.h"

#include <QMetaObject>
#include <QScreen>


GameConsole::GameConsole( Node *parent ) : Node( parent ),
    gameThread( new QThread ),

    // Pipeline nodes owned by this class (game thread)
    audioOutput( new AudioOutput ),
    gamepadManager( new GamepadManager ),
    keyboardManager( new KeyboardManager ),
    libretroLoader( new LibretroLoader ),
    libretroRunner( new LibretroRunner ),
    microTimer( new MicroTimer ),
    mouseManager( new MouseManager ),
    remapper( new Remapper ) {

    // Move all our stuff to the game thread
    audioOutput->moveToThread( gameThread );
    gamepadManager->moveToThread( gameThread );
    keyboardManager->moveToThread( gameThread );
    libretroLoader->moveToThread( gameThread );
    libretroRunner->moveToThread( gameThread );
    libretroVariableForwarder.moveToThread( gameThread );
    microTimer->moveToThread( gameThread );
    mouseManager->moveToThread( gameThread );
    remapper->moveToThread( gameThread );

    gameThread->setObjectName( "Game thread" );
    gameThread->start();

    // Connect global pipeline (at least the parts that can be connected at this point)
    connectNodes( microTimer, gamepadManager );
    connectNodes( gamepadManager, keyboardManager );
    connectNodes( keyboardManager, mouseManager );
    connectNodes( mouseManager, remapper );

    // Handle the wrapper nodes/node frontends/node proxies
    mouseManager->setListener( &keyboardMouseListener );
    keyboardManager->connectKeyboardInput( &keyboardMouseListener );

    connect( this, &GameConsole::remapperModelChanged, this, [ & ] {
        if( remapperModel ) {
            QMetaObject::invokeMethod( remapperModel, "setRemapper", Q_ARG( Remapper *, remapper ) );
            checkIfGlobalPipelineReady();
        }
    } );

    // Connect VariableModel (which lives in QML) to LibretroVariableForwarder as soon as it's set
    connect( this, &GameConsole::variableModelChanged, this, [ & ] {
        if( variableModel ) {
            qCDebug( phxControl ) << "VariableModel" << Q_FUNC_INFO << globalPipelineReady();
            variableModel->setForwarder( &libretroVariableForwarder );
            checkIfGlobalPipelineReady();
        }
    } );

    // Connect GlobalGamepad (which lives in QML) to the global pipeline as soon as it's set
    connect( this, &GameConsole::globalGamepadChanged, this, [ & ]() {
        if( globalGamepad ) {
            qCDebug( phxControl ) << "GlobalGamepad" << Q_FUNC_INFO << globalPipelineReady();
            connectNodes( remapper, globalGamepad );
            checkIfGlobalPipelineReady();
        }
    } );

    // Connect PhoenixWindow (which lives in QML) to the global pipeline as soon as it's set
    connect( this, &GameConsole::phoenixWindowChanged, this, [ & ]() {
        if( phoenixWindow ) {
            qCDebug( phxControl ) << "PhoenixWindow" << Q_FUNC_INFO << globalPipelineReady();
            connectNodes( this, phoenixWindow );
            connectNodes( phoenixWindow, microTimer );
            checkIfGlobalPipelineReady();
        }
    } );

    // Handle app quitting
    connect( QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [ = ]() {
        qDebug() << "";
        qCInfo( phxControl ) << ">>>>>>>> User requested app to close, shutting down (waiting up to 30 seconds)...";
        qDebug() << "";

        // Tell the pipeline to stop if loaded
        if( dynamicPipelineReady() ) {
            quitFlag = true;
            emit commandOut( Command::Stop, QVariant(), QDateTime::currentMSecsSinceEpoch() );
        } else {
            qCInfo( phxControl ) << "No core loaded";
            gameThread->quit();
        }

        // Wait up to 30 seconds to let the pipeline finish its events
        gameThread->wait( 30 * 1000 );
        gameThread->deleteLater();

        // Destroy our global pipeline objects *from the bottom up* (depending on core type)
        if( source[ "type" ] == QStringLiteral( "libretro" ) ||
            pendingPropertyChanges[ "source" ].toMap()[ "type" ] == QStringLiteral( "libretro" ) ) {
            deleteLibretro();
        }

        // Send a second set of delete calls for all other objects (redundant calls will be ignored)
        deleteMembers();

        qDebug() << "";
        qCInfo( phxControl ) << ">>>>>>>> Fully unloaded!";
        qDebug() << "";
    } );
}

// Public slots

void GameConsole::load() {
    if( !globalPipelineReady() ) {
        qCCritical( phxControl ).nospace() << QStringLiteral( "load() called before global pipeline has been set up!" );
    }

    // TODO: Hook QWindow::screenChanged()
    // TODO: Find a better place for this than load()... somewhere where the QScreen is guarantied to be valid
    // FIXME: Don't assume PhoenixWindowNode::phoenixWindow or GameConsole::phoenixWindow set at this point?
    Q_ASSERT( phoenixWindow );
    Q_ASSERT( phoenixWindow->phoenixWindow );
    Q_ASSERT( phoenixWindow->phoenixWindow->screen() );
    emit commandOut( Command::HostFPS, phoenixWindow->phoenixWindow->screen()->refreshRate(),
                     QDateTime::currentMSecsSinceEpoch() );

    if( source[ "type" ] == QStringLiteral( "libretro" ) ||
        pendingPropertyChanges[ "source" ].toMap()[ "type" ] == QStringLiteral( "libretro" ) ) {
        loadLibretro();
        qCDebug( phxControl ) << "Dynamic pipeline ready";
        emit commandOut( Command::DynamicPipelineReady, QVariant(), QDateTime::currentMSecsSinceEpoch() );
        applyPendingPropertyChanges();
        emit commandOut( Command::Load, QVariant(), QDateTime::currentMSecsSinceEpoch() );
    } else if( source[ "type" ].toString().isEmpty() ||
               pendingPropertyChanges[ "source" ].toMap()[ "type" ].toString().isEmpty() ) {
        qCCritical( phxControl ).nospace() << QStringLiteral( "Source was not set!" );
    } else {
        qCCritical( phxControl ).nospace() << QStringLiteral( "Unknown type " )
                                           << source[ "type" ] << QStringLiteral( " passed to load()!" );
    }
}

void GameConsole::play() {
    emit commandOut( Command::Play, QVariant(), QDateTime::currentMSecsSinceEpoch() );
}

void GameConsole::pause() {
    emit commandOut( Command::Pause, QVariant(), QDateTime::currentMSecsSinceEpoch() );
}

void GameConsole::stop() {
    emit commandOut( Command::Stop, QVariant(), QDateTime::currentMSecsSinceEpoch() );
}

void GameConsole::reset() {
    emit commandOut( Command::Reset, QVariant(), QDateTime::currentMSecsSinceEpoch() );
}

void GameConsole::unload() {
    if( !dynamicPipelineReady() ) {
        qCCritical( phxControl ) << ">>>>>>>>" << Q_FUNC_INFO << ": unload() called on an unloaded core!";
        qDebug() << "";
    }

    if( source[ "type" ] == QStringLiteral( "libretro" ) ) {
        unloadLibretro();
    } else if( source[ "type" ].toString().isEmpty() ) {
        qCCritical( phxControl ).nospace() << QStringLiteral( "Source was not set!" );
    } else {
        qCCritical( phxControl ).nospace() << QStringLiteral( "Unknown type " )
                                           << source[ "type" ] << QStringLiteral( " passed to unload()!" );
    }
}

// Private (Startup)

void GameConsole::loadLibretro() {
    // Ensure that the properties were set in QML
    Q_ASSERT_X( controlOutput, "libretro load", "controlOutput was not set!" );
    Q_ASSERT_X( videoOutput, "libretro load", "videoOutput was not set!" );
    Q_ASSERT_X( variableModel, "libretro load", "variableModel was not set!" );

    // Disconnect PhoenixWindow from MicroTimer, insert libretroLoader in between
    disconnectNodes( phoenixWindow, microTimer );
    sessionConnections << connectNodes( phoenixWindow, libretroLoader );
    sessionConnections << connectNodes( libretroLoader, microTimer );

    // Connect LibretroVariableForwarder to the global pipeline
    sessionConnections << connectNodes( remapper, libretroVariableForwarder );
    sessionConnections << connectNodes( libretroVariableForwarder, libretroRunner );

    // Connect LibretroRunner to its children
    sessionConnections << connectNodes( libretroRunner, audioOutput );
    sessionConnections << connectNodes( libretroRunner, videoOutput );
    sessionConnections << connectNodes( libretroRunner, controlOutput );

    // Hook LibretroCore so we know when commands have reached it
    // We can't hook ControlOutput as it lives on the main thread and if it's time to quit the main thread's event loop is dead
    // We care about this happening as LibretroCore needs to save its running game before quitting
    // We also need CoreFPS from LibretroCore so MicroTimer knows how quickly it should emit heartbeats
    sessionConnections << connect( libretroRunner, &Node::commandOut, libretroRunner, [ & ]( Command command, QVariant, qint64 ) {
        switch( command ) {
            case Command::Stop: {
                unloadLibretro();
                break;
            }

            default: {
                break;
            }
        }
    } );
}

bool GameConsole::globalPipelineReady() {
    return ( globalGamepad && phoenixWindow && phoenixWindow->phoenixWindow && phoenixWindow->phoenixWindow->screen() );
}

void GameConsole::checkIfGlobalPipelineReady() {
    if( globalPipelineReady() ) {
        qCDebug( phxControl ) << "Global pipeline ready";
        emit commandOut( Command::GlobalPipelineReady, QVariant(), QDateTime::currentMSecsSinceEpoch() );
    }
}

bool GameConsole::dynamicPipelineReady() {
    return ( globalPipelineReady() && !sessionConnections.empty() );
}

void GameConsole::applyPendingPropertyChanges() {
    if( !pendingPropertyChanges.isEmpty() ) {
        qCDebug( phxControl ) << "Applying pending property changes";
    }

    // Call each setter again if a pending change was set
    if( pendingPropertyChanges.contains( "aspectRatioMode" ) ) {
        setAspectRatioMode( pendingPropertyChanges[ "aspectRatioMode" ].toInt() );
    }

    if( pendingPropertyChanges.contains( "playbackSpeed" ) ) {
        setPlaybackSpeed( pendingPropertyChanges[ "playbackSpeed" ].toReal() );
    }

    if( pendingPropertyChanges.contains( "source" ) ) {
        setSource( pendingPropertyChanges[ "source" ].toMap() );
    }

    if( pendingPropertyChanges.contains( "volume" ) ) {
        setVolume( pendingPropertyChanges[ "volume" ].toReal() );
    }

    if( pendingPropertyChanges.contains( "vsync" ) ) {
        setVsync( pendingPropertyChanges[ "vsync" ].toBool() );
    }
}

// Private (Cleanup)

void GameConsole::unloadLibretro() {
    qCDebug( phxControl ) << Q_FUNC_INFO;

    for( QMetaObject::Connection connection : sessionConnections ) {
        disconnect( connection );
    }

    // Restore connection between PhoenixWindow and MicroTimer
    connectNodes( phoenixWindow, microTimer );

    sessionConnections.clear();

    if( quitFlag ) {
        gameThread->quit();
    }
}

void GameConsole::deleteLibretro() {
    // Delete the dynamic pipeline created by the Libretro core
    // Bottom to top
    audioOutput->deleteLater();
    libretroLoader->deleteLater();
    libretroRunner->deleteLater();
}

void GameConsole::deleteMembers() {
    // Delete everything owned by this class
    // Alphabetical order because it doesn't matter
    audioOutput->deleteLater();
    gamepadManager->deleteLater();
    keyboardManager->deleteLater();
    mouseManager->deleteLater();
    libretroLoader->deleteLater();
    libretroRunner->deleteLater();
    microTimer->deleteLater();
    remapper->deleteLater();
}

int GameConsole::getAspectRatioMode() {
    return aspectRatioMode;
}

void GameConsole::setAspectRatioMode( int aspectRatioMode ) {
    if( !dynamicPipelineReady() ) {
        qCDebug( phxControl ) << Q_FUNC_INFO << ": Dynamic pipeline not yet fully hooked up, caching change for later...";
        pendingPropertyChanges[ "aspectRatioMode" ] = aspectRatioMode;
        return;
    }

    this->aspectRatioMode = aspectRatioMode;
    emit commandOut( Command::SetAspectRatioMode, aspectRatioMode, QDateTime::currentMSecsSinceEpoch() );
    emit aspectRatioModeChanged();
}

// Private (property getters/setters)

qreal GameConsole::getPlaybackSpeed() {
    return playbackSpeed;
}

void GameConsole::setPlaybackSpeed( qreal playbackSpeed ) {
    if( !dynamicPipelineReady() ) {
        qCDebug( phxControl ) << Q_FUNC_INFO << ": Dynamic pipeline not yet fully hooked up, caching change for later...";
        pendingPropertyChanges[ "playbackSpeed" ] = playbackSpeed;
        return;
    }

    this->playbackSpeed = playbackSpeed;
    emit commandOut( Command::SetPlaybackSpeed, playbackSpeed, QDateTime::currentMSecsSinceEpoch() );
    emit playbackSpeedChanged();
}

QVariantMap GameConsole::getSource() {
    return source;
}

void GameConsole::setSource( QVariantMap source ) {
    if( !dynamicPipelineReady() ) {
        qCDebug( phxControl ) << Q_FUNC_INFO << ": Dynamic pipeline not yet fully hooked up, caching change for later...";
        pendingPropertyChanges[ "source" ] = source;
        return;
    }

    this->source = source;
    emit commandOut( Command::SetSource, source, QDateTime::currentMSecsSinceEpoch() );
    emit sourceChanged();
}

qreal GameConsole::getVolume() {
    return volume;
}

void GameConsole::setVolume( qreal volume ) {
    if( !dynamicPipelineReady() ) {
        qCDebug( phxControl ) << Q_FUNC_INFO << ": Dynamic pipeline not yet fully hooked up, caching change for later...";
        pendingPropertyChanges[ "volume" ] = volume;
        return;
    }

    this->volume = volume;
    emit commandOut( Command::SetVolume, volume, QDateTime::currentMSecsSinceEpoch() );
    emit volumeChanged();
}

bool GameConsole::getVsync() {
    return vsync;
}

void GameConsole::setVsync( bool vsync ) {
    if( !dynamicPipelineReady() ) {
        qCDebug( phxControl ) << Q_FUNC_INFO << ": Dynamic pipeline not yet fully hooked up, caching change for later...";
        pendingPropertyChanges[ "vsync" ] = vsync;
        return;
    }

    this->vsync = vsync;
    emit commandOut( Command::SetVsync, vsync, QDateTime::currentMSecsSinceEpoch() );
    emit vsyncChanged();
}
