#ifndef UCI_H
#define UCI_H

#include <vector>
#include <string>
#include <iostream>
#include <array>
#include <algorithm>
#include <initializer_list>
#include <sstream>
#include <fstream>
#include "board.h"

namespace morphy {
namespace uci {


// Allows for logging UCI -> Engine traffic to a file.
// This also makes debugging much easier as we can
// use all sorts of in's and out's while testing.
class IOPipe : public std::ostream, std::streambuf {
private:
    std::ofstream logHandle;
    std::istream& in;
    std::ostream& out;
    bool lastWasNewline;

public:

    IOPipe (std::ostream& out, std::istream& in) :
        out(out),
        in(in),
        std::ostream(this),
        lastWasNewline(true)
    {}

    IOPipe (std::ostream& out, std::istream& in, const std::string& path) :
        out(out),
        in(in),
        std::ostream(this),
        lastWasNewline(true)
    {
        logHandle.open(path);
    }

    template <class T>
    void log (const T& value){
        if (logHandle.is_open()) {logHandle << value; logHandle.flush(); }
    }

    std::ostream& logstream () {
        return logHandle;
    }

    std::istream& readLine (std::string& dest) {
        std::istream& v = std::getline(in,dest);
        //in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (v && logHandle.is_open()){ logHandle << "in: " << dest << "\n"; logHandle.flush(); }
        return v;
    }

    int overflow (int c) {
        if (lastWasNewline) log("out: ");
        out.put(c);
        if (logHandle.is_open()) {
            logHandle.put(c);
            logHandle.flush();
        }
        lastWasNewline = c == '\n';
        return 0;
    }
};

enum UCIMessageType {
    UCI, DEBUG, ISREADY, SETOPTION, REGISTER,
    UCINEWGAME, POSITION, GO, STOP, PONDERHIT,
    QUIT, ID, UCIOK, READYOK, BESTMOVE, COPYPROTECTION,
    REGISTRATION, INFO, OPTION, INVALID
};

struct UCIMessage {
    UCIMessageType command;
    std::vector<std::string> params;

    UCIMessage () {}
    UCIMessage (UCIMessageType cmd, const std::initializer_list<std::string>& opts) :
        command(cmd)
    {
        for (const auto& p : opts) {
            params.emplace_back(p);
        }
    }
};

class UCIConfigurator {
private:
    std::stringstream _stream;

public:
    UCIConfigurator () {}

    UCIConfigurator& setEngineName (const std::string& name);
    UCIConfigurator& setAuthorName (const std::string& name);
    UCIConfigurator& setHashRange (size_t min, size_t max, size_t def = 1);
    UCIConfigurator& setNalimovTableBase (const std::string& path, size_t min, size_t max);
    UCIConfigurator& enablePonder (bool enabled);
    UCIConfigurator& enableOwnBook (bool enabled);
    UCIConfigurator& setMultiPV (size_t v);
    UCIConfigurator& enableShowCurrLine (bool enabled);
    UCIConfigurator& enableShowRefutations (bool enabled);
    UCIConfigurator& setELORange (size_t min, size_t max);
    UCIConfigurator& enableAnalyzeMode (bool enabled);
    void build (std::stringstream& out);
    void build (std::ostream& out);
};


bool parseMove (const std::string& str, uint16_t& from, uint16_t& to);
void splitString (const std::string& str, std::vector<std::string>& strs, char delim);
void setSpinOption (std::ostream& stream, const std::string& name, const size_t def, const size_t min, const size_t max);
void setComboOption (std::ostream& stream, const std::string& name, const std::initializer_list<const std::string>& opts);
void setCheckOption (std::ostream& stream, const std::string& name, bool enabled);
void setStringOption (std::ostream& stream, const std::string& name, const std::string& value);

void signalReady (std::ostream& stream);

void signalBestMove (std::ostream& stream, const Move& move);
void signalBestMove (std::ostream& stream, const Move& move, const Move& ponder);
void logMessage (std::ostream& stream, const std::string& message);
//void moveGenInfo (std::stringstream& stream, const MoveGenState& gen);



} // end namespace
} // end namespace

#endif // UCI_H
