#pragma once

#include <QDir>
#include <QFile>
#include <QHash>
#include <QLibrary>
#include <QObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QRect>
#include <QSurface>

#include "core.h"
#include "gamepadstate.h"
#include "libretro.h"
#include "libretrosymbols.h"
#include "libretrovariable.h"
#include "logging.h"
#include "node.h"
#include "mousestate.h"

// Since each buffer holds one frame, depending on core, 30 frames = ~500ms
#define POOL_SIZE 30

/*
 * C++ wrapper around a Libretro core. Currently, only one LibretroCore instance may safely exist at any time due to the
 * lack of a context pointer for callbacks to use.
 *
 * The following keys are mandatory for source from setSource():
 * "type": "libretro"
 * "core": Absolute path to the Libretro core
 * "game": Absolute path to a game the Libretro core accepts
 * "systemPath": Absolute path to the system directory (contents of which depend on the core)
 * "savePath": Absolute path to the save directory
 *
 * LibretroCore expects some kind of input producer (such as InputManager) to produce input which LibretroCore will then
 * consume. This input production also drives the production of frames (retro_run()), so time it such that it's as close
 * as possible to the console's native framerate!
 *
 * This also means that you can send updates from the input producer whenever you want, at any stage. retro_run() will
 * not be called unless you're in the PLAYING state.
 */

class LibretroCore : public Core {
        Q_OBJECT

    public:
        explicit LibretroCore( Core *parent = nullptr );
        ~LibretroCore();

    signals:
        void commandOut( Node::Command command, QVariant data, qint64 timeStamp );
        void dataOut( Node::DataType type, QMutex *mutex, void *data, size_t bytes, qint64 timeStamp );

    public:
        // Struct containing libretro methods
        LibretroSymbols symbols;

        // Used by environment callback. Provides info about the OpenGL context provided by the Phoenix frontend for
        // the core's internal use
        retro_hw_render_callback openGLContext;

        // Files and paths

        QLibrary coreFile;
        QFile gameFile;

        QDir contentPath;
        QDir systemPath;
        QDir savePath;

        // These must be members so their data stays valid throughout the lifetime of LibretroCore
        QFileInfo coreFileInfo;
        QFileInfo gameFileInfo;
        QFileInfo systemPathInfo;
        QFileInfo savePathInfo;
        QByteArray corePathByteArray;
        QByteArray gameFileByteArray;
        QByteArray gamePathByteArray;
        QByteArray systemPathByteArray;
        QByteArray savePathByteArray;
        const char *corePathCString{ nullptr };
        const char *gameFileCString{ nullptr };
        const char *gamePathCString{ nullptr };
        const char *systemPathCString{ nullptr };
        const char *savePathCString{ nullptr };

        // Raw ROM/ISO data, empty if (systemInfo->need_fullpath)
        QByteArray gameData;

        // SRAM

        void *saveDataBuf{ nullptr };

        // Core-specific constants

        // Information about the core (we store, Libretro core fills out with symbols.retro_get_system_info())
        retro_system_info *systemInfo;

        // Information about the controllers and buttons used by the core
        // FIXME: Where's the max number of controllers defined?
        // Key format: "port,device,index,id" (all 4 unsigned integers are represented as strings)
        //     ex. "0,0,0,0"
        // Value is a human-readable description
        QMap<QString, QString> inputDescriptors;

        // Node data

        Node::State currentState;

        // Producer data (for consumers like AudioOutput, VideoOutput...)

        // Get AV info from the core and pass along to consumers
        void getAVInfo( retro_system_av_info *avInfo );

        // Format going out to the consumers
        ProducerFormat producerFmt;

        // Mutex for consumers (ensures reads/writes to/from the buffer pool are atomic)
        QMutex mutex;

        // Circular buffer pools. Used to avoid having to track when consumers have consumed a buffer
        int16_t *audioBufferPool[ POOL_SIZE ] { nullptr };
        int audioPoolCurrentBuffer{ 0 };

        // Amount audioBufferPool[ audioBufferPoolIndex ] has been filled
        // Each frame, exactly ( sampleRate * 4 ) bytes should be copied to
        // audioBufferPool[ audioBufferPoolIndex ][ audioBufferCurrentByte ] in total
        // FIXME: In practice, that's not always the case? Some cores only hit that *on average*
        int audioBufferCurrentByte{ 0 };

        uint8_t *videoBufferPool[ POOL_SIZE ] { nullptr };
        int videoPoolCurrentBuffer{ 0 };

        // Video
        QOpenGLContext *context { nullptr };
        QOpenGLFramebufferObject *fbo { nullptr };
        QSurface *surface { nullptr };

        // Audio

        qreal audioSampleRate{ 44100 };

        // Input

        ProducerFormat consumerFmt;

        QHash<int, GamepadState> gamepads;

        MouseState mouse;
        QRect geometry;
        int aspectMode { 0 };

        // Callbacks

        // Used by audio callback
        void emitAudioData( void *data, size_t bytes );

        // Used by video callback
        void emitVideoData( void *data, unsigned width, unsigned height, size_t pitch, size_t bytes );

        // Misc

        // Core-specific variables
        QMap<QByteArray, LibretroVariable> variables;

        // True if variables are dirty and the core needs to know about them
        bool variablesAreDirty{ false };

        void updateVariables();

};

// Libretro is a C API. This limits us to one LibretroCore per process.
extern LibretroCore core;

// SRAM
void loadSaveData();
void storeSaveData();

// Should only be called on load time (consumers expect buffers to be valid while Core is active)
void allocateBufferPool( retro_system_av_info *avInfo );

// Callbacks
void audioSampleCallback( int16_t left, int16_t right );
size_t audioSampleBatchCallback( const int16_t *data, size_t frames );
bool environmentCallback( unsigned cmd, void *data );
void inputPollCallback( void );
void logCallback( enum retro_log_level level, const char *fmt, ... );
int16_t inputStateCallback( unsigned port, unsigned device, unsigned index, unsigned id );
void videoRefreshCallback( const void *data, unsigned width, unsigned height, size_t pitch );

// Helper that generates key for looking up the inputDescriptors
QString inputTupleToString( unsigned port, unsigned device, unsigned index, unsigned id );
