#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "harness/unity.h"
#include "../src/lab.h"

void setUp(void) {}

void tearDown(void) {}

/* ------------------------------------------------------------------------ */
/* Helpers                                                                  */
/* ------------------------------------------------------------------------ */

static size_t encode_data(uint32_t seq, const char *payload, uint8_t *buf)
{
  rdt_packet p;
  p.type = RDT_DATA;
  p.seq = seq;
  p.length = (uint16_t)strlen(payload);
  memcpy(p.payload, payload, p.length);
  return rdt_encode(&p, buf, RDT_MAX_PACKET);
}

static rdt_packet make_pkt(rdt_type type, uint32_t seq, const char *payload)
{
  rdt_packet p;
  memset(&p, 0, sizeof p);
  p.type = type;
  p.seq = seq;
  p.length = (uint16_t)strlen(payload);
  memcpy(p.payload, payload, p.length);
  return p;
}

static void fill_pattern(uint8_t *buf, size_t n, uint32_t seed)
{
  for (size_t i = 0; i < n; i++)
  {
    seed = seed * 1103515245u + 12345u;
    buf[i] = (uint8_t)(seed >> 16);
  }
}

/* ------------------------------------------------------------------------ */
/* Layer 1: checksum                                                        */
/* ------------------------------------------------------------------------ */

void test_checksum_rfc1071_example(void)
{
  const uint8_t bytes[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
  /* the sum is 0xddf2, so the checksum is its complement */
  TEST_ASSERT_EQUAL_HEX16(0x220d, rdt_checksum(bytes, sizeof bytes));
}

void test_checksum_odd_length_pads_with_zero(void)
{
  const uint8_t odd[] = {0x12, 0x34, 0x56};
  const uint8_t padded[] = {0x12, 0x34, 0x56, 0x00};
  /* 0x1234 + 0x5600 = 0x6834 */
  TEST_ASSERT_EQUAL_HEX16((uint16_t)~0x6834u, rdt_checksum(odd, sizeof odd));
  TEST_ASSERT_EQUAL_HEX16(rdt_checksum(padded, sizeof padded), rdt_checksum(odd, sizeof odd));
}

void test_checksum_folds_carry(void)
{
  const uint8_t bytes[] = {0xff, 0xff, 0x00, 0x02};
  /* 0xffff + 0x0002 = 0x10001 -> fold -> 0x0002 */
  TEST_ASSERT_EQUAL_HEX16((uint16_t)~0x0002u, rdt_checksum(bytes, sizeof bytes));
}

void test_checksum_empty(void)
{
  TEST_ASSERT_EQUAL_HEX16(0xffff, rdt_checksum(NULL, 0));
}

void test_checksum_catches_every_single_bit_flip(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(2, "Hi!", buf);
  TEST_ASSERT_EQUAL_HEX16(0, rdt_checksum(buf, n));
  for (size_t bit = 0; bit < n * 8; bit++)
  {
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
    TEST_ASSERT_NOT_EQUAL(0, rdt_checksum(buf, n));
    rdt_packet p;
    TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
  }
}

/* ------------------------------------------------------------------------ */
/* Layer 1: encode                                                          */
/* ------------------------------------------------------------------------ */

void test_encode_worked_example_hi(void)
{
  const uint8_t expected[] = {0x00, 0x00, 0x96, 0x91, 0x00, 0x00, 0x00,
                              0x02, 0x00, 0x03, 0x48, 0x69, 0x21};
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(2, "Hi!", buf);
  TEST_ASSERT_EQUAL_size_t(sizeof expected, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, n);
}

void test_encode_worked_example_ack3(void)
{
  const uint8_t expected[] = {0x01, 0x00, 0xfe, 0xfc, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00};
  rdt_packet p;
  rdt_make_ack(&p, 3);
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = rdt_encode(&p, buf, sizeof buf);
  TEST_ASSERT_EQUAL_size_t(10, n);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, buf, n);
}

void test_make_ack(void)
{
  rdt_packet p;
  memset(&p, 0xaa, sizeof p);
  rdt_make_ack(&p, 77);
  TEST_ASSERT_EQUAL(RDT_ACK, p.type);
  TEST_ASSERT_EQUAL_UINT32(77, p.seq);
  TEST_ASSERT_EQUAL_UINT16(0, p.length);
}

void test_encode_full_payload_and_large_seq(void)
{
  rdt_packet p;
  p.type = RDT_DATA;
  p.seq = 0x01020304;
  p.length = RDT_MAX_PAYLOAD;
  fill_pattern(p.payload, RDT_MAX_PAYLOAD, 9);
  uint8_t buf[RDT_MAX_PACKET];
  TEST_ASSERT_EQUAL_size_t(RDT_MAX_PACKET, rdt_encode(&p, buf, sizeof buf));
  /* network byte order */
  TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[7]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[8]);
  TEST_ASSERT_EQUAL_HEX8(0x00, buf[9]);
}

void test_encode_rejects_bad_input(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  rdt_packet p = make_pkt(RDT_DATA, 0, "abc");
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(NULL, buf, sizeof buf));
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, NULL, sizeof buf));
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, buf, 12)); /* needs 13 */
  TEST_ASSERT_EQUAL_size_t(13, rdt_encode(&p, buf, 13));
  p.length = RDT_MAX_PAYLOAD + 1;
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, buf, sizeof buf));
  p.length = 0;
  p.type = (rdt_type)3;
  TEST_ASSERT_EQUAL_size_t(0, rdt_encode(&p, buf, sizeof buf));
}

