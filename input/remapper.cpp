#include "remapper.h"
#include "remappermodel.h"

#include <QKeySequence>
#include <QVector2D>
#include <QtMath>

Remapper::Remapper( Node *parent ) : Node( parent ) {
    keyboardGamepad.instanceID = -1;
}

// Public slots

void Remapper::commandIn( Node::Command command, QVariant data, qint64 timeStamp ) {
    emit commandOut( command, data, timeStamp );

    switch( command ) {
        case Command::Stop:
        case Command::Load:
        case Command::Pause:
        case Command::Unload:
        case Command::Reset: {
            playing = false;
            break;
        }

        case Command::Play: {
            playing = true;
            break;
        }

        case Command::HandleGlobalPipelineReady: {
            emit controllerAdded( "", "Keyboard" );

            // TODO: Read keyboard setting from disk too
            dpadToAnalogKeyboard = true;

            // Give the model friendly names for all the keys set
            for( int physicalKey : keyboardKeyToSDLButton.keys() ) {
                int virtualButton = keyboardKeyToSDLButton[ physicalKey ];

                // This prints garbage as-is, so change it to something that doesn't
                // Confirmed on Windows but looks okay in OS X?
                // https://bugreports.qt.io/browse/QTBUG-40030
                if( physicalKey == Qt::Key_Shift ) {
                    physicalKey = Qt::ShiftModifier;
                }

                if( physicalKey == Qt::Key_Control ) {
                    physicalKey = Qt::ControlModifier;
                }

                if( physicalKey == Qt::Key_Meta ) {
                    physicalKey = Qt::MetaModifier;
                }

                if( physicalKey == Qt::Key_Alt ) {
                    physicalKey = Qt::AltModifier;
                }

                if( physicalKey == Qt::Key_AltGr ) {
                    physicalKey = Qt::AltModifier;
                }

                QString keyString = QKeySequence( physicalKey ).toString( QKeySequence::NativeText );

                // Strip out trailing + in the corrected keys
                if( keyString.length() > 1 ) {
                    keyString = keyString.section( '+', 0, 0 );
                }

                emit setMapping( "",  keyString, buttonToString( virtualButton ) );
            }

            break;
        }

        case Command::Heartbeat: {
            // Send out per-GUID OR'd states to RemapperModel then clear stored pressed states
            for( QString GUID : pressed.keys() ) {
                emit buttonUpdate( GUID, pressed[ GUID ] );
                pressed[ GUID ] = false;
            }

            // Do the same for the keyboard
            emit buttonUpdate( "", keyboardKeyPressed );
            keyboardKeyPressed = false;

            // If in remap mode, make sure the GUID in question still exists, exit remap mode if not
            if( remapMode && !GUIDCount.contains( remapModeGUID ) ) {
                qCWarning( phxInput ) << "No controllers with GUID" << remapModeGUID << "remaining, exiting remap mode!";
                remapMode = false;
                emit remappingEnded();
            }

            break;
        }

        case Command::ControllerAdded: {
            GamepadState gamepad = data.value<GamepadState>();
            QString GUID( QByteArray( reinterpret_cast<const char *>( gamepad.GUID.data ), 16 ).toHex() );

            // Add to map if it hasn't been encountered yet
            // Otherwise, just increment the count
            if( !GUIDCount.contains( GUID ) ) {
                GUIDCount[ GUID ] = 1;
                emit controllerAdded( GUID, gamepad.friendlyName );
            } else {
                GUIDCount[ GUID ]++;
            }

            // TODO: Read value from disk
            analogToDpad[ GUID ] = false;
            dpadToAnalog[ GUID ] = false;

            // TODO: Read mappings from disk
            // For now, just init the remap with default mappings
            for( int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ ) {
                gamepadSDLButtonToSDLButton[ GUID ][ i ] = i;
                emit setMapping( GUID, buttonToString( i ), buttonToString( i ) );
            }

            break;
        }

        case Command::ControllerRemoved: {
            GamepadState gamepad = data.value<GamepadState>();
            QString GUID( QByteArray( reinterpret_cast<const char *>( gamepad.GUID.data ), 16 ).toHex() );
            GUIDCount[ GUID ]--;

            // Remove from map if no longer there
            if( GUIDCount[ GUID ] == 0 ) {
                GUIDCount.remove( GUID );
                emit controllerRemoved( GUID );
            }

            break;
        }

        default: {
            break;
        }
    }
}

