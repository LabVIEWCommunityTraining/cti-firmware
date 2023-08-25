
#ifndef CIRCULARBUFFER_H_
#define CIRCULARBUFFER_H_

#include <stdint.h>

class CircularBuffer {
public:
    CircularBuffer(bool lossy = true);
    ~CircularBuffer();
    
    bool isFull();
    bool isEmpty();
    uint8_t count();
    
    void put(char c);
    char get();
    
private:
    char _buf[256];
    uint8_t _head;
    uint8_t _tail;
    
    bool _lossy;
};

#endif //CIRCULARBUFFER_H_
