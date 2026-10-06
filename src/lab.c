#include "lab.h"

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Layer 1: packets                                                         */
/* ------------------------------------------------------------------------ */

uint16_t rdt_checksum(const uint8_t *buf, size_t len)
{
  uint32_t sum = 0;
  size_t i = 0;
  for (; i + 1 < len; i += 2)
  {
    sum += (uint32_t)((uint32_t)buf[i] << 8 | buf[i + 1]);
  }
  if (i < len)
  {
    sum += (uint32_t)buf[i] << 8; /* pad the odd byte with a zero */
  }
  while (sum >> 16)
  {
    sum = (sum & 0xffffu) + (sum >> 16);
  }
  return (uint16_t)~sum;
}

size_t rdt_encode(const rdt_packet *pkt, uint8_t *buf, size_t cap)
{
  if (pkt == NULL || buf == NULL)
  {
    return 0;
  }
  if (pkt->type != RDT_DATA && pkt->type != RDT_ACK && pkt->type != RDT_FIN)
  {
    return 0;
  }
  if (pkt->length > RDT_MAX_PAYLOAD)
  {
    return 0;
  }
  size_t total = RDT_HEADER_LEN + pkt->length;
  if (total > cap)
  {
    return 0;
  }

  uint16_t zero = 0;
  uint32_t seq = htonl(pkt->seq);
  uint16_t length = htons(pkt->length);
  buf[0] = (uint8_t)pkt->type;
  buf[1] = 0;
  memcpy(buf + 2, &zero, sizeof zero);
  memcpy(buf + 4, &seq, sizeof seq);
  memcpy(buf + 8, &length, sizeof length);
  if (pkt->length > 0)
  {
    memcpy(buf + RDT_HEADER_LEN, pkt->payload, pkt->length);
  }

  uint16_t sum = htons(rdt_checksum(buf, total));
  memcpy(buf + 2, &sum, sizeof sum);
  return total;
}

bool rdt_decode(const uint8_t *buf, size_t len, rdt_packet *out)
{
  if (buf == NULL || out == NULL || len < RDT_HEADER_LEN)
  {
    return false;
  }

  uint16_t length;
  memcpy(&length, buf + 8, sizeof length);
  length = ntohs(length);
  /* Check the length field against what actually arrived before trusting
   * it for anything. */
  if (length > RDT_MAX_PAYLOAD || RDT_HEADER_LEN + length != len)
  {
    return false;
  }
  if (buf[0] > RDT_FIN || buf[1] != 0)
  {
    return false;
  }
  if (rdt_checksum(buf, len) != 0)
  {
    return false;
  }

  uint32_t seq;
  memcpy(&seq, buf + 4, sizeof seq);
  out->type = (rdt_type)buf[0];
  out->seq = ntohl(seq);
  out->length = length;
  if (length > 0)
  {
    memcpy(out->payload, buf + RDT_HEADER_LEN, length);
  }
  return true;
}

void rdt_make_ack(rdt_packet *pkt, uint32_t seq)
{
  pkt->type = RDT_ACK;
  pkt->seq = seq;
  pkt->length = 0;
}

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N sender                                                */
/* ------------------------------------------------------------------------ */

static void sender_report(const gbn_sender *s, uint32_t from, uint32_t to,
                          gbn_sender_action *act)
{
  act->send_from = from;
  act->send_to = to;
  act->timer_running = s->timer_running;
  act->deadline_ms = s->deadline_ms;
  act->status = s->status;
}

/* Send as much as the window allows. The FIN goes only once every DATA
 * packet is acknowledged. Returns the first packet newly sent; the caller
 * sends from there up to s->next. */
static uint32_t sender_fill(gbn_sender *s, uint64_t now_ms)
{
  uint32_t first = s->next;
  while (s->next < s->data_packets &&
         (uint64_t)s->next < (uint64_t)s->base + s->window)
  {
    s->next++;
  }
  if (s->next == s->data_packets && s->base == s->data_packets)
  {
    s->next++; /* the FIN */
  }
  if (s->next > first && !s->timer_running)
  {
    s->timer_running = true;
    s->deadline_ms = now_ms + s->timeout_ms;
  }
  return first;
}

