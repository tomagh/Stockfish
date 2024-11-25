
#include <unistd.h>
#include <signal.h>

#include "uci.h"

using namespace Stockfish;


// Ignore SIGPIPE signal
void ignore_sigpipe() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGPIPE, &sa, NULL);
}


// Custom stream buffer for file descriptors
class fd_streambuf : public std::streambuf {
public:
    fd_streambuf(int fd) : fd_(fd) {
        setg(buffer_, buffer_ + sizeof(buffer_), buffer_ + sizeof(buffer_));
        setp(buffer_, buffer_ + sizeof(buffer_) - 1);
    }
    
protected:
    int_type underflow() override {
        if (gptr() < egptr()) {
            return traits_type::to_int_type(*gptr());
        }

        ssize_t n = read(fd_, buffer_, sizeof(buffer_));
        if (n <= 0) {
            return traits_type::eof();
        }

        setg(buffer_, buffer_, buffer_ + n);
        return traits_type::to_int_type(*gptr());
    }

    int_type overflow(int_type ch) override {
        if (pptr() == epptr()) {
            if (sync() == traits_type::eof()) {
                return traits_type::eof();
            }
        }

        if (ch != traits_type::eof()) {
            *pptr() = ch;
            pbump(1);
        }

        return ch;
    }

    int sync() override {
        ssize_t n = pptr() - pbase();
        if (write(fd_, pbase(), n) != n) {
            return traits_type::eof();
        }

        pbump(int(-n));
        return 0;
    }

private:
    int fd_;
    char buffer_[4096];
};


using namespace Stockfish;

extern "C" void stockfish_setup_io(FILE *input, FILE *output) {
    if (!input || !output) {
        std::cerr << "Invalid FILE pointers received" << std::endl;
        return;
    }
    
    int inputFd = fileno(input);
    int outputFd = fileno(output);

    ignore_sigpipe(); // TODO: remove

    static fd_streambuf inputBuf(inputFd);
    static fd_streambuf outputBuf(outputFd);

    static std::istream inputStream(&inputBuf);
    static std::ostream outputStream(&outputBuf);
    
    // Call the setup_io function
    setup_global_io(inputStream, outputStream);
}


extern "C" void stockfish_init(FILE *input, FILE *output) {
    stockfish_setup_io(input, output);
        
    sync_cout << engine_info() << sync_endl;

    Bitboards::init();
    Position::init();

    int argc = 1;
    char* argv[] = { (char*)"" };
    UCIEngine uci(argc, argv);

    Tune::init(uci.engine_options());

    uci.loop();
}