/* ------------------------------------------------------------------------ */
/* Layer 1: decode                                                          */
/* ------------------------------------------------------------------------ */

void test_decode_round_trip(void)
{
  rdt_packet in, out;
  in.type = RDT_DATA;
  in.seq = 123456;
  in.length = 700;
  fill_pattern(in.payload, in.length, 3);
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = rdt_encode(&in, buf, sizeof buf);
  TEST_ASSERT_TRUE(rdt_decode(buf, n, &out));
  TEST_ASSERT_EQUAL(RDT_DATA, out.type);
  TEST_ASSERT_EQUAL_UINT32(123456, out.seq);
  TEST_ASSERT_EQUAL_UINT16(700, out.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(in.payload, out.payload, 700);

  rdt_packet fin = make_pkt(RDT_FIN, 9, "");
  n = rdt_encode(&fin, buf, sizeof buf);
  TEST_ASSERT_TRUE(rdt_decode(buf, n, &out));
  TEST_ASSERT_EQUAL(RDT_FIN, out.type);
  TEST_ASSERT_EQUAL_UINT32(9, out.seq);
}

void test_decode_too_short(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(0, "", buf);
  rdt_packet p;
  TEST_ASSERT_EQUAL_size_t(10, n);
  TEST_ASSERT_TRUE(rdt_decode(buf, n, &p));
  for (size_t len = 0; len < RDT_HEADER_LEN; len++)
  {
    TEST_ASSERT_FALSE(rdt_decode(buf, len, &p));
  }
  TEST_ASSERT_FALSE(rdt_decode(NULL, 10, &p));
  TEST_ASSERT_FALSE(rdt_decode(buf, n, NULL));
}

void test_decode_length_mismatch(void)
{
  uint8_t buf[RDT_MAX_PACKET + 4];
  size_t n = encode_data(1, "hello", buf);
  rdt_packet p;
  TEST_ASSERT_FALSE(rdt_decode(buf, n - 1, &p)); /* truncated */
  buf[n] = 0;
  buf[n + 1] = 0;
  TEST_ASSERT_FALSE(rdt_decode(buf, n + 2, &p)); /* trailing bytes */
}

void test_decode_length_field_over_max(void)
{
  /* A length field of 1025 in a datagram that is really 1035 bytes long,
   * with a correct checksum: still rejected. */
  uint8_t buf[RDT_MAX_PACKET + 1];
  memset(buf, 0, sizeof buf);
  buf[8] = 0x04;
  buf[9] = 0x01;
  uint16_t sum = rdt_checksum(buf, sizeof buf);
  buf[2] = (uint8_t)(sum >> 8);
  buf[3] = (uint8_t)sum;
  rdt_packet p;
  TEST_ASSERT_FALSE(rdt_decode(buf, sizeof buf, &p));
}

void test_decode_unknown_type_and_reserved(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  rdt_packet p;

  /* Rewrite the header and fix the checksum so only the field is wrong. */
  size_t n = encode_data(1, "x", buf);
  buf[0] = 3;
  buf[2] = buf[3] = 0;
  uint16_t sum = rdt_checksum(buf, n);
  buf[2] = (uint8_t)(sum >> 8);
  buf[3] = (uint8_t)sum;
  TEST_ASSERT_EQUAL_HEX16(0, rdt_checksum(buf, n));
  TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));

  n = encode_data(1, "x", buf);
  buf[1] = 1;
  buf[2] = buf[3] = 0;
  sum = rdt_checksum(buf, n);
  buf[2] = (uint8_t)(sum >> 8);
  buf[3] = (uint8_t)sum;
  TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));
}

void test_decode_bad_checksum(void)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t n = encode_data(2, "Hi!", buf);
  rdt_packet p;
  buf[12] ^= 0x01;
  TEST_ASSERT_FALSE(rdt_decode(buf, n, &p));
}

/* ------------------------------------------------------------------------ */
/* Layer 2: sender                                                          */
/* ------------------------------------------------------------------------ */

static uint8_t file_buf[64 * 1024];

void test_sender_init_counts_packets(void)
{
  gbn_sender s;
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 2500, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(3, s.data_packets);
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 2048, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(2, s.data_packets);
  TEST_ASSERT_TRUE(gbn_sender_init(&s, NULL, 0, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(0, s.data_packets);
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 1, 4, 250));
  TEST_ASSERT_EQUAL_UINT32(1, s.data_packets);
}

void test_sender_init_rejects_bad_args(void)
{
  gbn_sender s;
  TEST_ASSERT_FALSE(gbn_sender_init(NULL, file_buf, 10, 4, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, NULL, 10, 4, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, file_buf, 10, 0, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, file_buf, 10, 65, 250));
  TEST_ASSERT_FALSE(gbn_sender_init(&s, file_buf, 10, 4, 0));
  TEST_ASSERT_TRUE(gbn_sender_init(&s, file_buf, 10, 64, 1));
}

