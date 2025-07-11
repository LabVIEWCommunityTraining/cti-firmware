#include "cti/platform.h"

#include <string.h>
#include <stdarg.h>

namespace CTI {

    PlatformIO::PlatformIO() {
        _curStream = 0;
        _streams = nullptr;
    }

    void PlatformIO::RegisterStreams(StreamIOImpl *streams[CTI_NUM_STREAMS]) {
        _streams = streams;
    }

    int PlatformIO::FGetCtimeout(uint32_t timeout_us, bool allStreams) {
        int c;
        CTI_DEBUG(" S!:%d", _curStream);
        c = _streams[_curStream]->getchar(timeout_us);

        if (c == -1 && allStreams) {
            //timed out on current stream, check others and update curStream
            for (int i = 0; i < CTI_NUM_STREAMS; ++i) {
                if (i != _curStream) {
                    CTI_DEBUG(" S:%d", i);
                    c = _streams[i]->getchar(timeout_us);
                    if (c != -1) {
                        _curStream = i;
                        break;
                    }
                }
            }
        }

        CTI_DEBUG(" c:%d '%c'", c, c);

        return c;
    }

    void PlatformIO::StatusLED(bool val, StatusSource source) {
        if (source == User) {
            //If the user explicitly set the LED, do what the user said
            // and set the source to user to disable other auto sources
            _statusSource = User;
            _statusLED(val);
        } else if (source == _statusSource) {
            //otherwise, the source must match the configured status source
            _statusLED(val);
        }
    }

} //namespace CTI