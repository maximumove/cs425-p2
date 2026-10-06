/*
 * Layer 3: the I/O. The socket, the relay hello, the poll loop, the clock and
 * the file live here and nowhere else. Each loop reads one event, hands it to
 * the state machine in lab.c, and carries out what comes back.
 */
#define _POSIX_C_SOURCE 200809L

#include "lab.h"

#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

#define HELLO_ATTEMPTS 5
#define HELLO_WAIT_MS 1000

typedef struct
{
  int fd;
  struct sockaddr_storage relay;
  socklen_t relay_len;
} relay_conn;

static uint64_t now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static int ms_until(uint64_t deadline)
{
  uint64_t now = now_ms();
  if (deadline <= now)
  {
    return 0;
  }
  uint64_t left = deadline - now;
  return left > 60000 ? 60000 : (int)left;
}

static bool same_addr(const struct sockaddr_storage *a, socklen_t alen,
                      const struct sockaddr_storage *b, socklen_t blen)
{
  return alen == blen && memcmp(a, b, alen) == 0;
}

/* Receive one datagram from the relay. Returns its size, or -1 when the
 * datagram should be ignored (error, or from someone else). */
static ssize_t relay_recv(relay_conn *rc, uint8_t *buf, size_t cap)
{
  struct sockaddr_storage from;
  socklen_t from_len = sizeof from;
  memset(&from, 0, sizeof from);
  ssize_t n = recvfrom(rc->fd, buf, cap, 0, (struct sockaddr *)&from, &from_len);
  if (n < 0 || !same_addr(&from, from_len, &rc->relay, rc->relay_len))
  {
    return -1;
  }
  return n;
}

static void relay_send(relay_conn *rc, const rdt_packet *pkt)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = rdt_encode(pkt, buf, sizeof buf);
  if (n > 0)
  {
    /* A failed send is just a lost packet; the timer recovers it. */
    (void)sendto(rc->fd, buf, n, 0, (struct sockaddr *)&rc->relay, rc->relay_len);
  }
}

/* Resolve the relay and open the one socket used for the whole run. */
static int relay_open(relay_conn *rc, const char *host, const char *port)
{
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;
  struct addrinfo *res;
  int rv = getaddrinfo(host, port, &hints, &res);
  if (rv != 0)
  {
    fprintf(stderr, "myapp: cannot resolve %s: %s\n", host, gai_strerror(rv));
    return -1;
  }
  rc->fd = -1;
  for (struct addrinfo *ai = res; ai != NULL; ai = ai->ai_next)
  {
    int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0)
    {
      continue;
    }
    rc->fd = fd;
    memcpy(&rc->relay, ai->ai_addr, ai->ai_addrlen);
    rc->relay_len = ai->ai_addrlen;
    break;
  }
  freeaddrinfo(res);
  if (rc->fd < 0)
  {
    perror("myapp: socket");
    return -1;
  }
  return 0;
}

/* Say hello and wait for OK, retrying once a second up to five times. */
static int relay_hello(relay_conn *rc, const app_options *opts)
{
  char hello[APP_HELLO_MAX];
  size_t len = app_format_hello(opts, hello, sizeof hello);
  for (int attempt = 0; attempt < HELLO_ATTEMPTS; attempt++)
  {
    if (sendto(rc->fd, hello, len, 0, (struct sockaddr *)&rc->relay, rc->relay_len) < 0)
    {
      perror("myapp: sendto");
    }
    uint64_t deadline = now_ms() + HELLO_WAIT_MS;
    struct pollfd pfd = {.fd = rc->fd, .events = POLLIN};
    int wait;
    while ((wait = ms_until(deadline)) > 0)
    {
      if (poll(&pfd, 1, wait) <= 0)
      {
        continue;
      }
      uint8_t buf[256];
      ssize_t n = relay_recv(rc, buf, sizeof buf);
      if (n < 0)
      {
        continue;
      }
      char reason[200];
      int r = app_parse_reply(buf, (size_t)n, reason, sizeof reason);
      if (r == 0)
      {
        return 0;
      }
      if (r == 1)
      {
        fprintf(stderr, "myapp: relay refused: %s\n", reason);
        return -1;
      }
    }
  }
  fprintf(stderr, "myapp: no reply from relay %s port %s after %d attempts\n",
          opts->relay, opts->port, HELLO_ATTEMPTS);
  return -1;
}

static uint8_t *read_file(const char *path, size_t *size)
{
  FILE *f = fopen(path, "rb");
  if (f == NULL)
  {
    perror(path);
    return NULL;
  }
  size_t cap = 65536, len = 0;
  uint8_t *data = malloc(cap);
  while (data != NULL)
  {
    size_t n = fread(data + len, 1, cap - len, f);
    len += n;
    if (n == 0)
    {
      break;
    }
    if (len == cap)
    {
      cap *= 2;
      uint8_t *bigger = realloc(data, cap);
      if (bigger == NULL)
      {
        free(data);
      }
      data = bigger;
    }
  }
  if (data == NULL || ferror(f))
  {
    fprintf(stderr, "myapp: cannot read %s\n", path);
    free(data);
    fclose(f);
    return NULL;
  }
  fclose(f);
  *size = len;
  return data;
}

