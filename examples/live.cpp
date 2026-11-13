// Live order book demo using two terminals.
//
//   Terminal 1:  ./build/lob_live          shows the order book, redrawn after every command
//   Terminal 2:  ./build/lob_live order    prompt for typing orders
//
// Orders travel from terminal 2 to terminal 1 through a named pipe (FIFO).

#include "lob/OrderBook.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <deque>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace lob;

namespace {

const char* kPipePath = "/tmp/lob_orders.fifo";
const char* kHelp =
    "buy 10 @ 100 [ioc|fok]   sell 5 @ 102   market buy 3   cancel 2   modify 2 8 @ 101";

// ---------------------------------------------------------------- book viewer

std::deque<std::string> events; // most recent activity, newest last

void logEvent(const std::string& msg) {
    events.push_back(msg);
    if (events.size() > 12) events.pop_front();
}

std::uint64_t parseNumber(const std::string& s) {
    if (s.empty() || !std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); })) {
        throw std::invalid_argument("expected a positive number, got '" + s + "'");
    }
    return std::stoull(s);
}

void logTrades(const std::vector<Trade>& trades) {
    for (const Trade& t : trades) {
        std::ostringstream os;
        os << "  TRADE " << t.quantity << " @ " << t.price << "  (#" << t.takerOrderId
           << (t.takerSide == Side::Buy ? " bought from #" : " sold to #") << t.makerOrderId << ")";
        logEvent(os.str());
    }
}

// Applies one command line such as "buy 10 @ 100 ioc" to the book.
void handleCommand(OrderBook& book, OrderId& nextId, const std::string& line) {
    std::istringstream in(line);
    std::vector<std::string> tok;
    for (std::string w; in >> w;) {
        if (w == "@") continue;
        std::transform(w.begin(), w.end(), w.begin(), [](unsigned char c) { return std::tolower(c); });
        tok.push_back(w);
    }
    if (tok.empty()) return;

    try {
        const std::string& cmd = tok[0];
        if ((cmd == "buy" || cmd == "sell") && (tok.size() == 3 || tok.size() == 4)) {
            Side side = cmd == "buy" ? Side::Buy : Side::Sell;
            Quantity qty = parseNumber(tok[1]);
            Price price = static_cast<Price>(parseNumber(tok[2]));
            TimeInForce tif = TimeInForce::GTC;
            if (tok.size() == 4) {
                if (tok[3] == "ioc") tif = TimeInForce::IOC;
                else if (tok[3] == "fok") tif = TimeInForce::FOK;
                else if (tok[3] != "gtc") throw std::invalid_argument("unknown type '" + tok[3] + "'");
            }
            OrderId id = nextId++;
            logEvent("#" + std::to_string(id) + " " + line);
            auto trades = book.addLimitOrder(id, side, price, qty, tif);
            logTrades(trades);
            if (book.hasOrder(id)) logEvent("  #" + std::to_string(id) + " resting on the book");
            else if (trades.empty()) logEvent("  #" + std::to_string(id) + " not filled, cancelled");
        } else if (cmd == "market" && tok.size() == 3 && (tok[1] == "buy" || tok[1] == "sell")) {
            OrderId id = nextId++;
            logEvent("#" + std::to_string(id) + " " + line);
            auto trades = book.addMarketOrder(id, tok[1] == "buy" ? Side::Buy : Side::Sell, parseNumber(tok[2]));
            logTrades(trades);
            if (trades.empty()) logEvent("  nothing to match against");
        } else if (cmd == "cancel" && tok.size() == 2) {
            OrderId id = parseNumber(tok[1]);
            logEvent(book.cancelOrder(id) ? "cancelled #" + std::to_string(id)
                                          : "cancel: no resting order #" + std::to_string(id));
        } else if (cmd == "modify" && tok.size() == 4) {
            OrderId id = parseNumber(tok[1]);
            if (!book.hasOrder(id)) throw std::invalid_argument("no resting order #" + std::to_string(id));
            logEvent("modify #" + std::to_string(id) + " -> " + tok[2] + " @ " + tok[3]);
            logTrades(book.modifyOrder(id, static_cast<Price>(parseNumber(tok[3])), parseNumber(tok[2])));
        } else {
            throw std::invalid_argument("unrecognised command");
        }
    } catch (const std::exception& e) {
        logEvent("error: '" + line + "': " + e.what());
    }
}