void Remapper::dataIn( Node::DataType type, QMutex *mutex, void *data, size_t bytes, qint64 timeStamp ) {
    switch( type ) {
        case DataType::Input: {
            // Copy incoming data to our own buffer
            GamepadState gamepad;
            {
                mutex->lock();
                gamepad = *reinterpret_cast< GamepadState * >( data );
                mutex->unlock();
            }

            QString GUID( QByteArray( reinterpret_cast<const char *>( gamepad.GUID.data ), 16 ).toHex() );

            // Apply axis to d-pad, if enabled
            // This will always be enabled if we're not currently playing so GlobalGamepad can use the analog stick
            if( analogToDpad[ GUID ] || !playing ) {
                // TODO: Support other axes?
                int xAxis = SDL_CONTROLLER_AXIS_LEFTX;
                int yAxis = SDL_CONTROLLER_AXIS_LEFTY;

                // TODO: Let user configure these

                qreal threshold = 16384.0;

                // Size in degrees of the arc covering and centered around each cardinal direction
                // If <90, there will be gaps in the diagonals
                // If >180, this code will always produce diagonal inputs
                qreal rangeDegrees = 180.0 - 45.0;

                // Get axis coords in cartesian coords
                // Bottom right is positive -> top right is positive
                float xCoord = gamepad.axis[ xAxis ];
                float yCoord = -gamepad.axis[ yAxis ];

                // Get radius from center
                QVector2D position( xCoord, yCoord );
                qreal radius = position.length();

                // Get angle in degrees
                qreal angle = qRadiansToDegrees( qAtan2( yCoord, xCoord ) );

                if( angle < 0.0 ) {
                    angle += 360.0;
                }

                if( radius > threshold ) {
                    qreal halfRange = rangeDegrees / 2.0;

                    if( angle > 90.0 - halfRange && angle < 90.0 + halfRange ) {
                        gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_UP ] = true;
                    }

                    if( angle > 270.0 - halfRange && angle < 270.0 + halfRange ) {
                        gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_DOWN ] = true;
                    }

                    if( angle > 180.0 - halfRange && angle < 180.0 + halfRange ) {
                        gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_LEFT ] = true;
                    }

                    if( angle > 360.0 - halfRange || angle < 0.0 + halfRange ) {
                        gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_RIGHT ] = true;
                    }
                }
            }

            // Apply d-pad to axis, if enabled
            if( dpadToAnalog[ GUID ] ) {
                gamepad = mapDpadToAnalog( gamepad );
            }

            // OR all button states together by GUID
            for( int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ ) {
                pressed[ GUID ] |= gamepad.button[ i ];
            }

            // If we are in remap mode, check for a button press from the stored GUID, and remap the stored button to that button
            // Don't go past this block if in remap mode
            {
                if( remapMode && GUID == remapModeGUID ) {
                    // Find a button press, the first one we encounter will be the new remapping
                    for( int physicalButton = 0; physicalButton < SDL_CONTROLLER_BUTTON_MAX; physicalButton++ ) {
                        if( gamepad.button[ physicalButton ] == SDL_PRESSED ) {
                            int virtualButton = remapModeButton;
                            // TODO: Store this mapping to disk
                            qCDebug( phxInput ) << "Button" << buttonToString( physicalButton )
                                                << "from GUID" << GUID << "now activates" << buttonToString( virtualButton );

                            // Store the new remapping internally
                            gamepadSDLButtonToSDLButton[ GUID ][ physicalButton ] = virtualButton;

                            // Tell the model we're done
                            remapMode = false;
                            ignoreMode = true;
                            ignoreModeGUID = GUID;
                            ignoreModeButton = physicalButton;
                            ignoreModeInstanceID = gamepad.instanceID;
                            emit setMapping( GUID, buttonToString( physicalButton ), buttonToString( virtualButton ) );
                            emit remappingEnded();
                            break;
                        }
                    }

                    // Do not go any further in this data handler
                    return;
                } else if( remapMode ) {
                    // Do not go any further in this data handler
                    return;
                }
            }

            // If ignoreButton is set, the user hasn't let go of the button they were remapping to
            // Do not let the button go through until they let go
            if( ignoreMode && ignoreModeGUID == GUID && ignoreModeInstanceID == gamepad.instanceID ) {
                if( gamepad.button[ ignoreModeButton ] == SDL_PRESSED ) {
                    gamepad.button[ ignoreModeButton ] = SDL_RELEASED;
                } else {
                    ignoreMode = false;
                }
            }

            // Remap button states according to stored data
            {
                GamepadState virtualGamepad = gamepad;

                // Clear remappedGamepad's states
                for( int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ ) {
                    virtualGamepad.button[ i ] = SDL_RELEASED;
                }

                QMap<int, int> physicalToVirtual = gamepadSDLButtonToSDLButton[ GUID ];

                for( int physicalButton = 0; physicalButton < SDL_CONTROLLER_BUTTON_MAX; physicalButton++ ) {
                    // Read virtual (remapped) button id from stored remap data
                    int virtualButton = physicalToVirtual[ physicalButton ];

                    // Write the physical button's state to its corresponding virtual button (set by user)
                    virtualGamepad.button[ virtualButton ] |= gamepad.button[ physicalButton ];
                }

                gamepad = virtualGamepad;
            }

            // Send updated data out
            {
                // Copy current gamepad into buffer
                this->mutex.lock();
                gamepadBuffer[ gamepadBufferIndex ] = gamepad;
                this->mutex.unlock();

                // Send buffer on its way
                emit dataOut( DataType::Input, &( this->mutex ),
                              reinterpret_cast< void * >( &gamepadBuffer[ gamepadBufferIndex ] ), 0,
                              QDateTime::currentMSecsSinceEpoch() );

                // Increment the index
                gamepadBufferIndex = ( gamepadBufferIndex + 1 ) % 100;
            }
            break;
        }

        case DataType::KeyboardInput: {
            // Unpack keyboard states and write to gamepad according to remap data
            {
                mutex->lock();
                KeyboardState keyboard = *reinterpret_cast<KeyboardState *>( data );

                for( int i = keyboard.head; i < keyboard.tail; i = ( i + 1 ) % 128 ) {
                    int key = keyboard.key[ i ];
                    bool pressed = keyboard.pressed[ i ];

                    if( keyboardKeyToSDLButton.contains( key ) ) {
                        keyboardGamepad.button[ keyboardKeyToSDLButton[ key ] ] = pressed ? SDL_PRESSED : SDL_RELEASED;
                    }
                }

                mutex->unlock();
            }

            // OR all key states together and store that value
            for( int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++ ) {
                keyboardKeyPressed |= keyboardGamepad.button[ i ];
            }

            // Apply d-pad to axis, if enabled
            if( dpadToAnalogKeyboard ) {
                keyboardGamepad = mapDpadToAnalog( keyboardGamepad, true );
            }

            // Send gamepad on its way
            {
                // Copy current gamepad into buffer
                this->mutex.lock();
                gamepadBuffer[ gamepadBufferIndex ] = keyboardGamepad;
                this->mutex.unlock();

                // Send buffer on its way
                emit dataOut( DataType::Input, &( this->mutex ),
                              reinterpret_cast< void * >( &gamepadBuffer[ gamepadBufferIndex ] ), 0,
                              QDateTime::currentMSecsSinceEpoch() );

                // Increment the index
                gamepadBufferIndex = ( gamepadBufferIndex + 1 ) % 100;
            }
            break;
        }

        default: {
            emit dataOut( type, mutex, data, bytes, timeStamp );
            break;
        }
    }
}