static void send_range(relay_conn *rc, const gbn_sender *s, const gbn_sender_action *act)
{
  rdt_packet pkt;
  for (uint32_t seq = act->send_from; seq < act->send_to; seq++)
  {
    if (gbn_sender_packet(s, seq, &pkt))
    {
      relay_send(rc, &pkt);
    }
  }
}

static int run_sender(relay_conn *rc, const app_options *opts)
{
  size_t size = 0;
  uint8_t *data = read_file(opts->file, &size);
  if (data == NULL)
  {
    return APP_EXIT_FAIL;
  }
  gbn_sender s;
  if (!gbn_sender_init(&s, data, size, opts->window, opts->timeout_ms))
  {
    fprintf(stderr, "myapp: cannot send %s\n", opts->file);
    free(data);
    return APP_EXIT_FAIL;
  }
  if (relay_hello(rc, opts) != 0)
  {
    free(data);
    return APP_EXIT_FAIL;
  }

  gbn_sender_action act;
  gbn_sender_start(&s, now_ms(), &act);
  send_range(rc, &s, &act);

  struct pollfd pfd = {.fd = rc->fd, .events = POLLIN};
  while (act.status == GBN_RUNNING)
  {
    int wait = act.timer_running ? ms_until(act.deadline_ms) : 1000;
    if (poll(&pfd, 1, wait) > 0)
    {
      uint8_t buf[RDT_MAX_PACKET + 1];
      rdt_packet pkt;
      ssize_t n = relay_recv(rc, buf, sizeof buf);
      if (n >= 0 && rdt_decode(buf, (size_t)n, &pkt) && pkt.type == RDT_ACK)
      {
        gbn_sender_on_ack(&s, pkt.seq, now_ms(), &act);
        send_range(rc, &s, &act);
      }
    }
    if (act.status == GBN_RUNNING)
    {
      gbn_sender_on_tick(&s, now_ms(), &act);
      send_range(rc, &s, &act);
    }
  }

  free(data);
  if (act.status == GBN_FAILED)
  {
    fprintf(stderr, "myapp: giving up after %d timeouts in a row with no progress\n",
            GBN_MAX_TIMEOUTS);
    return APP_EXIT_FAIL;
  }
  return APP_EXIT_OK;
}

static int run_receiver(relay_conn *rc, const app_options *opts)
{
  FILE *out = fopen(opts->file, "wb");
  if (out == NULL)
  {
    perror(opts->file);
    return APP_EXIT_FAIL;
  }
  if (relay_hello(rc, opts) != 0)
  {
    fclose(out);
    return APP_EXIT_FAIL;
  }

  gbn_receiver r;
  gbn_receiver_init(&r, now_ms());
  struct pollfd pfd = {.fd = rc->fd, .events = POLLIN};
  gbn_status status = GBN_RUNNING;
  int rv = APP_EXIT_OK;
  while (status == GBN_RUNNING)
  {
    if (poll(&pfd, 1, ms_until(gbn_receiver_deadline(&r))) > 0)
    {
      uint8_t buf[RDT_MAX_PACKET + 1];
      rdt_packet pkt;
      ssize_t n = relay_recv(rc, buf, sizeof buf);
      if (n >= 0 && rdt_decode(buf, (size_t)n, &pkt))
      {
        gbn_receiver_action act;
        gbn_receiver_on_packet(&r, &pkt, now_ms(), &act);
        if (act.deliver_len > 0 && fwrite(act.deliver, 1, act.deliver_len, out) != act.deliver_len)
        {
          perror(opts->file);
          rv = APP_EXIT_FAIL;
          break;
        }
        if (act.fin)
        {
          int closed = fclose(out);
          out = NULL;
          if (closed != 0)
          {
            perror(opts->file);
            rv = APP_EXIT_FAIL;
            break;
          }
        }
        if (act.send_ack)
        {
          rdt_packet ack;
          rdt_make_ack(&ack, act.ack_seq);
          relay_send(rc, &ack);
        }
      }
    }
    status = gbn_receiver_on_tick(&r, now_ms());
  }

  if (out != NULL)
  {
    fclose(out);
  }
  if (status == GBN_FAILED)
  {
    fprintf(stderr, "myapp: nothing arrived for %u seconds, giving up\n", GBN_IDLE_MS / 1000);
    rv = APP_EXIT_FAIL;
  }
  return rv;
}

int main(int argc, char *argv[])
{
  if (argc == 1)
  {
    app_usage(stdout);
    return APP_EXIT_OK;
  }
  app_options opts;
  if (app_parse_args(argc, argv, &opts, stderr) != 0)
  {
    return APP_EXIT_USAGE;
  }

  relay_conn rc;
  if (relay_open(&rc, opts.relay, opts.port) != 0)
  {
    return APP_EXIT_FAIL;
  }
  int rv = opts.mode == APP_SEND ? run_sender(&rc, &opts) : run_receiver(&rc, &opts);
  close(rc.fd);
  return rv;
}