void draw(const OrderBook& book) {
    std::ostringstream out;
    out << "\033[2J\033[H"; // clear screen, cursor to top-left
    out << "LIVE LIMIT ORDER BOOK\n\n";
    out << "        PRICE       QTY   ORDERS\n";

    auto asks = book.depth(Side::Sell, 10);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        out << "  ASK " << std::setw(7) << it->price << std::setw(10) << it->totalQuantity
            << std::setw(9) << it->orderCount << "\n";
    }
    if (book.bestBid() && book.bestAsk()) {
        out << "  ------- spread: " << (*book.bestAsk() - *book.bestBid()) << " -------\n";
    } else {
        out << "  ------------------------------\n";
    }
    for (const auto& level : book.depth(Side::Buy, 10)) {
        out << "  BID " << std::setw(7) << level.price << std::setw(10) << level.totalQuantity
            << std::setw(9) << level.orderCount << "\n";
    }

    out << "\nRecent activity:\n";
    if (events.empty()) out << "  (none yet: run './build/lob_live order' in another terminal)\n";
    for (const auto& e : events) out << "  " << e << "\n";
    out << "\nCommands: " << kHelp << "\n";
    std::cout << out.str() << std::flush;
}

int runBook() {
    unlink(kPipePath); // remove anything stale from a previous run
    if (mkfifo(kPipePath, 0600) != 0) {
        std::perror("mkfifo");
        return 1;
    }

    OrderBook book;
    OrderId nextId = 1;
    draw(book);

    // Open the read end right away (non-blocking) so order terminals can connect,
    // and hold our own write end so read() never hits end-of-file when an order
    // terminal disconnects. Then switch back to blocking reads.
    int fd = open(kPipePath, O_RDONLY | O_NONBLOCK);
    int keepAlive = open(kPipePath, O_WRONLY);
    if (fd < 0 || keepAlive < 0) {
        std::perror("open");
        return 1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

    // Raw read() (not std::ifstream, which waits for a full buffer) so each
    // order is shown the moment it arrives.
    std::string pending;
    char buf[512];
    for (ssize_t n; (n = read(fd, buf, sizeof buf)) > 0;) {
        pending.append(buf, static_cast<std::size_t>(n));
        for (std::size_t nl; (nl = pending.find('\n')) != std::string::npos;) {
            handleCommand(book, nextId, pending.substr(0, nl));
            pending.erase(0, nl + 1);
            draw(book);
        }
    }
    std::perror("read");
    return 1;
}

// ------------------------------------------------------------- order terminal

int runOrderEntry() {
    std::signal(SIGPIPE, SIG_IGN); // report a closed book window instead of dying silently

    // Non-blocking open fails immediately if the book window isn't running.
    int fd = open(kPipePath, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Order book is not running. Start './build/lob_live' in another terminal first.\n";
        return 1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

    std::cout << "Connected to the live order book. Type orders and press Enter (Ctrl+D to quit).\n"
              << "  " << kHelp << "\n";
    for (std::string line; std::cout << "> " << std::flush, std::getline(std::cin, line);) {
        if (line.find_first_not_of(" \t") == std::string::npos) continue;
        line += "\n";
        if (write(fd, line.data(), line.size()) < 0) {
            std::cerr << "Order book window was closed.\n";
            return 1;
        }
    }
    close(fd);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "order") return runOrderEntry();
    if (argc == 1) return runBook();
    std::cerr << "usage: " << argv[0] << "          show the live order book\n"
              << "       " << argv[0] << " order    place orders (run in a second terminal)\n";
    return 1;
}
