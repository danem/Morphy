#include <vector>
#include <array>
#include <iostream>
#include <time.h>
#include <random>
#include <string>
#include <unistd.h>
#include <sys/wait.h>

#include <morphy/engine.h>
#include <morphy/uci.h>


using namespace morphy;


int main () {
    //testMasks();
    //std::istringstream input("position startpos moves a2a3 g8f6 h2h4 b8c6 e2e4 f6e4 f2f3 e4g3 h4h5 g3h1 d2d3 e7e5\n");
    std::istringstream input("position startpos moves a2a3 g8f6 h2h4 b8c6\nisready\ngo movetime 1000\n");
    std::cout.setf(std::ios::unitbuf);
    //uci::IOPipe io(std::cout, std::cin, "./log.txt");
    uci::IOPipe io(std::cout, input, "./log.txt");

    Engine engine;
    UCIAdaptor uciengine(engine,io);

    std::vector<std::string> message;
    while (uciengine.isRunning()) {
        std::string line;
        io.readLine(line);
        if (line.length() > 0) {
            uci::splitString(line,message, ' ');
            uciengine.handleUCIMessage(message);
        }
        message.clear();
    }
}

