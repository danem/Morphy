#include "../include/uci.h"

using namespace morphy::uci;

template <class T>
static int findIndexOf (T const * begin, T const * end, const T& value){
    auto v = std::find(begin, end, value);
    if (v == end) return -1;
    return v - begin;
}

void morphy::uci::splitString (const std::string& str, std::vector<std::string>& strs, char delim) {
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        strs.emplace_back(item);
    }
}

bool morphy::uci::parseMove (const std::string& str, uint16_t& from, uint16_t& to) {
    if (str.length() != 4) return false;
    uint16_t fx = str[0] - 97;
    uint16_t fy = str[1] - 49;
    uint16_t tx = str[2] - 97;
    uint16_t ty = str[3] - 49;
    if (fx < 0 || fx > 7 || tx < 0 || tx > 7
     || fy < 0 || fy > 7 || ty < 0 || ty > 7) return false;
    from = fy * 8 + fx;
    to = ty * 8 + tx;
    return true;
}


void morphy::uci::setSpinOption (std::ostream& stream, const std::string& name, const size_t def, const size_t min, const size_t max) {
    stream << "option name " << name << " type spin "
            << "default " << def << " min " << min << " max " << max << "\n";
}

void morphy::uci::setComboOption (std::ostream& stream, const std::string& name, const std::initializer_list<const std::string>& opts) {
    stream << "option name " << name << "type combo ";
    bool first = true;
    for (const auto& v : opts) {
        if (first) stream << "default " << v;
        else stream << " var " << v;
        first = false;
    }
    stream << "\n";
}

void morphy::uci::setCheckOption (std::ostream& stream, const std::string& name, bool enabled) {
    stream << "option name " << name << " type check " << "default " << (enabled ? "true" : "false") << "\n";
}

void morphy::uci::setStringOption (std::ostream& stream, const std::string& name, const std::string& value) {
    stream << "option name " << name << " type text " << "default " << value << "\n";
}


UCIConfigurator& UCIConfigurator::setEngineName (const std::string& name) {
    _stream << "id name " << name << "\n";
    return *this;
}

UCIConfigurator& UCIConfigurator::setAuthorName (const std::string& name) {
    _stream << "id author " << name << "\n";
    return *this;
}

UCIConfigurator& UCIConfigurator::setHashRange (size_t min, size_t max, size_t def) {
    setSpinOption(_stream, "Hash", def, min, max);
    return *this;
}

UCIConfigurator& UCIConfigurator::setNalimovTableBase (const std::string& path, size_t min, size_t max) {
    setStringOption(_stream, "NamilovPath", path);
    setSpinOption(_stream, "NamilovCache", min, max, min);
    return *this;
}

UCIConfigurator& UCIConfigurator::enablePonder (bool enabled) {
    setCheckOption(_stream, "Ponder", enabled);
    return *this;
}

UCIConfigurator& UCIConfigurator::enableOwnBook (bool enabled) {
    setCheckOption(_stream, "OwnBook", enabled);
    return *this;
}

UCIConfigurator& UCIConfigurator::setMultiPV (size_t v) {
    setSpinOption(_stream, "MultiPV", v, v, 10);
    return *this;
}

UCIConfigurator& UCIConfigurator::enableShowCurrLine (bool enabled) {
    setCheckOption(_stream, "UCI_ShowCurrLine", enabled);
    return *this;
}

UCIConfigurator& UCIConfigurator::enableShowRefutations (bool enabled) {
    setCheckOption(_stream,"UCI_ShowRefutations", enabled);
    return *this;
}

UCIConfigurator& UCIConfigurator::setELORange (size_t min, size_t max) {
    setCheckOption(_stream, "UCI_LimitStrength", true);
    setSpinOption(_stream, "UCI_Elo", min, min, max);
    return *this;
}

UCIConfigurator& UCIConfigurator::enableAnalyzeMode (bool enabled) {
    setCheckOption(_stream, "UCI_AnalyseMode", enabled);
    return *this;
}

void UCIConfigurator::build(std::stringstream& out) {
    _stream << "uciok\n";
    out << _stream.str();
}

void UCIConfigurator::build(std::ostream& out) {
    _stream << "uciok\n";
    out << _stream.str();
}

void morphy::uci::signalReady (std::ostream& stream) {
    stream << "readyok\n";
}

static std::string moveToString (const morphy::Move& move) {
    std::stringstream stream;
    char fx = static_cast<char>((move.from % 8)+97);
    uint16_t fy = move.from / 8 + 1;
    char tx = static_cast<char>((move.to % 8)+97);
    uint16_t ty = move.to / 8 + 1;
    stream << fx << fy << tx << ty;
    return stream.str();

}

void morphy::uci::signalBestMove (std::ostream& stream, const Move& move) {
    stream << "bestmove " << moveToString(move) << "\n";
}

void morphy::uci::signalBestMove (std::ostream& stream, const Move& move, const Move& ponder) {
    stream << "bestmove " << moveToString(move) << " ponder " << moveToString(ponder) << "\n";
}

void morphy::uci::logMessage (std::ostream& stream, const std::string& message) {
    stream << "info string " << message << "\n";
}

//void morphy::uci::moveGenInfo (std::stringstream& stream, const MoveGenState& gen) {
//}


