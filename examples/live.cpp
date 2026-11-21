// Live order book in two terminals:
//   ./build/lob_live          shows the book
//   ./build/lob_live order    type orders here
// Orders are sent through a named pipe.

#include "lob/OrderBook.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <bits/stdc++.h>

using namespace std;

using namespace lob;

namespace {

const char* kPipePath = "/tmp/lob_orders.fifo";
const char* kHelp =
    "buy 10 @ 100 [ioc|fok]   sell 5 @ 102   market buy 3   cancel 2   modify 2 8 @ 101";

// ---- book viewer

deque<string> events; // recent activity

void logEvent(const string& msg) {
    events.push_back(msg);
    if (events.size() > 12) events.pop_front();
}

uint64_t parseNumber(const string& s) {
    if (s.empty() || !all_of(s.begin(), s.end(), [](unsigned char c) { return isdigit(c); })) {
        throw invalid_argument("expected a positive number, got '" + s + "'");
    }
    return stoull(s);
}

void logTrades(const vector<Trade>& trades) {
    for (const Trade& t : trades) {
        ostringstream os;
        os << "  TRADE " << t.quantity << " @ " << t.price << "  (#" << t.takerOrderId
           << (t.takerSide == Side::Buy ? " bought from #" : " sold to #") << t.makerOrderId << ")";
        logEvent(os.str());
    }
}

// e.g. "buy 10 @ 100 ioc"
void handleCommand(OrderBook& book, OrderId& nextId, const string& line) {
    istringstream in(line);
    vector<string> tok;
    for (string w; in >> w;) {
        if (w == "@") continue;
        transform(w.begin(), w.end(), w.begin(), [](unsigned char c) { return tolower(c); });
        tok.push_back(w);
    }
    if (tok.empty()) return;

    try {
        const string& cmd = tok[0];
        if ((cmd == "buy" || cmd == "sell") && (tok.size() == 3 || tok.size() == 4)) {
            Side side = cmd == "buy" ? Side::Buy : Side::Sell;
            Quantity qty = parseNumber(tok[1]);
            Price price = static_cast<Price>(parseNumber(tok[2]));
            TimeInForce tif = TimeInForce::GTC;
            if (tok.size() == 4) {
                if (tok[3] == "ioc") tif = TimeInForce::IOC;
                else if (tok[3] == "fok") tif = TimeInForce::FOK;
                else if (tok[3] != "gtc") throw invalid_argument("unknown type '" + tok[3] + "'");
            }
            OrderId id = nextId++;
            logEvent("#" + to_string(id) + " " + line);
            auto trades = book.addLimitOrder(id, side, price, qty, tif);
            logTrades(trades);
            if (book.hasOrder(id)) logEvent("  #" + to_string(id) + " resting on the book");
            else if (trades.empty()) logEvent("  #" + to_string(id) + " not filled, cancelled");
        } else if (cmd == "market" && tok.size() == 3 && (tok[1] == "buy" || tok[1] == "sell")) {
            OrderId id = nextId++;
            logEvent("#" + to_string(id) + " " + line);
            auto trades = book.addMarketOrder(id, tok[1] == "buy" ? Side::Buy : Side::Sell, parseNumber(tok[2]));
            logTrades(trades);
            if (trades.empty()) logEvent("  nothing to match against");
        } else if (cmd == "cancel" && tok.size() == 2) {
            OrderId id = parseNumber(tok[1]);
            logEvent(book.cancelOrder(id) ? "cancelled #" + to_string(id)
                                          : "cancel: no resting order #" + to_string(id));
        } else if (cmd == "modify" && tok.size() == 4) {
            OrderId id = parseNumber(tok[1]);
            if (!book.hasOrder(id)) throw invalid_argument("no resting order #" + to_string(id));
            logEvent("modify #" + to_string(id) + " -> " + tok[2] + " @ " + tok[3]);
            logTrades(book.modifyOrder(id, static_cast<Price>(parseNumber(tok[3])), parseNumber(tok[2])));
        } else {
            throw invalid_argument("unrecognised command");
        }
    } catch (const exception& e) {
        logEvent("error: '" + line + "': " + e.what());
    }
}

void draw(const OrderBook& book) {
    ostringstream out;
    out << "\033[2J\033[H"; // clear screen
    out << "LIVE LIMIT ORDER BOOK\n\n";
    out << "        PRICE       QTY   ORDERS\n";

    auto asks = book.depth(Side::Sell, 10);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        out << "  ASK " << setw(7) << it->price << setw(10) << it->totalQuantity
            << setw(9) << it->orderCount << "\n";
    }
    if (book.bestBid() && book.bestAsk()) {
        out << "  ------- spread: " << (*book.bestAsk() - *book.bestBid()) << " -------\n";
    } else {
        out << "  ------------------------------\n";
    }
    for (const auto& level : book.depth(Side::Buy, 10)) {
        out << "  BID " << setw(7) << level.price << setw(10) << level.totalQuantity
            << setw(9) << level.orderCount << "\n";
    }

    out << "\nRecent activity:\n";
    if (events.empty()) out << "  (none yet: run './build/lob_live order' in another terminal)\n";
    for (const auto& e : events) out << "  " << e << "\n";
    out << "\nCommands: " << kHelp << "\n";
    cout << out.str() << flush;
}

int runBook() {
    unlink(kPipePath); // remove old pipe
    if (mkfifo(kPipePath, 0600) != 0) {
        perror("mkfifo");
        return 1;
    }

    OrderBook book;
    OrderId nextId = 1;
    draw(book);

    // Keep a write end open so read() never sees end-of-file.
    int fd = open(kPipePath, O_RDONLY | O_NONBLOCK);
    int keepAlive = open(kPipePath, O_WRONLY);
    if (fd < 0 || keepAlive < 0) {
        perror("open");
        return 1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

    // Raw read() so each line shows up immediately.
    string pending;
    char buf[512];
    for (ssize_t n; (n = read(fd, buf, sizeof buf)) > 0;) {
        pending.append(buf, static_cast<size_t>(n));
        for (size_t nl; (nl = pending.find('\n')) != string::npos;) {
            handleCommand(book, nextId, pending.substr(0, nl));
            pending.erase(0, nl + 1);
            draw(book);
        }
    }
    perror("read");
    return 1;
}

// ---- order terminal

int runOrderEntry() {
    signal(SIGPIPE, SIG_IGN); // handle a closed book window ourselves

    // Fails right away if the book is not running.
    int fd = open(kPipePath, O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        cerr << "Order book is not running. Start './build/lob_live' in another terminal first.\n";
        return 1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);

    cout << "Connected to the live order book. Type orders and press Enter (Ctrl+D to quit).\n"
              << "  " << kHelp << "\n";
    for (string line; cout << "> " << flush, getline(cin, line);) {
        if (line.find_first_not_of(" \t") == string::npos) continue;
        line += "\n";
        if (write(fd, line.data(), line.size()) < 0) {
            cerr << "Order book window was closed.\n";
            return 1;
        }
    }
    close(fd);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && string(argv[1]) == "order") return runOrderEntry();
    if (argc == 1) return runBook();
    cerr << "usage: " << argv[0] << "          show the live order book\n"
              << "       " << argv[0] << " order    place orders (run in a second terminal)\n";
    return 1;
}