bool gbn_sender_init(gbn_sender *s, const uint8_t *data, size_t size,
                     uint32_t window, uint64_t timeout_ms)
{
  if (s == NULL || (data == NULL && size > 0) || window < 1 ||
      window > GBN_MAX_WINDOW || timeout_ms == 0)
  {
    return false;
  }
  size_t packets = (size + RDT_MAX_PAYLOAD - 1) / RDT_MAX_PAYLOAD;
  if (packets >= UINT32_MAX)
  {
    return false; // GCOVR_EXCL_LINE: would need a 4 TiB file
  }
  memset(s, 0, sizeof *s);
  s->data = data;
  s->size = size;
  s->data_packets = (uint32_t)packets;
  s->window = window;
  s->timeout_ms = timeout_ms;
  s->status = GBN_RUNNING;
  return true;
}

void gbn_sender_start(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act)
{
  uint32_t first = sender_fill(s, now_ms);
  sender_report(s, first, s->next, act);
}

void gbn_sender_on_ack(gbn_sender *s, uint32_t ack_seq, uint64_t now_ms,
                       gbn_sender_action *act)
{
  if (s->status != GBN_RUNNING || ack_seq <= s->base || ack_seq > s->next)
  {
    /* duplicate, finished, or an ACK for something never sent */
    sender_report(s, s->next, s->next, act);
    return;
  }

  s->base = ack_seq;
  s->timeouts = 0;
  if (s->base == s->data_packets + 1)
  {
    /* the FIN is acknowledged */
    s->status = GBN_DONE;
    s->timer_running = false;
    sender_report(s, s->next, s->next, act);
    return;
  }

  if (s->base < s->next)
  {
    s->timer_running = true;
    s->deadline_ms = now_ms + s->timeout_ms;
  }
  else
  {
    s->timer_running = false;
  }
  uint32_t first = sender_fill(s, now_ms);
  sender_report(s, first, s->next, act);
}

void gbn_sender_on_tick(gbn_sender *s, uint64_t now_ms, gbn_sender_action *act)
{
  if (s->status != GBN_RUNNING || !s->timer_running || now_ms < s->deadline_ms)
  {
    sender_report(s, s->next, s->next, act);
    return;
  }

  s->timeouts++;
  if (s->timeouts >= GBN_MAX_TIMEOUTS)
  {
    s->status = GBN_FAILED;
    s->timer_running = false;
    sender_report(s, s->next, s->next, act);
    return;
  }

  /* go back: resend the whole window */
  s->deadline_ms = now_ms + s->timeout_ms;
  sender_report(s, s->base, s->next, act);
}

bool gbn_sender_packet(const gbn_sender *s, uint32_t seq, rdt_packet *pkt)
{
  if (seq > s->data_packets)
  {
    return false;
  }
  if (seq == s->data_packets)
  {
    pkt->type = RDT_FIN;
    pkt->seq = seq;
    pkt->length = 0;
    return true;
  }
  size_t offset = (size_t)seq * RDT_MAX_PAYLOAD;
  size_t left = s->size - offset;
  size_t n = left < RDT_MAX_PAYLOAD ? left : RDT_MAX_PAYLOAD;
  pkt->type = RDT_DATA;
  pkt->seq = seq;
  pkt->length = (uint16_t)n;
  memcpy(pkt->payload, s->data + offset, n);
  return true;
}

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N receiver                                              */
/* ------------------------------------------------------------------------ */

void gbn_receiver_init(gbn_receiver *r, uint64_t now_ms)
{
  memset(r, 0, sizeof *r);
  r->last_activity_ms = now_ms;
}

void gbn_receiver_on_packet(gbn_receiver *r, const rdt_packet *pkt,
                            uint64_t now_ms, gbn_receiver_action *act)
{
  memset(act, 0, sizeof *act);
  r->last_activity_ms = now_ms;
  if (pkt->type == RDT_ACK)
  {
    return; /* nobody should send us these */
  }

  if (!r->finished && pkt->seq == r->expected)
  {
    if (pkt->type == RDT_DATA)
    {
      act->deliver = pkt->payload;
      act->deliver_len = pkt->length;
    }
    else
    {
      act->fin = true;
      r->finished = true;
      r->linger_until_ms = now_ms + GBN_LINGER_MS;
    }
    r->expected++;
  }

  /* In order or not, the reply is the same: everything below expected. */
  act->send_ack = true;
  act->ack_seq = r->expected;
}

