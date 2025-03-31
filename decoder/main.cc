#include <cstdio>
#include "decoder.h"

/** @brief Program working mode enumeration */

enum ProgramMode {
    STANDARD_MODE         = 0,
    READ_FROM_BINARY_FILE = 1,
    SAVE_TO_BINARY_FILE   = 2,
    RX_PACKED             = 4,
};

/** @brief Interrupt flag */

static volatile int gSigintFlag = 0;

/**
 * @brief Handle SIGINT to clean up
 *
 */

static void sigint_handler(int val)
{
    gSigintFlag = 1;
}

/**
 * @brief Decoder program entry point
 *
 * Reads demodulated values from UDP port 42000 coming from physical demodulator
 * Writes decoded frames to UDP port 42100 to tetra interpreter
 *
 * Filtering log for SDS: sed -n '/SDS/ p' log.txt > out.txt
 *
 */

int main(int argc, char * argv[])
{
    // connect interrupt Ctrl-C handler
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, 0);

    int udpPortRx = 42000;                                                      // UDP RX port (ie. where to receive bits from PHY layer)
    int udpPortTx = 42100;                                                      // UDP TX port (ie. where to send Json data)
    int selfCarrierNum = 0;

    const int FILENAME_LEN = 256;
    char optFilenameInput[FILENAME_LEN]  = "";                                  // input bits filename
    char optFilenameOutput[FILENAME_LEN] = "";                                  // output bits filename

    int programMode = STANDARD_MODE;
    int debugLevel = 1;
    bool bRemoveFillBits = true;
    bool bEnableWiresharkOutput = false;

    int option;
    while ((option = getopt(argc, argv, "hPwr:t:i:o:d:c:f")) != -1)
    {
        switch (option)
        {
        case 'r':
            udpPortRx = atoi(optarg);
            break;

        case 't':
            udpPortTx = atoi(optarg);
            break;

        case 'P':
            programMode |= RX_PACKED;
            break;
        case 'i':
            strncpy(optFilenameInput, optarg, FILENAME_LEN - 1);
            programMode |= READ_FROM_BINARY_FILE;
            break;

        case 'o':
            strncpy(optFilenameOutput, optarg, FILENAME_LEN - 1);
            programMode |= SAVE_TO_BINARY_FILE;
            break;

        case 'd':
            debugLevel = atoi(optarg);
            break;

        case 'c':
            selfCarrierNum = atoi(optarg);
            break;

        case 'f':
            bRemoveFillBits = false;
            break;

        case 'w':
            bEnableWiresharkOutput = true;
            break;
        
        case 'h':
            printf("\nUsage: ./decoder [OPTIONS]\n\n"
                   "Options:\n"
                   "  -r <UDP socket> receiving from phy [default port is 42000]\n"
                   "  -t <UDP socket> sending Json data [default port is 42100]\n"
                   "  -i <file> replay data from binary file instead of UDP\n"
                   "  -o <file> record data to binary file (can be replayed with -i option)\n"
                   "  -d <level> print debug information\n"
                   "  -f keep fill bits\n"
                   "  -w enable wireshark output [EXPERIMENTAL]\n"
                   "  -P pack rx data (1 byte = 8 bits)\n"
                   "  -c <carrier num> send carrier number flag to recorder, used\n"
                   "  -h print this help\n\n");
            exit(EXIT_FAILURE);
            break;

        case '?':
            printf("unkown option, run ./decoder -h to list available options\n");
            exit(EXIT_FAILURE);
            break;
        }
    }


    // create output destination socket
    struct sockaddr_in addr_output;
    memset(&addr_output, 0, sizeof(struct sockaddr_in));
    addr_output.sin_family = AF_INET;
    addr_output.sin_port = htons(udpPortTx);
    inet_aton("127.0.0.1", &addr_output.sin_addr);

    int udpSocketFd  = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    connect(udpSocketFd, (struct sockaddr *) & addr_output, sizeof(struct sockaddr));
    printf("Output socket 0x%04x on port %d\n", udpSocketFd, udpPortTx);
    if (udpSocketFd < 0)
    {
        perror("Couldn't create output socket");
        exit(EXIT_FAILURE);
    }

    // create decoder
    Tetra::LogLevel logLevel;
    switch (debugLevel)
    {
    case 0:
        logLevel = Tetra::LogLevel::NONE;
        break;
    case 1:
        logLevel = Tetra::LogLevel::LOW;
        break;
    case 2:
        logLevel = Tetra::LogLevel::MEDIUM;
        break;
    case 3:
        logLevel = Tetra::LogLevel::HIGH;
        break;
    case 4:
        logLevel = Tetra::LogLevel::VERYHIGH;
        break;
    default:
        logLevel = Tetra::LogLevel::LOW;

    }

    // output file if any
    int fdOutputSaveFile = 0;

    if (programMode & SAVE_TO_BINARY_FILE)
    {
        // save input bits to file
        fdOutputSaveFile = open(optFilenameOutput, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
        if (fdOutputSaveFile < 0)
        {
            fprintf(stderr, "Couldn't open output file");
            exit(EXIT_FAILURE);
        }
    }

    // input source
    int fdInput = 0;

    if (programMode & READ_FROM_BINARY_FILE)
    {
        // read input bits from file
        fdInput = open(optFilenameInput, O_RDONLY);

        printf("Input from file '%s' 0x%04x\n", optFilenameInput, fdInput);

        if (fdInput < 0)
        {
            fprintf(stderr, "Couldn't open input bits file");
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        // read input bits from UDP socket
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(struct sockaddr_in));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(udpPortRx);
        inet_aton("127.0.0.1", &addr.sin_addr);

        fdInput = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        bind(fdInput, (struct sockaddr *)&addr, sizeof(struct sockaddr));

        printf("Input socket 0x%04x on port %d\n", fdInput, udpPortRx);

        if (fdInput < 0)
        {
            fprintf(stderr, "Couldn't create input socket");
            exit(EXIT_FAILURE);
        }
    }

    // create decoder
    Tetra::TetraDecoder * decoder = new Tetra::TetraDecoder(udpSocketFd, bRemoveFillBits, logLevel, bEnableWiresharkOutput, selfCarrierNum);

    // receive buffer
    const int RXBUF_LEN = 2048;
    uint8_t rxBuf[RXBUF_LEN];

    while (!gSigintFlag)
    {
        int bytesRead = read(fdInput, rxBuf, sizeof(rxBuf));

        if (errno == EINTR)
        {
            // print is required for ^C to be handled
            fprintf(stderr, "EINTR\n");
            break;
        }
        else if (bytesRead < 0)
        {
            fprintf(stderr, "Read error\n");
            break;
        }
        else if (bytesRead == 0)
        {
            break;
        }

        if (programMode & SAVE_TO_BINARY_FILE)
        {
            write(fdOutputSaveFile, rxBuf, bytesRead);
        }

        // bytes must be pushed one at a time into decoder
        for (int cnt = 0; cnt < bytesRead; cnt++)
        {
        	if (programMode & RX_PACKED)
        	{
        		for (uint8_t idx = 0; idx <= 7; idx++)
        	    {
        			decoder->rxSymbol((rxBuf[cnt] >> idx) & 0x01);
        		}
        	}
        	else
        	{
        		decoder->rxSymbol(rxBuf[cnt]);
        	}
        }
    }

    close(udpSocketFd);

    // file or socket must be closed
    close(fdInput);

    // close save file only if openede
    if (programMode & SAVE_TO_BINARY_FILE)
    {
        close(fdOutputSaveFile);
    }

    delete decoder;

    printf("Clean exit\n");

    return EXIT_SUCCESS;
}