void Remapper::beginRemapping( QString GUID, QString button ) {
    remapMode = true;
    remapModeGUID = GUID;
    remapModeButton = stringToButton( button );
}

// Private

GamepadState Remapper::mapDpadToAnalog( GamepadState gamepad, bool clear ) {
    // TODO: Support other axes?
    // TODO: Support multiple axes?
    int xAxis = SDL_CONTROLLER_AXIS_LEFTX;
    int yAxis = SDL_CONTROLLER_AXIS_LEFTY;

    // The distance from center the analog stick will go if a d-pad button is pressed
    // TODO: Let the user set this
    // TODO: Recommend to the user to set this to a number comparable to the analog stick's actual range
    qreal maxRange = 32768.0;

    bool up = gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_UP ] == SDL_PRESSED;
    bool down = gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_DOWN ] == SDL_PRESSED;
    bool left = gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_LEFT ] == SDL_PRESSED;
    bool right = gamepad.button[ SDL_CONTROLLER_BUTTON_DPAD_RIGHT ] == SDL_PRESSED;

    if( up || down || left || right ) {
        qreal angle = 0.0;

        // Check diagonals first
        if( up && right ) {
            angle = 45.0;
        } else if( up && left ) {
            angle = 135.0;
        } else if( down && left ) {
            angle = 225.0;
        } else if( down && right ) {
            angle = 315.0;
        } else if( right ) {
            angle = 0.0;
        } else if( up ) {
            angle = 90.0;
        } else if( left ) {
            angle = 180.0;
        } else { /*if( down )*/
            angle = 270.0;
        }

        // Get coords on a unit circle
        qreal xScale = qCos( qDegreesToRadians( angle ) );
        qreal yScale = qSin( qDegreesToRadians( angle ) );

        // Convert from positive top right coord system to positive bottom right coord system
        yScale = -yScale;

        // Map unit circle range to full range of Sint16 without over/underflowing
        Sint16 xValue = 0;
        Sint16 yValue = 0;
        {
            // Map scales from [-1.0, 1.0] to [0.0, 1.0]
            xScale += 1.0;
            xScale /= 2.0;
            yScale += 1.0;
            yScale /= 2.0;

            // Map scales from [0.0, 1.0] to [0, maxRange + maxRange - 1]
            xScale *= maxRange + maxRange - 1;
            yScale *= maxRange + maxRange - 1;

            // Map scales from [0, maxRange + maxRange - 1] to [-maxRange, maxRange - 1]
            xScale -= maxRange;
            yScale -= maxRange;

            // Convert to int (drops fractional value, not the same as a floor operation!)
            xValue = static_cast<Sint16>( xScale );
            yValue = static_cast<Sint16>( yScale );
        }

        // Finally, set the value and return
        gamepad.axis[ xAxis ] = xValue;
        gamepad.axis[ yAxis ] = yValue;
    } else if( clear ) {
        gamepad.axis[ xAxis ] = 0;
        gamepad.axis[ yAxis ] = 0;
    }

    return gamepad;
}

