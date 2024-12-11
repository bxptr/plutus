#include "EngineController.h"
#include "Replay.h"
#include "Logging.h"
#include "NetworkInterface.h"
#include "EventLoop.h"
#include "SymbolConfig.h"

int main() {
    GLOBAL_LOG_LEVEL = LogLevel::INFO;
    Replay replayLog;
    SymbolConfigManager configManager;
    EngineController controller(replayLog, configManager);

    // Add some symbols
    controller.addEngineForSymbol("AAPL", 0.01, 1, 1.00, 10000.00, 0.5, 150.00);
    controller.addEngineForSymbol("BTCUSD", 0.01, 1, 1000.00, 100000.00, 0.3, 20000.00);

    NetworkInterface net;
    int listenFd = net.setupListener("", 9999);
    if (listenFd < 0) {
        LOG(LogLevel::ERROR, "Failed to setup listener");
        return 1;
    }

    EventLoop loop(controller);
    if (!loop.init(listenFd)) {
        LOG(LogLevel::ERROR, "Failed to init event loop");
        return 1;
    }

    LOG(LogLevel::INFO, "Server running on port 9999");
    loop.run();

    return 0;
}

