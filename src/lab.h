#ifndef LAB_H
#define LAB_H

/*
 * Reliable file transfer over UDP with Go-Back-N.
 *
 * The program is split into three layers:
 *
 *   1. Packets: checksum, encode, decode/validate. Pure functions.
 *   2. Go-Back-N state machines: a sender and a receiver. They are fed
 *      events (packet arrived, timer expired) together with the current time
 *      in milliseconds and answer with what to do next. They never touch a
 *      socket, a clock, or a file.
 *   3. I/O: the socket, the relay hello, the poll loop, the clock, and the
 *      file. That layer lives in main.c and is the only code that knows any
 *      of them exist. The pure helpers it needs (argument parsing, building
 *      the hello, reading the relay's reply) are declared at the end of this
 *      header so they can be unit tested.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* ------------------------------------------------------------------------ */
/* Layer 1: packets                                                         */
/* ------------------------------------------------------------------------ */

#define RDT_HEADER_LEN 10u
#define RDT_MAX_PAYLOAD 1024u
#define RDT_MAX_PACKET (RDT_HEADER_LEN + RDT_MAX_PAYLOAD)

typedef enum
{
  RDT_DATA = 0,
  RDT_ACK = 1,
  RDT_FIN = 2
} rdt_type;

/** A decoded packet. The payload is copied in so the struct owns it. */
typedef struct
{
  rdt_type type;
  uint32_t seq;
  uint16_t length;
  uint8_t payload[RDT_MAX_PAYLOAD];
} rdt_packet;

/**
 * @brief RFC 1071 Internet checksum.
 *
 * Returns the one's complement of the one's complement sum of @p buf taken as
 * big-endian 16-bit words. An odd length is padded with one zero byte for the
 * calculation only. Summing a packet that already holds a correct checksum
 * yields 0xffff, so this function returns 0 for such a packet.
 *
 * @param buf bytes to sum (may be NULL when len is 0)
 * @param len number of bytes
 * @return the checksum in host byte order
 */
uint16_t rdt_checksum(const uint8_t *buf, size_t len);

/**
 * @brief Encode a packet into wire format, filling in the checksum.
 *
 * @param pkt the packet; pkt->length bytes of pkt->payload are written
 * @param buf output buffer
 * @param cap capacity of @p buf
 * @return the number of bytes written, or 0 if the packet is invalid (bad
 *         type, length over RDT_MAX_PAYLOAD) or does not fit in @p cap
 */
size_t rdt_encode(const rdt_packet *pkt, uint8_t *buf, size_t cap);

/**
 * @brief Validate a received datagram and decode it.
 *
 * The datagram is rejected unless it is at least RDT_HEADER_LEN bytes, its
 * length field matches its size and is at most RDT_MAX_PAYLOAD, its type is
 * DATA, ACK or FIN, its reserved byte is 0, and its checksum verifies.
 *
 * @param buf the datagram
 * @param len its size, as returned by recvfrom
 * @param out filled in only when the datagram is valid
 * @return true if the datagram is a valid packet
 */
bool rdt_decode(const uint8_t *buf, size_t len, rdt_packet *out);

/**
 * @brief Build an ACK packet.
 * @param pkt output
 * @param seq index of the next packet the receiver expects
 */
void rdt_make_ack(rdt_packet *pkt, uint32_t seq);

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N state machines                                        */
/* ------------------------------------------------------------------------ */

#define GBN_MAX_WINDOW 64u
#define GBN_MAX_TIMEOUTS 10
#define GBN_LINGER_MS 2000u
#define GBN_IDLE_MS 30000u

typedef enum
{
  GBN_RUNNING = 0, /* keep going */
  GBN_DONE,        /* transfer finished successfully */
  GBN_FAILED       /* gave up */
} gbn_status;

/** What the sender wants done after an event. */
typedef struct
{
  /** Send packets send_from .. send_to - 1, in order (empty if equal). */
  uint32_t send_from;
  uint32_t send_to;
  /** Whether the timer is running, and when it expires. */
  bool timer_running;
  uint64_t deadline_ms;
  gbn_status status;
} gbn_sender_action;

/**
 * Go-Back-N sender. The whole file is held in memory (files are at most
 * 16 MiB), so the copy of every unacknowledged packet is simply the file
 * itself: any packet can be rebuilt with gbn_sender_packet.
 */
typedef struct
{
  const uint8_t *data;
  size_t size;
  uint32_t data_packets; /* number of DATA packets; the FIN's seq */
  uint32_t window;
  uint64_t timeout_ms;
  uint32_t base; /* oldest unacknowledged packet */
  uint32_t next; /* next packet never sent */
  bool timer_running;
  uint64_t deadline_ms;
  int timeouts; /* consecutive timeouts without progress */
  gbn_status status;
} gbn_sender;

/**
 * @brief Initialise a sender for a file held in memory.
 * @param s the sender
 * @param data file contents (may be NULL when size is 0)
 * @param size file size in bytes
 * @param window window size in packets, 1 to GBN_MAX_WINDOW
 * @param timeout_ms retransmission timeout
 * @return true on success, false on a bad argument
 */
bool gbn_sender_init(gbn_sender *s, const uint8_t *data, size_t size,
                     uint32_t window, uint64_t timeout_ms);