void test_sender_packet_cuts_file(void)
{
  /* Worked example 1: 2500 bytes -> 1024, 1024, 452, FIN 3 */
  fill_pattern(file_buf, 2500, 1);
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 2500, 4, 250);
  rdt_packet p;
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 0, &p));
  TEST_ASSERT_EQUAL(RDT_DATA, p.type);
  TEST_ASSERT_EQUAL_UINT16(1024, p.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(file_buf, p.payload, 1024);
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 1, &p));
  TEST_ASSERT_EQUAL_UINT16(1024, p.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(file_buf + 1024, p.payload, 1024);
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 2, &p));
  TEST_ASSERT_EQUAL_UINT32(2, p.seq);
  TEST_ASSERT_EQUAL_UINT16(452, p.length);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(file_buf + 2048, p.payload, 452);
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 3, &p));
  TEST_ASSERT_EQUAL(RDT_FIN, p.type);
  TEST_ASSERT_EQUAL_UINT32(3, p.seq);
  TEST_ASSERT_EQUAL_UINT16(0, p.length);
  TEST_ASSERT_FALSE(gbn_sender_packet(&s, 4, &p));
}

void test_sender_exact_multiple_of_1024(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 3072, 8, 250);
  rdt_packet p;
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 2, &p));
  TEST_ASSERT_EQUAL(RDT_DATA, p.type);
  TEST_ASSERT_EQUAL_UINT16(1024, p.length); /* no empty DATA at the end */
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 3, &p));
  TEST_ASSERT_EQUAL(RDT_FIN, p.type);

  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(3, a.send_to); /* DATA only; FIN waits */
  gbn_sender_on_ack(&s, 3, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(3, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_to); /* now the FIN */
  gbn_sender_on_ack(&s, 4, 20, &a);
  TEST_ASSERT_EQUAL(GBN_DONE, a.status);
}

void test_sender_empty_file(void)
{
  gbn_sender s;
  gbn_sender_init(&s, NULL, 0, 8, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_to);
  rdt_packet p;
  TEST_ASSERT_TRUE(gbn_sender_packet(&s, 0, &p));
  TEST_ASSERT_EQUAL(RDT_FIN, p.type);
  TEST_ASSERT_EQUAL_UINT32(0, p.seq);
  TEST_ASSERT_TRUE(a.timer_running);
  /* the FIN is retransmitted like any other packet */
  gbn_sender_on_tick(&s, 250, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_to);
  gbn_sender_on_ack(&s, 1, 300, &a);
  TEST_ASSERT_EQUAL(GBN_DONE, a.status);
  TEST_ASSERT_FALSE(a.timer_running);
}

void test_sender_window_full(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 10 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 1000, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_to);
  TEST_ASSERT_TRUE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT64(1250, a.deadline_ms);
  TEST_ASSERT_EQUAL_UINT32(0, s.base);
  TEST_ASSERT_EQUAL_UINT32(4, s.next);

  /* nothing more can go until an ACK moves base */
  gbn_sender_on_tick(&s, 1100, &a);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
  TEST_ASSERT_EQUAL(GBN_RUNNING, a.status);

  /* an ACK for one packet opens room for exactly one more */
  gbn_sender_on_ack(&s, 1, 1100, &a);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(5, a.send_to);
  TEST_ASSERT_EQUAL_UINT64(1350, a.deadline_ms); /* restarted */
}

void test_sender_stop_and_wait(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 3000, 1, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_to);
  gbn_sender_on_ack(&s, 1, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(1, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(2, a.send_to);
  gbn_sender_on_tick(&s, 110, &a); /* timeout resends only packet 1 */
  TEST_ASSERT_EQUAL_UINT32(1, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(2, a.send_to);
}

void test_sender_cumulative_ack_slides_several(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 20 * 1024, 6, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_to);
  /* Worked example 3: ACKs 1..4 were lost, ACK 5 covers them all. */
  gbn_sender_on_ack(&s, 5, 50, &a);
  TEST_ASSERT_EQUAL_UINT32(5, s.base);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(11, a.send_to);
  TEST_ASSERT_TRUE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT64(300, a.deadline_ms);
}

void test_sender_duplicate_ack_ignored(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 6 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 2, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(2, s.base);
  TEST_ASSERT_EQUAL_UINT64(260, a.deadline_ms);
  uint32_t next = s.next;
  for (int i = 0; i < 3; i++)
  {
    gbn_sender_on_ack(&s, 2, 100, &a);
    TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
    TEST_ASSERT_EQUAL_UINT64(260, a.deadline_ms); /* timer not restarted */
  }
  gbn_sender_on_ack(&s, 1, 100, &a); /* older still */
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
  TEST_ASSERT_EQUAL_UINT32(2, s.base);
  TEST_ASSERT_EQUAL_UINT32(next, s.next);
}

void test_sender_ack_beyond_next_ignored(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 10 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 9, 10, &a);
  TEST_ASSERT_EQUAL_UINT32(0, s.base);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
}

void test_sender_timeout_resends_whole_window(void)
{
  /* The diagram: window 4, six packets, DATA 2 lost. */
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 6 * 1024, 4, 250);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 1, 100, &a);
  TEST_ASSERT_EQUAL_UINT32(4, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(5, a.send_to);
  gbn_sender_on_ack(&s, 2, 110, &a);
  TEST_ASSERT_EQUAL_UINT32(5, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_to);
  gbn_sender_on_ack(&s, 2, 200, &a);
  gbn_sender_on_ack(&s, 2, 210, &a);
  gbn_sender_on_ack(&s, 2, 220, &a);
  gbn_sender_on_tick(&s, 359, &a); /* not yet */
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
  gbn_sender_on_tick(&s, 360, &a);
  TEST_ASSERT_EQUAL_UINT32(2, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_to);
  TEST_ASSERT_EQUAL_UINT64(610, a.deadline_ms);
  gbn_sender_on_ack(&s, 6, 450, &a);
  TEST_ASSERT_EQUAL_UINT32(6, a.send_from);
  TEST_ASSERT_EQUAL_UINT32(7, a.send_to); /* the FIN */
}

