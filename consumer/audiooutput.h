#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include "avformat.h"
#include "audiobuffer.h"
#include "pipelinenode.h"

#include <samplerate.h>

#include <QAudioOutput>

/* The AudioOutput class writes data to the default output device. Its internal buffers must be set by calling
 * slotAudioFormat() with the proper arguments before any data can be passed to it with slotAudioData(). In addition,
 * it has the ability to pause and resume whether or not it 'expects' audio with slotSetAudioActive(). Make this match
 * whether or not the core is paused and you'll not have any underruns (hopefully). Set the volume (from 0 to 1
 * inclusive) with slotSetVolume().
 *
 * For clarity, assuming 16-bit stereo audio:
 * 1 frame = 4 bytes (L, L, R, R)
 * 1 sample = 2 bytes (L, L) or (R, R)
 */

class AudioOutput : public QObject {
        Q_OBJECT
        PHX_PIPELINE_INTERFACE( AudioOutput )
    public:
        explicit AudioOutput( QObject *parent = 0 );
        ~AudioOutput();

    public slots:
        void consumerFormat( AVFormat consumerFmt );
        //void consumerData( QString type, QMutex *mutex, void *data, size_t bytes , qint64 timestamp );

        // Systems have varying native framerates (coreFPS) which determine the *amount* of audio we'll get each
        // video frame period. This could be different from the rate frame production is driven (hostFPS).
        // AudioOutput will automatically compensate for this. Use this slot to set hostFPS.
        // Must be called *after* consumerData (which will make hostFPS = coreFPS)
        void libretroSetFramerate( qreal hostFPS );

        void stateIn( PipeState t_state );

        void controlIn( Command t_cmd, QVariant t_data);

        void dataIn( DataReason t_reason
                     , QMutex *
                     , void *t_data
                     , size_t t_bytes
                     , qint64 t_timeStamp );

    private slots:
        void handleStateChanged( QAudio::State currentState );
        void handleUnderflow();



signals:
        void dataOut( DataReason reason
                     , QMutex *producerMutex
                     , void *data
                     , size_t bytes
                     , qint64 timeStamp );

        void controlOut( Command t_cmd
                        , QVariant data );

        void stateOut( PipeState t_state );

    private:

        AVFormat m_avFormat;

        // Respond to the core running or not by keeping audio output active or not
        // AKA we'll pause if core is paused
        void setAudioActive( bool coreIsRunning );

        // Output incoming video frame of audio data to the audio output
        void audioData( int16_t *inputData, int inputBytes );

        // Free memory, clean up
        void shutdown();

        // Completely init/re-init audio output and resampler
        void resetAudio();

        // Allocate memory for conversion
        void allocateMemory();

        // Opaque pointer for libsamplerate
        SRC_STATE *resamplerState{ nullptr };

        // Audio and video timing provided by Core via the controller
        int sampleRate{ 0 };
        double hostFPS{ 60.0 };
        double coreFPS{ 60.0 };
        double sampleRateRatio{ 1.0 };

        // Internal buffers used for resampling
        short *inputDataShort{ nullptr };
        float *inputDataFloat{ nullptr };
        float *outputDataFloat{ nullptr };
        short *outputDataShort{ nullptr };

        // Set to true if the core is currently running
        bool coreIsRunning{ false };

        // Input and output audio formats being used
        QAudioFormat outputAudioFormat;
        QAudioFormat inputAudioFormat;

        // An interface to the output device
        QAudioOutput *outputAudioInterface{ nullptr };

        // Size of outputBuffer's unconsumed data
        int outputCurrentByte{ 0 };

        // A buffer that removes data from itself once it's read
        AudioBuffer outputBuffer;

        //
        // TODO: Make these configurable
        //

        // Max size of the outputBuffer. Equivalent to "audio buffering" setting in other programs
        int outputLengthMs{ 200 };

        // Ideal amount of data in the output buffer. Make this large enough to ensure no underruns
        int outputTargetMs{ 40 };

        // Max amount of stretching performed to compensate for output buffer position being off target
        double maxDeviation{ 0.005 };

        //
        // ---
        //

};

#endif
