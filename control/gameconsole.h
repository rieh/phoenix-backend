#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QThread>
#include <QVariant>

#include "node.h"

// Nodes
#include "audiooutput.h"
#include "gamepadmanager.h"
#include "controloutput.h"
#include "globalgamepad.h"
#include "keyboardmouselistener.h"
#include "keyboardmanager.h"
#include "libretroloader.h"
#include "libretrorunner.h"
#include "microtimer.h"
#include "phoenixwindow.h"
#include "phoenixwindownode.h"
#include "remapper.h"
#include "remappermodel.h"
#include "videooutput.h"
#include "videooutputnode.h"

#include "libretrovariablemodel.h"
#include "libretrovariableforwarder.h"

/*
 * GameConsole's role is twofold:
 * 1. Manage the lifecycle of the global pipeline and every dynamic pipeline (invoked by writing to the source property
 *    and calling load())
 * 2. Pass messages to the pipeline, but only once a dynamic pipeline is established. These messages include commands
 *    like load() (which does some extra management stuff), play(), pause(), etc. along with setters like
 *    playbackSpeed, source, etc.
 *
 * Other than to satisfy those two roles (and to cleanup on shutdown), GameConsole has no other logic nor should it.
 * At its core, its primary role is to simply pass messages to the pipeline from QML.
 *
 * To hook the propogation of these commands down to the running Core, see ControlOutput. Usually this is directly
 * connected to the Core of a particular dynamic pipeline.
 */

class MouseManager;

class GameConsole : public Node {
        Q_OBJECT

        Q_PROPERTY( ControlOutput *controlOutput MEMBER controlOutput NOTIFY controlOutputChanged )
        Q_PROPERTY( GlobalGamepad *globalGamepad MEMBER globalGamepad NOTIFY globalGamepadChanged )
        Q_PROPERTY( PhoenixWindowNode *phoenixWindow MEMBER phoenixWindow NOTIFY phoenixWindowChanged )
        Q_PROPERTY( RemapperModel *remapperModel MEMBER remapperModel NOTIFY remapperModelChanged )
        Q_PROPERTY( LibretroVariableModel *variableModel MEMBER variableModel NOTIFY variableModelChanged )
        Q_PROPERTY( VideoOutputNode *videoOutput MEMBER videoOutput NOTIFY videoOutputChanged )

        Q_PROPERTY( int aspectRatioMode READ getAspectRatioMode WRITE setAspectRatioMode NOTIFY aspectRatioModeChanged )
        Q_PROPERTY( qreal playbackSpeed READ getPlaybackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged )
        Q_PROPERTY( QVariantMap source READ getSource WRITE setSource NOTIFY sourceChanged )
        Q_PROPERTY( qreal volume READ getVolume WRITE setVolume NOTIFY volumeChanged )
        Q_PROPERTY( bool vsync READ getVsync WRITE setVsync NOTIFY vsyncChanged )
        Q_PROPERTY( QString userDataLocation MEMBER userDataLocation NOTIFY userDataLocationChanged)

    public:
        explicit GameConsole( Node *parent = nullptr );
        ~GameConsole() = default;

    public slots:
        void load();
        void play();
        void pause();
        void stop();
        void reset();
        void unload();

    private: // Startup
        // API-specific loaders
        void loadLibretro();

        // Return true if all global pipeline members from QML are set
        bool globalPipelineReady();

        // Emit Command::GlobalPipelineReady if it's ready to go
        void checkIfGlobalPipelineReady();

        // Return true if a core is loaded and its dynamic pipeline is hooked to the global one
        bool dynamicPipelineReady();

        // QVariantMap that holds property changes that were set before the global pipeline loaded
        QVariantMap pendingPropertyChanges;
        void applyPendingPropertyChanges();

    private: // Cleanup
        // Keeps track of session connections so they may be disconnected once emulation ends
        QList<QMetaObject::Connection> sessionConnections;

        // Used to stop the game thread on app quit
        bool quitFlag{ false };

        // API-specific unloaders
        void unloadLibretro();
        void deleteLibretro();

        // Sends deferred delete events to everything, safe (also required) to call after calling your API-specific deleter
        void deleteMembers();

    private: // Members
        // Emulation thread
        QThread *gameThread;

        // Pipeline nodes owned by this class (game thread)
        // When adding a new one, remember to:
        //     - Add to deleteMembers() if adding a new one no matter what pipeline it belongs to
        //     - Move to gameThread in the constructor
        AudioOutput *audioOutput { nullptr };
        GamepadManager *gamepadManager { nullptr };
        KeyboardManager *keyboardManager { nullptr };
        LibretroLoader *libretroLoader { nullptr };
        LibretroRunner *libretroRunner { nullptr };
        MicroTimer *microTimer { nullptr };
        MouseManager *mouseManager{ nullptr };
        Remapper *remapper { nullptr };

        // Pipeline nodes owned by the QML engine (main thread)
        // Must be given to us via properties
        ControlOutput *controlOutput { nullptr };
        GlobalGamepad *globalGamepad { nullptr };
        LibretroVariableModel *variableModel { nullptr };
        PhoenixWindowNode *phoenixWindow { nullptr };
        VideoOutputNode *videoOutput { nullptr };

        // Misc (owned by us)
        KeyboardMouseListener keyboardMouseListener;
        LibretroVariableForwarder libretroVariableForwarder;

        // Misc (not owned by us)
        RemapperModel *remapperModel { nullptr };

    private: // Property getters/setters
        int aspectRatioMode { 0 };
        int getAspectRatioMode();
        void setAspectRatioMode( int aspectRatioMode );
        qreal playbackSpeed { 1.0 };
        qreal getPlaybackSpeed();
        void setPlaybackSpeed( qreal playbackSpeed );
        QVariantMap source;
        QVariantMap getSource();
        void setSource( QVariantMap source );
        qreal volume { 1.0 };
        qreal getVolume();
        void setVolume( qreal volume );
        bool vsync { true };
        bool getVsync();
        void setVsync( bool vsync );
        QString userDataLocation;

    signals: // Property changed notifiers
        void controlOutputChanged();
        void globalGamepadChanged();
        void phoenixWindowChanged();
        void remapperModelChanged();
        void videoOutputChanged();
        void variableModelChanged();

        void aspectRatioModeChanged();
        void playbackSpeedChanged();
        void sourceChanged();
        void volumeChanged();
        void vsyncChanged();
        void userDataLocationChanged();

};