void test_sender_gives_up_after_ten_timeouts(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 4 * 1024, 2, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  uint64_t now = 0;
  for (int i = 1; i < GBN_MAX_TIMEOUTS; i++)
  {
    now += 100;
    gbn_sender_on_tick(&s, now, &a);
    TEST_ASSERT_EQUAL(GBN_RUNNING, a.status);
    TEST_ASSERT_EQUAL_UINT32(0, a.send_from);
    TEST_ASSERT_EQUAL_UINT32(2, a.send_to);
  }
  now += 100;
  gbn_sender_on_tick(&s, now, &a);
  TEST_ASSERT_EQUAL(GBN_FAILED, a.status);
  TEST_ASSERT_FALSE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);

  /* a failed sender stays failed */
  gbn_sender_on_ack(&s, 2, now, &a);
  TEST_ASSERT_EQUAL(GBN_FAILED, a.status);
}

void test_sender_progress_resets_timeout_count(void)
{
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 8 * 1024, 2, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  uint64_t now = 0;
  for (int i = 1; i < GBN_MAX_TIMEOUTS; i++)
  {
    now += 100;
    gbn_sender_on_tick(&s, now, &a);
  }
  gbn_sender_on_ack(&s, 1, now, &a); /* progress */
  TEST_ASSERT_EQUAL_INT(0, s.timeouts);
  for (int i = 1; i < GBN_MAX_TIMEOUTS; i++)
  {
    now += 100;
    gbn_sender_on_tick(&s, now, &a);
    TEST_ASSERT_EQUAL(GBN_RUNNING, a.status);
  }
}

void test_sender_timer_stops_when_all_acked(void)
{
  /* window 2 over 2 DATA: when both are acked the FIN goes out, and the
   * timer is restarted for it rather than left stopped */
  gbn_sender s;
  gbn_sender_init(&s, file_buf, 2048, 2, 100);
  gbn_sender_action a;
  gbn_sender_start(&s, 0, &a);
  gbn_sender_on_ack(&s, 2, 50, &a);
  TEST_ASSERT_TRUE(a.timer_running);
  TEST_ASSERT_EQUAL_UINT64(150, a.deadline_ms);
  /* tick with no timer due does nothing */
  gbn_sender_on_tick(&s, 60, &a);
  TEST_ASSERT_EQUAL_UINT32(a.send_from, a.send_to);
}

/* ------------------------------------------------------------------------ */
/* Layer 2: receiver                                                        */
/* ------------------------------------------------------------------------ */

void test_receiver_in_order(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p = make_pkt(RDT_DATA, 0, "abc");
  gbn_receiver_on_packet(&r, &p, 10, &a);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(3, a.deliver_len);
  TEST_ASSERT_EQUAL_MEMORY("abc", a.deliver, 3);
  TEST_ASSERT_FALSE(a.fin);
}

void test_receiver_duplicate(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p = make_pkt(RDT_DATA, 0, "abc");
  gbn_receiver_on_packet(&r, &p, 10, &a);
  gbn_receiver_on_packet(&r, &p, 20, &a);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  TEST_ASSERT_EQUAL_UINT32(1, r.expected);
}

void test_receiver_beyond_gap(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p0 = make_pkt(RDT_DATA, 0, "a");
  rdt_packet p2 = make_pkt(RDT_DATA, 2, "c");
  rdt_packet fin = make_pkt(RDT_FIN, 3, "");
  gbn_receiver_on_packet(&r, &p0, 10, &a);
  gbn_receiver_on_packet(&r, &p2, 20, &a);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  /* a FIN beyond the gap does not end anything */
  gbn_receiver_on_packet(&r, &fin, 30, &a);
  TEST_ASSERT_FALSE(a.fin);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_FALSE(r.finished);
}

void test_receiver_fin_and_repeated_fin(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet p0 = make_pkt(RDT_DATA, 0, "a");
  rdt_packet fin = make_pkt(RDT_FIN, 1, "");
  gbn_receiver_on_packet(&r, &p0, 10, &a);
  gbn_receiver_on_packet(&r, &fin, 100, &a);
  TEST_ASSERT_TRUE(a.fin);
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(2, a.ack_seq);
  TEST_ASSERT_EQUAL_UINT64(100 + GBN_LINGER_MS, gbn_receiver_deadline(&r));

  /* Worked example 5: ACK 2 was lost and the FIN comes again. */
  gbn_receiver_on_packet(&r, &fin, 400, &a);
  TEST_ASSERT_FALSE(a.fin); /* the file is not closed twice */
  TEST_ASSERT_TRUE(a.send_ack);
  TEST_ASSERT_EQUAL_UINT32(2, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  /* the linger is not extended */
  TEST_ASSERT_EQUAL_UINT64(100 + GBN_LINGER_MS, gbn_receiver_deadline(&r));

  /* a stray DATA after the FIN is just acked */
  gbn_receiver_on_packet(&r, &p0, 500, &a);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
  TEST_ASSERT_EQUAL_UINT32(2, a.ack_seq);
}

void test_receiver_empty_file(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet fin = make_pkt(RDT_FIN, 0, "");
  gbn_receiver_on_packet(&r, &fin, 5, &a);
  TEST_ASSERT_TRUE(a.fin);
  TEST_ASSERT_EQUAL_UINT32(1, a.ack_seq);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
}

void test_receiver_ignores_ack(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet ack;
  rdt_make_ack(&ack, 0);
  gbn_receiver_on_packet(&r, &ack, 5, &a);
  TEST_ASSERT_FALSE(a.send_ack);
  TEST_ASSERT_FALSE(a.fin);
  TEST_ASSERT_EQUAL_size_t(0, a.deliver_len);
}

void test_receiver_linger_then_done(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 0);
  gbn_receiver_action a;
  rdt_packet fin = make_pkt(RDT_FIN, 0, "");
  gbn_receiver_on_packet(&r, &fin, 1000, &a);
  TEST_ASSERT_EQUAL(GBN_RUNNING, gbn_receiver_on_tick(&r, 1000 + GBN_LINGER_MS - 1));
  TEST_ASSERT_EQUAL(GBN_DONE, gbn_receiver_on_tick(&r, 1000 + GBN_LINGER_MS));
}

