#ifndef visa_core_h_
#define visa_core_h_

#include <vector>
#include <string>

namespace Visa {

class OutputStream;

typedef struct {
    char* response;
    int status;
    bool freeResponse;
} Response;

typedef Response* (*CommandHandler)(const char* pattern, std::vector<std::string> params);

typedef struct {
    const char* pattern;
    CommandHandler handler;
} Command;

int registerCommands(Command* commands, int count);
int removeCommands(Command* commands, int count);

bool parseCommand(const char* command);

}; //namespace Visa

#endif //visa_core_h_