uint64_t gbn_receiver_deadline(const gbn_receiver *r)
{
  return r->finished ? r->linger_until_ms : r->last_activity_ms + GBN_IDLE_MS;
}

gbn_status gbn_receiver_on_tick(const gbn_receiver *r, uint64_t now_ms)
{
  if (now_ms < gbn_receiver_deadline(r))
  {
    return GBN_RUNNING;
  }
  return r->finished ? GBN_DONE : GBN_FAILED;
}

/* ------------------------------------------------------------------------ */
/* Layer 3 helpers                                                          */
/* ------------------------------------------------------------------------ */

void app_usage(FILE *out)
{
  fprintf(out,
          "Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]\n"
          "                  [-c corrupt] [-d dup] [-p port] <relay> <file>\n"
          "       myapp recv -s <session> [-p port] <relay> <file>\n"
          "  -s <session>     session name shared by the sender and the receiver\n"
          "  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)\n"
          "  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)\n"
          "  -l <loss>        probability the relay drops a packet (default: 0)\n"
          "  -c <corrupt>     probability the relay flips a bit (default: 0)\n"
          "  -d <dup>         probability the relay duplicates a packet (default: 0)\n"
          "  -p <port>        relay port (default: 4250)\n"
          "  <relay>          host name or address of the relay\n"
          "  <file>           file to send, or file to write what is received\n");
}