void test_receiver_idle_gives_up(void)
{
  gbn_receiver r;
  gbn_receiver_init(&r, 500);
  TEST_ASSERT_EQUAL_UINT64(500 + GBN_IDLE_MS, gbn_receiver_deadline(&r));
  TEST_ASSERT_EQUAL(GBN_RUNNING, gbn_receiver_on_tick(&r, 500 + GBN_IDLE_MS - 1));
  TEST_ASSERT_EQUAL(GBN_FAILED, gbn_receiver_on_tick(&r, 500 + GBN_IDLE_MS));

  /* a valid packet pushes the deadline back */
  gbn_receiver_action a;
  rdt_packet p = make_pkt(RDT_DATA, 5, "z");
  gbn_receiver_on_packet(&r, &p, 20000, &a);
  TEST_ASSERT_EQUAL_UINT64(20000 + GBN_IDLE_MS, gbn_receiver_deadline(&r));
  TEST_ASSERT_EQUAL(GBN_RUNNING, gbn_receiver_on_tick(&r, 30500));
}

/* ------------------------------------------------------------------------ */
/* Layer 2: a whole transfer through a damaging in-memory channel           */
/* ------------------------------------------------------------------------ */

#define SIM_QUEUE 4096
#define SIM_DELAY_MS 20

typedef struct
{
  uint64_t due;
  size_t len;
  uint8_t bytes[RDT_MAX_PACKET];
} sim_dgram;

/* One direction of the channel. The delay is constant, so it is a FIFO. */
typedef struct
{
  sim_dgram q[SIM_QUEUE];
  size_t head, count;
  double loss, corrupt, dup;
  uint32_t rng;
  unsigned dropped, corrupted, duplicated;
} sim_chan;

static uint32_t sim_rand(uint32_t *state)
{
  /* xorshift32 */
  uint32_t x = *state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  *state = x;
  return x;
}

static double sim_uniform(uint32_t *state)
{
  return (double)sim_rand(state) / 4294967296.0;
}

static void sim_enqueue(sim_chan *c, const uint8_t *bytes, size_t len, uint64_t due)
{
  TEST_ASSERT_TRUE_MESSAGE(c->count < SIM_QUEUE, "simulated channel overflowed");
  sim_dgram *d = &c->q[(c->head + c->count) % SIM_QUEUE];
  d->due = due;
  d->len = len;
  memcpy(d->bytes, bytes, len);
  c->count++;
}

/* Hand a packet to the channel, damaging it the way the relay does. */
static void sim_send(sim_chan *c, const rdt_packet *pkt, uint64_t now)
{
  uint8_t buf[RDT_MAX_PACKET];
  size_t len = rdt_encode(pkt, buf, sizeof buf);
  TEST_ASSERT_NOT_EQUAL(0, len);
  if (sim_uniform(&c->rng) < c->loss)
  {
    c->dropped++;
    return;
  }
  if (sim_uniform(&c->rng) < c->corrupt)
  {
    uint32_t bit = sim_rand(&c->rng) % (uint32_t)(len * 8);
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
    c->corrupted++;
    sim_enqueue(c, buf, len, now + SIM_DELAY_MS);
    return;
  }
  if (sim_uniform(&c->rng) < c->dup)
  {
    c->duplicated++;
    sim_enqueue(c, buf, len, now + SIM_DELAY_MS);
  }
  sim_enqueue(c, buf, len, now + SIM_DELAY_MS);
}

static bool sim_pop(sim_chan *c, uint64_t now, sim_dgram *out)
{
  if (c->count == 0 || c->q[c->head].due > now)
  {
    return false;
  }
  *out = c->q[c->head];
  c->head = (c->head + 1) % SIM_QUEUE;
  c->count--;
  return true;
}

static void sim_send_range(sim_chan *c, const gbn_sender *s,
                           const gbn_sender_action *a, uint64_t now)
{
  rdt_packet p;
  for (uint32_t seq = a->send_from; seq < a->send_to; seq++)
  {
    TEST_ASSERT_TRUE(gbn_sender_packet(s, seq, &p));
    sim_send(c, &p, now);
  }
}

static sim_chan to_recv, to_send;
static uint8_t sim_in[200 * 1024];
static uint8_t sim_out[200 * 1024];

/* Run one transfer. Returns the sender's final status; the receiver's output
 * is in sim_out, *out_len bytes long. */
