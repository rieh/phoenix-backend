#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "backendcommon.h"

#include "inputdevice.h"
#include "inputdeviceevent.h"

// This class represents one Qt keyboard.
// This class connects to the the window's keyPressEvent()
// and keyReleaseEvent() functions, to handle incoming key values
// and turn the into valid RETRO_PAD button.

using InputDeviceMapping = QHash< int, InputDeviceEvent::Event >;

class Keyboard : public InputDevice {
        Q_OBJECT

    public:

        // This needs to be included for proper lookup during compliation.
        // Else the compiler will throw a warning.
        using InputDevice::insert;

        explicit Keyboard( QObject *parent = 0 );
        ~Keyboard();

        InputDeviceMapping &mapping();

        bool loadMapping() override;

    public slots:

        void insert( const int &event, int16_t pressed );
        bool setMappings( const QVariant key, const QVariant mapping, const InputDeviceEvent::EditEventType type ) override;
        void saveMappings() override;

    private:

        void loadDefaultMapping();

        InputDeviceMapping mDeviceMapping;

};

#endif // KEYBOARD_H