bool app_valid_session(const char *session)
{
  if (session == NULL)
  {
    return false;
  }
  size_t n = strlen(session);
  if (n < 1 || n > APP_SESSION_MAX)
  {
    return false;
  }
  for (size_t i = 0; i < n; i++)
  {
    char c = session[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
    {
      return false;
    }
  }
  return true;
}

static bool parse_ulong(const char *s, unsigned long lo, unsigned long hi,
                        unsigned long *out)
{
  if (s == NULL || *s < '0' || *s > '9')
  {
    return false; /* rejects "", "-1", " 1", "+1" */
  }
  char *end;
  errno = 0;
  unsigned long v = strtoul(s, &end, 10);
  if (errno != 0 || *end != '\0' || v < lo || v > hi)
  {
    return false;
  }
  *out = v;
  return true;
}

static bool parse_prob(const char *s, double *out)
{
  if (s == NULL || ((*s < '0' || *s > '9') && *s != '.'))
  {
    return false; /* rejects "", "-0.1", "nan", "inf" */
  }
  char *end;
  errno = 0;
  double v = strtod(s, &end);
  if (errno != 0 || *end != '\0' || !(v >= 0.0 && v <= 0.5))
  {
    return false;
  }
  *out = v;
  return true;
}

int app_parse_args(int argc, char *argv[], app_options *opts, FILE *err)
{
  opts->mode = APP_SEND;
  opts->session = NULL;
  opts->window = 8;
  opts->timeout_ms = 250;
  opts->loss = 0.0;
  opts->corrupt = 0.0;
  opts->dup = 0.0;
  opts->port = "4250";
  opts->relay = NULL;
  opts->file = NULL;

  if (argc < 2)
  {
    app_usage(err);
    return APP_EXIT_USAGE;
  }
  const char *optstring;
  if (strcmp(argv[1], "send") == 0)
  {
    opts->mode = APP_SEND;
    optstring = ":s:w:T:l:c:d:p:";
  }
  else if (strcmp(argv[1], "recv") == 0)
  {
    opts->mode = APP_RECV;
    optstring = ":s:p:";
  }
  else
  {
    fprintf(err, "myapp: unknown mode '%s'\n", argv[1]);
    app_usage(err);
    return APP_EXIT_USAGE;
  }

  /* Parse argv[1..] so the mode plays the role of the program name. */
  int sub_argc = argc - 1;
  char **sub_argv = argv + 1;
#ifdef __GLIBC__
  optind = 0; /* full reset, so the parser can run more than once */
#else
  optind = 1;
#endif
  opterr = 0;
  int c;
  unsigned long ul;
  while ((c = getopt(sub_argc, sub_argv, optstring)) != -1)
  {
    switch (c)
    {
    case 's':
      opts->session = optarg;
      break;
    case 'w':
      if (!parse_ulong(optarg, 1, GBN_MAX_WINDOW, &ul))
      {
        fprintf(err, "myapp: window must be 1 to %u: '%s'\n", GBN_MAX_WINDOW, optarg);
        return APP_EXIT_USAGE;
      }
      opts->window = (uint32_t)ul;
      break;
    case 'T':
      if (!parse_ulong(optarg, 1, 3600000, &ul))
      {
        fprintf(err, "myapp: timeout must be a positive number of ms: '%s'\n", optarg);
        return APP_EXIT_USAGE;
      }
      opts->timeout_ms = (uint32_t)ul;
      break;
    case 'l':
    case 'c':
    case 'd':
    {
      double *p = c == 'l' ? &opts->loss : c == 'c' ? &opts->corrupt : &opts->dup;
      if (!parse_prob(optarg, p))
      {
        fprintf(err, "myapp: -%c must be a probability from 0 to 0.5: '%s'\n", c, optarg);
        return APP_EXIT_USAGE;
      }
      break;
    }
    case 'p':
      if (!parse_ulong(optarg, 1, 65535, &ul))
      {
        fprintf(err, "myapp: port must be 1 to 65535: '%s'\n", optarg);
        return APP_EXIT_USAGE;
      }
      opts->port = optarg;
      break;
    case ':':
      fprintf(err, "myapp: option -%c needs an argument\n", optopt);
      app_usage(err);
      return APP_EXIT_USAGE;
    default:
      fprintf(err, "myapp: unknown option -%c for %s\n", optopt, argv[1]);
      app_usage(err);
      return APP_EXIT_USAGE;
    }
  }

  if (opts->session == NULL)
  {
    fprintf(err, "myapp: -s <session> is required\n");
    app_usage(err);
    return APP_EXIT_USAGE;
  }
  if (!app_valid_session(opts->session))
  {
    fprintf(err, "myapp: session must be 1 to 32 characters from a-z, 0-9 and -: '%s'\n",
            opts->session);
    return APP_EXIT_USAGE;
  }
  if (sub_argc - optind != 2)
  {
    fprintf(err, "myapp: expected <relay> <file>\n");
    app_usage(err);
    return APP_EXIT_USAGE;
  }
  opts->relay = sub_argv[optind];
  opts->file = sub_argv[optind + 1];
  return 0;
}

/* Write a probability as a plain decimal, e.g. 0.1, 0.05, 0. */
static void format_prob(double p, char *buf, size_t cap)
{
  snprintf(buf, cap, "%.6f", p);
  char *end = buf + strlen(buf) - 1;
  while (*end == '0')
  {
    *end-- = '\0';
  }
  if (*end == '.')
  {
    *end = '\0';
  }
}

size_t app_format_hello(const app_options *opts, char *buf, size_t cap)
{
  int n;
  if (opts->mode == APP_RECV)
  {
    n = snprintf(buf, cap, "HELLO %s recv", opts->session);
  }
  else
  {
    char l[32], c[32], d[32];
    format_prob(opts->loss, l, sizeof l);
    format_prob(opts->corrupt, c, sizeof c);
    format_prob(opts->dup, d, sizeof d);
    n = snprintf(buf, cap, "HELLO %s send %s %s %s", opts->session, l, c, d);
  }
  if (n < 0 || (size_t)n >= cap)
  {
    return 0;
  }
  return (size_t)n;
}

int app_parse_reply(const uint8_t *buf, size_t len, char *reason, size_t cap)
{
  if (cap > 0)
  {
    reason[0] = '\0';
  }
  /* Accept a trailing newline or whitespace in case the relay sends one. */
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' || buf[len - 1] == ' '))
  {
    len--;
  }
  if (len == 2 && memcmp(buf, "OK", 2) == 0)
  {
    return 0;
  }
  if (len >= 3 && memcmp(buf, "ERR", 3) == 0 && (len == 3 || buf[3] == ' '))
  {
    size_t start = len > 3 ? 4 : 3;
    size_t n = len - start;
    if (cap > 0)
    {
      if (n >= cap)
      {
        n = cap - 1;
      }
      memcpy(reason, buf + start, n);
      reason[n] = '\0';
    }
    return 1;
  }
  return -1;
}