static gbn_status sim_transfer(size_t size, uint32_t window, double rate,
                               uint32_t seed, size_t *out_len, bool *closed)
{
  memset(&to_recv, 0, sizeof to_recv);
  memset(&to_send, 0, sizeof to_send);
  to_recv.loss = to_recv.corrupt = to_recv.dup = rate;
  to_send.loss = to_send.corrupt = to_send.dup = rate;
  to_recv.rng = seed * 2654435761u + 1;
  to_send.rng = seed * 40503u + 7;
  fill_pattern(sim_in, size, seed);

  gbn_sender s;
  gbn_receiver r;
  gbn_sender_action sa;
  TEST_ASSERT_TRUE(gbn_sender_init(&s, sim_in, size, window, 100));
  gbn_receiver_init(&r, 0);

  uint64_t now = 0;
  *out_len = 0;
  *closed = false;
  gbn_sender_start(&s, now, &sa);
  sim_send_range(&to_recv, &s, &sa, now);

  for (int steps = 0; sa.status == GBN_RUNNING && steps < 1000000; steps++)
  {
    /* jump straight to the next event */
    uint64_t next = UINT64_MAX;
    if (to_recv.count > 0)
      next = to_recv.q[to_recv.head].due;
    if (to_send.count > 0 && to_send.q[to_send.head].due < next)
      next = to_send.q[to_send.head].due;
    if (sa.timer_running && sa.deadline_ms < next)
      next = sa.deadline_ms;
    TEST_ASSERT_TRUE_MESSAGE(next != UINT64_MAX, "simulation stalled");
    now = next;

    sim_dgram d;
    rdt_packet p;
    while (sim_pop(&to_recv, now, &d))
    {
      if (!rdt_decode(d.bytes, d.len, &p))
        continue;
      gbn_receiver_action ra;
      gbn_receiver_on_packet(&r, &p, now, &ra);
      TEST_ASSERT_FALSE_MESSAGE(*closed && ra.deliver_len > 0, "data after FIN");
      memcpy(sim_out + *out_len, ra.deliver, ra.deliver_len);
      *out_len += ra.deliver_len;
      if (ra.fin)
        *closed = true;
      if (ra.send_ack)
      {
        rdt_packet ack;
        rdt_make_ack(&ack, ra.ack_seq);
        sim_send(&to_send, &ack, now);
      }
    }
    while (sim_pop(&to_send, now, &d) && sa.status == GBN_RUNNING)
    {
      if (!rdt_decode(d.bytes, d.len, &p) || p.type != RDT_ACK)
        continue;
      gbn_sender_on_ack(&s, p.seq, now, &sa);
      sim_send_range(&to_recv, &s, &sa, now);
    }
    if (sa.status == GBN_RUNNING)
    {
      gbn_sender_on_tick(&s, now, &sa);
      sim_send_range(&to_recv, &s, &sa, now);
    }
  }
  return sa.status;
}

static void check_transfer(size_t size, uint32_t window, double rate, uint32_t seed)
{
  size_t out_len;
  bool closed;
  gbn_status st = sim_transfer(size, window, rate, seed, &out_len, &closed);
  char msg[128];
  snprintf(msg, sizeof msg, "size %zu window %u rate %.2f seed %u", size, window, rate, seed);
  TEST_ASSERT_EQUAL_MESSAGE(GBN_DONE, st, msg);
  TEST_ASSERT_TRUE_MESSAGE(closed, msg);
  TEST_ASSERT_EQUAL_size_t_MESSAGE(size, out_len, msg);
  if (size > 0)
  {
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(sim_in, sim_out, size, msg);
  }
}

void test_transfer_clean_channel(void)
{
  check_transfer(2500, 4, 0.0, 1);
  check_transfer(0, 4, 0.0, 1);
  check_transfer(8192, 1, 0.0, 1);
}

void test_transfer_lossy_channel_many_seeds(void)
{
  /* 20% loss, 20% corruption and 20% duplication in each direction */
  for (uint32_t seed = 1; seed <= 20; seed++)
  {
    check_transfer(100 * 1024 + 321, 8, 0.2, seed);
  }
  /* the channel really did damage things */
  TEST_ASSERT_TRUE(to_recv.dropped > 0 && to_recv.corrupted > 0 && to_recv.duplicated > 0);
  TEST_ASSERT_TRUE(to_send.dropped > 0 && to_send.corrupted > 0 && to_send.duplicated > 0);
}

void test_transfer_lossy_channel_windows_and_sizes(void)
{
  const uint32_t windows[] = {1, 2, 16, 64};
  const size_t sizes[] = {0, 1, 1024, 4096, 50000};
  for (size_t w = 0; w < sizeof windows / sizeof windows[0]; w++)
  {
    for (size_t z = 0; z < sizeof sizes / sizeof sizes[0]; z++)
    {
      check_transfer(sizes[z], windows[w], 0.2, (uint32_t)(w * 10 + z + 100));
    }
  }
}

void test_transfer_dead_channel_gives_up(void)
{
  size_t out_len;
  bool closed;
  /* rate 1.0: everything is dropped */
  gbn_status st = sim_transfer(5000, 4, 1.0, 3, &out_len, &closed);
  TEST_ASSERT_EQUAL(GBN_FAILED, st);
  TEST_ASSERT_EQUAL_size_t(0, out_len);
}

/* ------------------------------------------------------------------------ */
/* Layer 3 helpers                                                          */
/* ------------------------------------------------------------------------ */

static FILE *devnull;

