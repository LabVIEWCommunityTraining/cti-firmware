#include "circularbuffer.h"

CircularBuffer::CircularBuffer(bool lossy) {
}

CircularBuffer::~CircularBuffer() {
}

bool CircularBuffer::isFull() {
    return (_head + 1 == _tail);
}

bool CircularBuffer::isEmpty() {
    return (_head == _tail);
}

uint8_t CircularBuffer::count() {
    return (_head - _tail);
}

void CircularBuffer::put(char c) {
    if (!_lossy && isFull()) {
        return;
    }
    
    _buf[_head++] = c;
    
    if (_head == _tail) ++_tail;
}

char CircularBuffer::get() {
    if (isEmpty())
        return 0xff;
    
    return _buf[_tail++];
}
