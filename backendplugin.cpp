#include "backendplugin.h"

// Nodes
#include "controloutput.h"
#include "gameconsole.h"
#include "globalgamepad.h"
#include "phoenixwindownode.h"
#include "videooutputnode.h"

// QML objects
#include "phoenixwindow.h"
#include "remappermodel.h"
#include "videooutput.h"

// Misc
#include "logging.h"

void BackendPlugin::registerTypes( const char *uri ) {
    // QML-owned nodes
    qmlRegisterType<ControlOutput>( uri, 1, 0, "ControlOutput" );
    qmlRegisterType<GameConsole>( uri, 1, 0, "GameConsole" );
    qmlRegisterType<GlobalGamepad>( uri, 1, 0, "GlobalGamepad" );
    qmlRegisterType<PhoenixWindowNode>( uri, 1, 0, "PhoenixWindowNode" );
    qmlRegisterType<VideoOutputNode>( uri, 1, 0, "VideoOutputNode" );

    // Other QML-owned classes
    qmlRegisterType<PhoenixWindow>( uri, 1, 0, "PhoenixWindow" );
    qmlRegisterType<RemapperModel>( uri, 1, 0, "RemapperModel" );
    qmlRegisterType<VideoOutput>( uri, 1, 0, "VideoOutput" );

    // Node structs and enums
    qRegisterMetaType<Node::Command>( "Command" );
    qRegisterMetaType<Node::DataType>( "DataType" );
    qmlRegisterUncreatableType<Node>( uri, 1, 0, "Node", "Node cannot be directly instantiated from QML." );
    qRegisterMetaType<Node::State>( "State" );

    // Misc
    qRegisterMetaType<ProducerFormat>();
    qRegisterMetaType<QStringMap>();
    qRegisterMetaType<size_t>( "size_t" );
}