static int parse(app_options *o, int argc, ...)
{
  static char storage[24][64];
  static char *argv[25];
  va_list ap;
  va_start(ap, argc);
  for (int i = 0; i < argc; i++)
  {
    snprintf(storage[i], sizeof storage[i], "%s", va_arg(ap, const char *));
    argv[i] = storage[i];
  }
  va_end(ap);
  argv[argc] = NULL;
  return app_parse_args(argc, argv, o, devnull);
}

void test_usage_prints(void)
{
  char buf[2048] = {0};
  FILE *f = fmemopen(buf, sizeof buf - 1, "w");
  TEST_ASSERT_NOT_NULL(f);
  app_usage(f);
  fclose(f);
  TEST_ASSERT_NOT_NULL(strstr(buf, "Usage: myapp send -s <session>"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "myapp recv -s <session> [-p port] <relay> <file>"));
}

void test_parse_args_send_defaults(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 6, "myapp", "send", "-s", "jdoe-1", "127.0.0.1", "in.bin"));
  TEST_ASSERT_EQUAL(APP_SEND, o.mode);
  TEST_ASSERT_EQUAL_STRING("jdoe-1", o.session);
  TEST_ASSERT_EQUAL_UINT32(8, o.window);
  TEST_ASSERT_EQUAL_UINT32(250, o.timeout_ms);
  TEST_ASSERT_TRUE(o.loss == 0.0);
  TEST_ASSERT_EQUAL_STRING("4250", o.port);
  TEST_ASSERT_EQUAL_STRING("127.0.0.1", o.relay);
  TEST_ASSERT_EQUAL_STRING("in.bin", o.file);
}

void test_parse_args_send_all_options(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 18, "myapp", "send", "-s", "a", "-w", "16", "-T", "100",
                                 "-l", "0.1", "-c", "0.05", "-d", "0.5", "-p", "20000",
                                 "localhost", "f"));
  TEST_ASSERT_EQUAL_UINT32(16, o.window);
  TEST_ASSERT_EQUAL_UINT32(100, o.timeout_ms);
  TEST_ASSERT_TRUE(o.loss == 0.1);
  TEST_ASSERT_TRUE(o.corrupt == 0.05);
  TEST_ASSERT_TRUE(o.dup == 0.5);
  TEST_ASSERT_EQUAL_STRING("20000", o.port);
  TEST_ASSERT_EQUAL_STRING("localhost", o.relay);
}

void test_parse_args_recv(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "recv", "-s", "x", "-p", "4251", "h", "out"));
  TEST_ASSERT_EQUAL(APP_RECV, o.mode);
  TEST_ASSERT_EQUAL_STRING("4251", o.port);
  TEST_ASSERT_EQUAL_STRING("out", o.file);
  /* the parser can run again after a previous run */
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 6, "myapp", "recv", "-s", "y", "h", "out"));
  TEST_ASSERT_EQUAL_STRING("y", o.session);
}

void test_parse_args_errors(void)
{
  app_options o;
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 1, "myapp"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 2, "myapp", "bogus"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 4, "myapp", "send", "h", "f")); /* no -s */
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 5, "myapp", "send", "-s", "a", "h"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 7, "myapp", "send", "-s", "a", "h", "f", "x"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 3, "myapp", "send", "-s")); /* missing arg */
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 7, "myapp", "send", "-s", "a", "-z", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 6, "myapp", "send", "-s", "Bad!", "h", "f"));
  /* recv does not take sender options */
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "recv", "-s", "a", "-w", "4", "h", "f"));

  const char *bad_windows[] = {"0", "65", "-1", "abc", "4x", ""};
  for (size_t i = 0; i < sizeof bad_windows / sizeof bad_windows[0]; i++)
  {
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-w", bad_windows[i], "h", "f"));
  }
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "send", "-s", "a", "-w", "64", "h", "f"));
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "send", "-s", "a", "-w", "1", "h", "f"));

  const char *bad_probs[] = {"0.51", "1", "-0.1", "nan", "inf", "0.1x", ""};
  for (size_t i = 0; i < sizeof bad_probs / sizeof bad_probs[0]; i++)
  {
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-l", bad_probs[i], "h", "f"));
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-c", bad_probs[i], "h", "f"));
    TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE,
                          parse(&o, 8, "myapp", "send", "-s", "a", "-d", bad_probs[i], "h", "f"));
  }
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "send", "-s", "a", "-T", "0", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "send", "-s", "a", "-p", "0", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "send", "-s", "a", "-p", "65536", "h", "f"));
  TEST_ASSERT_EQUAL_INT(APP_EXIT_USAGE, parse(&o, 8, "myapp", "recv", "-s", "a", "-p", "x", "h", "f"));
}

void test_valid_session(void)
{
  TEST_ASSERT_TRUE(app_valid_session("jdoe-1"));
  TEST_ASSERT_TRUE(app_valid_session("a"));
  TEST_ASSERT_TRUE(app_valid_session("0123456789abcdefghijklmnopqrstuv")); /* 32 */
  TEST_ASSERT_FALSE(app_valid_session("0123456789abcdefghijklmnopqrstuvw")); /* 33 */
  TEST_ASSERT_FALSE(app_valid_session(""));
  TEST_ASSERT_FALSE(app_valid_session(NULL));
  TEST_ASSERT_FALSE(app_valid_session("JDoe"));
  TEST_ASSERT_FALSE(app_valid_session("a b"));
  TEST_ASSERT_FALSE(app_valid_session("a_b"));
}

