#ifndef scpi_core_h_
#define scpi_core_h_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace CTI {
namespace SCPI {

    enum class ParserStatus {
        Success,
        Unknown,
        Incomplete,
        SyntaxError,
        UnknownCommand,
        MissingParameter
    };

    enum class CommandResult {
        Success,
        MissingParam,
        UnexpectedParam,
        SyntaxError,
        NoHandler,
        Error
    };

    enum class QueryResult {
        Success,
        NoHandler,
        Error
    };

    enum class RegistrationResult {
        Success,
        SyntaxError,
        Abiguous,
        InvalidHandler
    } ;

    enum class ParserState {
        Idle,
        ReadingCommand,
        ParseCommand,
        FindCommand,
        InvokeHandler,
        CompleteCommand
    };

    class ScpiNode;
    class ScpiParser;

    typedef CommandResult (*ScpiCommand)(ScpiNode* node, ScpiParser* parser, std::vector<int>* nodeNumbers);
    typedef QueryResult (*ScpiQuery)(ScpiNode* node, std::vector<int>* nodeNumbers);

    class ScpiNode {
    public:
        ScpiNode(std::string_view command, ScpiNode* parent = nullptr);

        CommandResult InvokeCommand(ScpiParser* parser, std::vector<int>* nodeNumbers) {
            if (_cmdHandler != nullptr) {
                return _cmdHandler(this, parser, nodeNumbers);
            }

            return CommandResult::NoHandler;
        };

        QueryResult InvokeQuery(ScpiParser* parser, std::vector<int>* nodeNumbers) {
            if (_queryHandler != nullptr) {
                return _queryHandler(this, nodeNumbers);
            }

            return QueryResult::NoHandler;
        };

    private:
        /// @brief The original segment of the node string at this level as passed into the registration.
        std::string_view _command;

        /// @brief An upper-cased value of the required portion of the node string.
        std::string _reqString;

        /// @brief A lower-cased value of the remaining optional portion of the node string.
        std::string _optString;

        /// @brief Signifies whether the node segment ended with '#' and should track a number param
        bool _hasNumberParam;

        /// @brief Whether the node at this level is option (was surrounded in '[]' during registration).
        bool _optional;

        /// @brief Pointer to the parent level of the tree this node is nested under.
        /// Will be nullptr for the root node.
        ScpiNode* _parent;

        /// @brief  List of pointers to children nodes. Will be empty for leaf nodes.
        std::vector<ScpiNode*> _children;

        ScpiCommand _cmdHandler;
        ScpiQuery _queryHandler;
    };

    class ScpiParser {
    public:
        ScpiParser(int bufSize = 255);
        ~ScpiParser();

        /// @brief Buffers new input characters. As soon as a query/command terminator is seen (space) a handler will be invoked if a match is found.
        /// @param data Buffer of characters to add to the parser.
        /// @param n The count of characters contained in data to buffer.
        /// @return The count of characters actually added, anything less than n means the buffer is out of room.
        int bufferInput(uint8_t* data, int n);

        /// @brief Registers a node in the SCPI command tree to be able to respond to commands and/or queries.
        ///     Note that any string content passed into the registration must remain valid beyond the lifetime of the parser.
        /// @param command The command tree notation in scpi syntax that the node should handle.
        /// @param cmdHandler The handler that is invoked when a command is received matching the node.
        /// @param queryHandler The optional handler when the node matches a SCPI query.
        /// @return An enumerated status of the result of trying to register the node.
        RegistrationResult addScpiNode(std::string_view nodeString, ScpiCommand cmdHandler, ScpiQuery queryHandler = nullptr);

    private:
        ParserStatus parseNode();
        
        uint8_t* _buf;
        int _bufSize;
        int _bufHead;
        int _bufTail;

        ScpiNode* _treeRoot;

        ParserState _state;
    };

} // SCPI
} // CTI

#endif //scpi_core_h_