/**
 * @brief Start the transfer: fill the window.
 * @param s the sender
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_sender_start(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act);

/**
 * @brief Feed the sender an ACK.
 *
 * An ACK with seq > base slides the window to seq; anything else is a
 * duplicate and is ignored. An ACK for a packet never sent is ignored too.
 *
 * @param s the sender
 * @param ack_seq the ACK's seq field
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_sender_on_ack(gbn_sender *s, uint32_t ack_seq, uint64_t now_ms,
                       gbn_sender_action *act);

/**
 * @brief Tell the sender time has passed.
 *
 * If the timer is running and has expired, this is a timeout: everything
 * from base to next - 1 is resent and the timer restarts, unless this is the
 * GBN_MAX_TIMEOUTS-th timeout in a row, in which case the sender gives up.
 * Otherwise nothing happens.
 *
 * @param s the sender
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_sender_on_tick(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act);

/**
 * @brief Build packet @p seq: DATA for seq < data_packets, FIN at
 *        data_packets.
 * @param s the sender
 * @param seq packet index
 * @param pkt output
 * @return false if seq is past the FIN
 */
bool gbn_sender_packet(const gbn_sender *s, uint32_t seq, rdt_packet *pkt);

/** What the receiver wants done after an event. */
typedef struct
{
  /** Send an ACK carrying ack_seq. */
  bool send_ack;
  uint32_t ack_seq;
  /** Append these bytes to the file (deliver_len may be 0). Points into the
   *  packet passed in, so it is valid as long as that packet is. */
  const uint8_t *deliver;
  size_t deliver_len;
  /** The FIN arrived just now: the file is complete and can be closed. */
  bool fin;
} gbn_receiver_action;

/** Go-Back-N receiver. */
typedef struct
{
  uint32_t expected;        /* next packet wanted */
  bool finished;            /* FIN received */
  uint64_t last_activity_ms;/* last valid packet, or start */
  uint64_t linger_until_ms; /* when finished: when to exit */
} gbn_receiver;

/**
 * @brief Initialise a receiver.
 * @param r the receiver
 * @param now_ms current time, which starts the idle timer
 */
void gbn_receiver_init(gbn_receiver *r, uint64_t now_ms);

/**
 * @brief Feed the receiver a valid packet.
 *
 * In-order DATA is delivered and acknowledged; any other DATA is discarded
 * and the current ACK repeated. The FIN at the expected seq completes the
 * file and starts the linger; a repeated FIN is answered with the same ACK.
 * ACK packets are ignored.
 *
 * @param r the receiver
 * @param pkt a packet that passed rdt_decode
 * @param now_ms current time
 * @param act what to do next
 */
void gbn_receiver_on_packet(gbn_receiver *r, const rdt_packet *pkt,
                            uint64_t now_ms, gbn_receiver_action *act);

/**
 * @brief When the receiver next needs to be woken up.
 * @param r the receiver
 * @return the end of the linger if finished, else the idle deadline
 */
uint64_t gbn_receiver_deadline(const gbn_receiver *r);

/**
 * @brief Tell the receiver time has passed.
 * @param r the receiver
 * @param now_ms current time
 * @return GBN_DONE once the linger is over, GBN_FAILED after GBN_IDLE_MS
 *         with nothing valid arriving, else GBN_RUNNING
 */
gbn_status gbn_receiver_on_tick(const gbn_receiver *r, uint64_t now_ms);

/* ------------------------------------------------------------------------ */
/* Layer 3 helpers: pure pieces of the I/O layer                            */
/* ------------------------------------------------------------------------ */

#define APP_EXIT_OK 0
#define APP_EXIT_USAGE 1
#define APP_EXIT_FAIL 2

#define APP_SESSION_MAX 32u
#define APP_HELLO_MAX 128u

typedef enum
{
  APP_SEND,
  APP_RECV
} app_mode;

typedef struct
{
  app_mode mode;
  const char *session;
  uint32_t window;
  uint32_t timeout_ms;
  double loss;
  double corrupt;
  double dup;
  const char *port; /* decimal string, for getaddrinfo */
  const char *relay;
  const char *file;
} app_options;

/**
 * @brief Print the usage message.
 * @param out where to print it
 */
void app_usage(FILE *out);

/**
 * @brief Parse the command line. argv[1] is the mode, the rest is parsed
 *        with getopt. Error messages go to @p err.
 * @param argc argument count
 * @param argv argument vector (getopt may permute it)
 * @param opts output, filled with defaults first
 * @param err where to report problems
 * @return 0 if the options are valid, APP_EXIT_USAGE if not
 */
int app_parse_args(int argc, char *argv[], app_options *opts, FILE *err);

/**
 * @brief Check a session name: 1 to 32 characters from a-z, 0-9 and '-'.
 * @param session the name
 * @return true if valid
 */
bool app_valid_session(const char *session);

/**
 * @brief Build the hello datagram (no newline, no terminator on the wire).
 * @param opts parsed options
 * @param buf output; NUL terminated
 * @param cap capacity of @p buf
 * @return the number of bytes to send, or 0 if it does not fit
 */
size_t app_format_hello(const app_options *opts, char *buf, size_t cap);

/**
 * @brief Interpret the relay's reply to a hello.
 * @param buf the datagram (not NUL terminated)
 * @param len its size
 * @param reason output for the text after "ERR " (NUL terminated)
 * @param cap capacity of @p reason
 * @return 0 for OK, 1 for ERR, -1 for anything else
 */
int app_parse_reply(const uint8_t *buf, size_t len, char *reason, size_t cap);

#endif // LAB_H
