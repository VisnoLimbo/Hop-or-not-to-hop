#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/energest.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

typedef struct {
    linkaddr_t source;
    linkaddr_t dest;
    uint8_t seq_num;
    uint8_t ack;
    uint8_t relay_flag;
    char payload[50];
} custom_packet_t;

static custom_packet_t packet;

PROCESS(relay_process, "Relay Node");
AUTOSTART_PROCESSES(&relay_process);

static void log_energy() {
    energest_flush();
    printf("Node B Energy - CPU: %" PRIu64 ", TX: %" PRIu64 ", RX: %" PRIu64 ", LPM: %" PRIu64 "\n",
           energest_type_time(ENERGEST_TYPE_CPU),
           energest_type_time(ENERGEST_TYPE_TRANSMIT),
           energest_type_time(ENERGEST_TYPE_LISTEN),
           energest_type_time(ENERGEST_TYPE_LPM));
}

static void send_ack(const linkaddr_t *dest, uint8_t seq_num) {
    custom_packet_t ack_packet;
    memset(&ack_packet, 0, sizeof(ack_packet));
    ack_packet.seq_num = seq_num;
    ack_packet.ack = 1;
    linkaddr_copy(&ack_packet.source, &linkaddr_node_addr);
    nullnet_buf = (uint8_t *)&ack_packet;
    nullnet_len = sizeof(ack_packet);
    NETSTACK_NETWORK.output(dest);

    printf("Node B: Sent ACK for packet %d\n", seq_num);
    log_energy();
}

static void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    if (len == sizeof(custom_packet_t)) {
        memcpy(&packet, data, len);
        if (packet.relay_flag) {
            printf("Node B: Relaying packet %d to Sink\n", packet.seq_num);
            NETSTACK_NETWORK.output(&packet.dest);
            send_ack(src, packet.seq_num);
        }
    }
}

PROCESS_THREAD(relay_process, ev, data) {
    PROCESS_BEGIN();

    nullnet_set_input_callback(input_callback);
    energest_init();
    printf("Node B: Relay node initialized\n");

    PROCESS_END();
}
