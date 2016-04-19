#include "gamepadmanager.h"
#include "logging.h"

#include <QDateTime>
#include <QQmlEngine>
#include <QEvent>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QWindow>

using namespace Input;

// The Keyboard will be always active in port 0,
// unless changed by the user.

GamepadManager::GamepadManager( QObject *parent )
    : QObject( parent ), Producer(), Controllable(),
      touchCoords(), touchState( false ), touchLatchState( 0 ), touchSet( false ), touchReset( false ),
      m_gamepadList( 16 ),
      m_keyboardStates( 16 )
{
    for ( int i=0; i < 16; ++i ) {
        for ( int j=0; j < 16; j++ ) {
            m_gamepadStates[ i ][ j ] = 0;
        }
        m_keyboardStates[ i ] = 0;
        m_gamepadList[ i ] = nullptr;
    }

    m_keyboardMap = defaultMap();

    connect( this, &GamepadManager::controllerDBFileChanged, &m_SDLEventLoop, &SDLEventLoop::onControllerDBFileChanged );
    connect( &m_SDLEventLoop, &SDLEventLoop::gamepadAdded, this, &GamepadManager::addGamepad );
    connect( &m_SDLEventLoop, &SDLEventLoop::gamepadRemoved, this, &GamepadManager::gamepadRemoved );


}

void GamepadManager::poll( qint64 timeStamp ) {
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    if( currentTime - timeStamp > 64 ) {
        return;
    }

    if( currentState == Control::PLAYING ) {

        QMutexLocker locker( &producerMutex );

        memset(m_gamepadStates, 0, sizeof(m_gamepadStates)); //clear buffer

        // Fetch input states
        m_SDLEventLoop.poll( timeStamp );

        // Copy states from gamepads and the keyboard to a buffer.
        for ( int i=0; i < 16; ++i ) {
            auto gPad = m_gamepadList[ i ];
            if( gPad ) {
                for ( int b=0; b < 16; ++i ) {
                    auto gPadState = gPad->buttonState( static_cast<Gamepad::Button>( b ) );
                    m_gamepadStates[ i ][ b ] = ( 0 == i )
                            ? gPadState | m_keyboardStates[ b ] : gPadState;

                }

            } else {
                if ( 0 == i ) {
                    for ( int b=0; b < 16; ++b ) {
                        m_gamepadStates[ i ][ b ] = m_keyboardStates[ b ];
                        if ( m_gamepadStates[ i ][ b ] == 1 ) {
                        }
                    }
                }
            }
        }


        // Set final touch state once per frame
        // The flipflop is needed as the touched state may change several times *during* a frame
        // This ensures it'll get touch input for one frame if at any point during said frame it gets a touch input
        // Currently configured to keep touched state latched for 2 frames, releasing on 3rd
        // Games I've tried don't like it only going for one then off the next
        updateTouchLatch();

        // Touch input must be done before regular input as that drives frame production
        emit producerData( QStringLiteral( "touchinput" )
                           , &producerMutex, &touchCoords
                           , ( size_t )touchState
                           , currentTime );

        // Cya later buffer!
        emit producerData( QStringLiteral( "input" )
                           , &producerMutex
                           , static_cast<void *>( &m_gamepadStates )
                           , static_cast<size_t>( sizeof( m_gamepadStates ) )
                           , timeStamp );

    }
}

void GamepadManager::setState( Control::State state ) {

    // We only care about the transition to or away from PLAYING
    if( ( this->currentState == Control::PLAYING && state != Control::PLAYING ) ||
        ( this->currentState != Control::PLAYING && state == Control::PLAYING ) ) {
        bool run = ( state == Control::PLAYING );

        //setGamepadControlsFrontend( !run );

        if( run ) {
            qCDebug( phxInput ) << "Reading game input from keyboard";
            installKeyboardFilter();
        }

        else {
            qCDebug( phxInput ) << "No longer reading keyboard input";
            removeKeyboardFilter();
        }
    }

    this->currentState = state;

}

void GamepadManager::updateTouchState( QPointF point, bool pressed ) {
    if( currentState == Control::PLAYING ) {
        touchCoords = point;

        if( pressed ) {
            touchSet = true;
        } else {
            touchReset = true;
        }

    }
}

void GamepadManager::updateTouchLatch() {
    // Set for 2 frames, reset on 3rd
    static int setDuration = 3;
    static int frameCounter = 0;

    // qCDebug( phxInput ) << "frame start";

    // Execute transition function
    if( !touchSet && !touchReset && touchLatchState != 3 ) {
        // Latch (only if not in 3)
        touchLatchState = 0;
    }

    if( !touchSet && !touchReset && touchLatchState == 3 ) {
        // Otherwise, set if setDuration frames have not passed (should only execute once)
        // Reset if they have (should only execute once)
        if( ( frameCounter + 1 ) % setDuration == 0 ) {
            // qCDebug( phxInput ) << "    resetting" << false;
            touchLatchState = 1;
            frameCounter = 0;
        } else {
            // qCDebug( phxInput ) << "    setting" << true;
            frameCounter++;
        }
    }

    if( !touchSet && touchReset ) {
        // Reset
        // qCDebug( phxInput ) << "    normal reset";
        touchLatchState = 1;
    }

    if( touchSet && !touchReset ) {
        // Set
        // qCDebug( phxInput ) << "    normal set";
        touchLatchState = 2;
    }

    if( touchSet && touchReset ) {
        // Set (for 2 frames)
        // This is the first of the 3 frames in the sequence
        // qCDebug( phxInput ) << "    >>>event forced";
        touchLatchState = 3;
        frameCounter = 1;
    }

    // Get FF output
    switch( touchLatchState ) {
        case 0: // Latch
            break;

        case 1: // Reset
            touchState = false;
            break;

        case 2: // Set
            touchState = true;
            break;

        case 3: // Set (for a time)
            touchState = true;
            break;

        default:
            break;
    }

    // qCDebug( phxInput ) << "    frame end" << touchState;

    // Clear FF inputs
    touchSet = false;
    touchReset = false;
}

bool GamepadManager::eventFilter( QObject *object, QEvent *event ) {

    if( event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease ) {
        auto *keyEvent = static_cast<QKeyEvent *>( event );
        bool pressed = ( event->type() == QEvent::KeyPress );
        auto key = static_cast<Qt::Key>( keyEvent->key() );
        if ( m_keyboardMap.contains( key ) ) {
            m_keyboardStates[ static_cast<int>( m_keyboardMap[ key ] ) ] = pressed;
            event->accept();
        }
        return true;
    }
    return QObject::eventFilter( object, event );
}

void GamepadManager::addGamepad(const Gamepad *_gamepad) {
    m_gamepadList.append( _gamepad );
    emit gamepadAdded( _gamepad );
}

void GamepadManager::installKeyboardFilter() {
    Q_ASSERT( QGuiApplication::topLevelWindows().size() > 0 );

    auto *window =  QGuiApplication::topLevelWindows().at( 0 );
    Q_ASSERT( window );

    window->installEventFilter( this );
}

void GamepadManager::removeKeyboardFilter() {
    Q_ASSERT( QGuiApplication::topLevelWindows().size() > 0 );

    auto *window =  QGuiApplication::topLevelWindows().at( 0 );

    Q_ASSERT( window );
    window->removeEventFilter( this );
}