void test_format_hello(void)
{
  app_options o;
  char buf[APP_HELLO_MAX];
  TEST_ASSERT_EQUAL_INT(0, parse(&o, 6, "myapp", "recv", "-s", "jdoe-1", "h", "f"));
  TEST_ASSERT_EQUAL_size_t(17, app_format_hello(&o, buf, sizeof buf));
  TEST_ASSERT_EQUAL_STRING("HELLO jdoe-1 recv", buf);

  TEST_ASSERT_EQUAL_INT(0, parse(&o, 10, "myapp", "send", "-s", "jdoe-1", "-l", "0.1",
                                 "-c", "0.05", "h", "f"));
  app_format_hello(&o, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("HELLO jdoe-1 send 0.1 0.05 0", buf);

  TEST_ASSERT_EQUAL_INT(0, parse(&o, 8, "myapp", "send", "-s", "a", "-d", ".5", "h", "f"));
  app_format_hello(&o, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("HELLO a send 0 0 0.5", buf);

  /* does not fit */
  TEST_ASSERT_EQUAL_size_t(0, app_format_hello(&o, buf, 5));
}

void test_parse_reply(void)
{
  char reason[64];
  TEST_ASSERT_EQUAL_INT(0, app_parse_reply((const uint8_t *)"OK", 2, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(0, app_parse_reply((const uint8_t *)"OK\n", 3, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR no receiver", 15, reason,
                                           sizeof reason));
  TEST_ASSERT_EQUAL_STRING("no receiver", reason);
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR", 3, reason, sizeof reason));
  TEST_ASSERT_EQUAL_STRING("", reason);
  /* reason is truncated to fit */
  char small[5];
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR session in use", 18, small,
                                           sizeof small));
  TEST_ASSERT_EQUAL_STRING("sess", small);
  TEST_ASSERT_EQUAL_INT(1, app_parse_reply((const uint8_t *)"ERR x", 5, NULL, 0));
  /* anything else, including a stray data packet, is not a reply */
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply((const uint8_t *)"OKAY", 4, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply((const uint8_t *)"ERROR", 5, reason, sizeof reason));
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply((const uint8_t *)"", 0, reason, sizeof reason));
  uint8_t ack[RDT_MAX_PACKET];
  rdt_packet p;
  rdt_make_ack(&p, 3);
  size_t n = rdt_encode(&p, ack, sizeof ack);
  TEST_ASSERT_EQUAL_INT(-1, app_parse_reply(ack, n, reason, sizeof reason));
}

int main(void)
{
  devnull = fopen("/dev/null", "w");
  UNITY_BEGIN();
  RUN_TEST(test_checksum_rfc1071_example);
  RUN_TEST(test_checksum_odd_length_pads_with_zero);
  RUN_TEST(test_checksum_folds_carry);
  RUN_TEST(test_checksum_empty);
  RUN_TEST(test_checksum_catches_every_single_bit_flip);
  RUN_TEST(test_encode_worked_example_hi);
  RUN_TEST(test_encode_worked_example_ack3);
  RUN_TEST(test_make_ack);
  RUN_TEST(test_encode_full_payload_and_large_seq);
  RUN_TEST(test_encode_rejects_bad_input);
  RUN_TEST(test_decode_round_trip);
  RUN_TEST(test_decode_too_short);
  RUN_TEST(test_decode_length_mismatch);
  RUN_TEST(test_decode_length_field_over_max);
  RUN_TEST(test_decode_unknown_type_and_reserved);
  RUN_TEST(test_decode_bad_checksum);
  RUN_TEST(test_sender_init_counts_packets);
  RUN_TEST(test_sender_init_rejects_bad_args);
  RUN_TEST(test_sender_packet_cuts_file);
  RUN_TEST(test_sender_exact_multiple_of_1024);
  RUN_TEST(test_sender_empty_file);
  RUN_TEST(test_sender_window_full);
  RUN_TEST(test_sender_stop_and_wait);
  RUN_TEST(test_sender_cumulative_ack_slides_several);
  RUN_TEST(test_sender_duplicate_ack_ignored);
  RUN_TEST(test_sender_ack_beyond_next_ignored);
  RUN_TEST(test_sender_timeout_resends_whole_window);
  RUN_TEST(test_sender_gives_up_after_ten_timeouts);
  RUN_TEST(test_sender_progress_resets_timeout_count);
  RUN_TEST(test_sender_timer_stops_when_all_acked);
  RUN_TEST(test_receiver_in_order);
  RUN_TEST(test_receiver_duplicate);
  RUN_TEST(test_receiver_beyond_gap);
  RUN_TEST(test_receiver_fin_and_repeated_fin);
  RUN_TEST(test_receiver_empty_file);
  RUN_TEST(test_receiver_ignores_ack);
  RUN_TEST(test_receiver_linger_then_done);
  RUN_TEST(test_receiver_idle_gives_up);
  RUN_TEST(test_transfer_clean_channel);
  RUN_TEST(test_transfer_lossy_channel_many_seeds);
  RUN_TEST(test_transfer_lossy_channel_windows_and_sizes);
  RUN_TEST(test_transfer_dead_channel_gives_up);
  RUN_TEST(test_usage_prints);
  RUN_TEST(test_parse_args_send_defaults);
  RUN_TEST(test_parse_args_send_all_options);
  RUN_TEST(test_parse_args_recv);
  RUN_TEST(test_parse_args_errors);
  RUN_TEST(test_valid_session);
  RUN_TEST(test_format_hello);
  RUN_TEST(test_parse_reply);
  int rv = UNITY_END();
  fclose(devnull);
  return rv;
}