QString buttonToString( int button ) {
    switch( button ) {
        case SDL_CONTROLLER_BUTTON_A: {
            return QStringLiteral( "A" );
        }

        case SDL_CONTROLLER_BUTTON_B: {
            return QStringLiteral( "B" );
        }

        case SDL_CONTROLLER_BUTTON_X: {
            return QStringLiteral( "X" );
        }

        case SDL_CONTROLLER_BUTTON_Y: {
            return QStringLiteral( "Y" );
        }

        case SDL_CONTROLLER_BUTTON_BACK: {
            return QStringLiteral( "Back" );
        }

        case SDL_CONTROLLER_BUTTON_GUIDE: {
            return QStringLiteral( "Guide" );
        }

        case SDL_CONTROLLER_BUTTON_START: {
            return QStringLiteral( "Start" );
        }

        case SDL_CONTROLLER_BUTTON_LEFTSTICK: {
            return QStringLiteral( "L3" );
        }

        case SDL_CONTROLLER_BUTTON_RIGHTSTICK: {
            return QStringLiteral( "R3" );
        }

        case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: {
            return QStringLiteral( "L" );
        }

        case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: {
            return QStringLiteral( "R" );
        }

        case SDL_CONTROLLER_BUTTON_DPAD_UP: {
            return QStringLiteral( "Up" );
        }

        case SDL_CONTROLLER_BUTTON_DPAD_DOWN: {
            return QStringLiteral( "Down" );
        }

        case SDL_CONTROLLER_BUTTON_DPAD_LEFT: {
            return QStringLiteral( "Left" );
        }

        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: {
            return QStringLiteral( "Right" );
        }

        default: {
            return QStringLiteral( "INVALID" );
        }
    }
}

int stringToButton( QString button ) {
    if( button == QStringLiteral( "A" ) ) {
        return SDL_CONTROLLER_BUTTON_A;
    } else if( button == QStringLiteral( "B" ) ) {
        return SDL_CONTROLLER_BUTTON_B;
    } else if( button == QStringLiteral( "X" ) ) {
        return SDL_CONTROLLER_BUTTON_X;
    } else if( button == QStringLiteral( "Y" ) ) {
        return SDL_CONTROLLER_BUTTON_Y;
    } else if( button == QStringLiteral( "Back" ) ) {
        return SDL_CONTROLLER_BUTTON_BACK;
    } else if( button == QStringLiteral( "Guide" ) ) {
        return SDL_CONTROLLER_BUTTON_GUIDE;
    } else if( button == QStringLiteral( "Start" ) ) {
        return SDL_CONTROLLER_BUTTON_START;
    } else if( button == QStringLiteral( "L3" ) ) {
        return SDL_CONTROLLER_BUTTON_LEFTSTICK;
    } else if( button == QStringLiteral( "R3" ) ) {
        return SDL_CONTROLLER_BUTTON_RIGHTSTICK;
    } else if( button == QStringLiteral( "L" ) ) {
        return SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    } else if( button == QStringLiteral( "R" ) ) {
        return SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    } else if( button == QStringLiteral( "Up" ) ) {
        return SDL_CONTROLLER_BUTTON_DPAD_UP;
    } else if( button == QStringLiteral( "Down" ) ) {
        return SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    } else if( button == QStringLiteral( "Left" ) ) {
        return SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    } else if( button == QStringLiteral( "Right" ) ) {
        return SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
    } else {
        return SDL_CONTROLLER_BUTTON_INVALID;
    }
}

