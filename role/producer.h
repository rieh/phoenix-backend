#pragma once
#ifndef PRODUCER_H
#define PRODUCER_H

#include <QAudioFormat>
#include <QImage>
#include <QMutex>

/*
 * Functionality and structures common to all producers.
 *
 * To declare these signals you must use the define provided:
 * class ProducerSubclass : public QObject, public Producer {
 *     signals:
 *         PRODUCER_SIGNALS
 *         void anyOtherSignals();
 * };
 *
 * To connect to these signals:
 * connect( dynamic_cast<QObject *>( ProducerSubclassPtr ), SIGNAL( producerSignal( argType, anotherArgType ) ),
 *          dynamic_cast<QObject *>( ConsumerSubclassPtr ), SLOT( consumerSlot( argType, anotherArgType ) ) );
 *
 * Explanation:
 *
 * The old syntax is necessary here as we're doing some fancy tricks to work around some limitations of Qt.
 * Qt does not support QObjects inheriting from multple QObjects, but we want Producer and Consumer to provide common
 * signals and slots to all kinds of QObject-derived classes. So, we declare the signals and slots anyway and just
 * have Producer and Consumer be plain classes, doing some hacky stuff with this old syntax to make it work somehow.
 *
 * Thanks to peppe and thiago from #Qt on Freenode for the idea
 */

// In order to properly declare the signals in your subclass, simply use this macro under "signals:"
// Documented below
#define PRODUCER_SIGNALS \
    void producerData( QString type, QMutex *producerMutex, void *data, size_t bytes, qint64 timestamp ); \
    void producerFormat( ProducerFormat format );

#define CONNECT_PRODUCER_CONSUMER( producer, consumer ) \
    connect( dynamic_cast<QObject *>( producer ), SIGNAL( producerData( QString, QMutex *, void *, size_t, qint64 ) ), \
             dynamic_cast<QObject *>( consumer ), SLOT( consumerData( QString, QMutex *, void *, size_t, qint64 ) ) ); \
    connect( dynamic_cast<QObject *>( producer ), SIGNAL( producerFormat( ProducerFormat ) ), \
             dynamic_cast<QObject *>( consumer ), SLOT( consumerFormat( ProducerFormat ) ) )

#define CLIST_CONNECT_PRODUCER_CONSUMER( producer, consumer ) \
    connectionList << connect( dynamic_cast<QObject *>( producer ), SIGNAL( producerData( QString, QMutex *, void *, size_t, qint64 ) ), \
             dynamic_cast<QObject *>( consumer ), SLOT( consumerData( QString, QMutex *, void *, size_t, qint64 ) ) ); \
    connectionList << connect( dynamic_cast<QObject *>( producer ), SIGNAL( producerFormat( ProducerFormat ) ), \
             dynamic_cast<QObject *>( consumer ), SLOT( consumerFormat( ProducerFormat ) ) )

// Type of video output (for use by video consumers)
enum VideoRendererType {

    // Video is generated by the CPU and lives in RAM
    SOFTWARERENDER = 0,

    // Video is generated by the GPU and lives in an FBO
    HARDWARERENDER

};

// Information for the consumer from the producer
struct ProducerFormat {

    ProducerFormat();
    ~ProducerFormat();

    // Control

    // "libretro", etc.
    QString producerType;

    // Audio

    QAudioFormat audioFormat;

    // If audio data is sent at a regular rate, but the amount is too much/insufficient to keep the buffer from
    // over/underflowing, stretch the incoming audio data by this factor to compensate
    // In Libretro cores, this factor compensates for the emulation rate differing from the console's native framerate
    // if using VSync, for example
    // The ratio is hostFPS / coreFPS
    qreal audioRatio;

    // Video

    qreal videoAspectRatio;
    size_t videoBytesPerLine;
    size_t videoBytesPerPixel;
    qreal videoFramerate;
    VideoRendererType videoMode;
    QImage::Format videoPixelFormat;
    QSize videoSize;

};
Q_DECLARE_METATYPE( ProducerFormat )
Q_DECLARE_METATYPE( size_t )

class Producer {

    public:
        Producer();
        ~Producer();

        // These signals have been commented out because the linker will complain no implementation of them
        // exists for Producer. Remember, this does NOT have the Q_OBJECT macro and is NOT a QObject!

        // Data for consumers. Pointers will be valid for the lifetime of the producer
        // Use QDateTime::currentMSecsSinceEpoch() to get the time
        // void producerData( QString type, QMutex *producerMutex, void *data, size_t bytes, qint64 timestamp );

        // Format information for consumers.
        // void producerFormat( ProducerFormat format );

    protected:
        // A copy of the data we send out to the consumers
        ProducerFormat producerFmt;

        QMutex producerMutex;

};

#endif // PRODUCER_H
