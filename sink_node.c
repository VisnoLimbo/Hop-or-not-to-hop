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

PROCESS(sink_process, "Sink Node");
AUTOSTART_PROCESSES(&sink_process);

static void log_energy() {
    energest_flush();
    printf("Node C Energy - CPU: %" PRIu64 ", TX: %" PRIu64 ", RX: %" PRIu64 ", LPM: %" PRIu64 "\n",
           energest_type_time(ENERGEST_TYPE_CPU),
           energest_type_time(ENERGEST_TYPE_TRANSMIT),
           energest_type_time(ENERGEST_TYPE_LISTEN),
           energest_type_time(ENERGEST_TYPE_LPM));
}

static void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    if (len == sizeof(custom_packet_t)) {
        memcpy(&packet, data, len);
        printf("Node C: Received packet %d from %d.%d\n", packet.seq_num, src->u8[0], src->u8[1]);

        // Send ACK back to sender
        custom_packet_t ack_packet;
        memset(&ack_packet, 0, sizeof(ack_packet));
        ack_packet.seq_num = packet.seq_num;
        ack_packet.ack = 1;
        linkaddr_copy(&ack_packet.source, &linkaddr_node_addr);
        nullnet_buf = (uint8_t *)&ack_packet;
        nullnet_len = sizeof(ack_packet);
        NETSTACK_NETWORK.output(src);

        printf("Node C: Sent ACK for packet %d\n", packet.seq_num);
        log_energy();
    }
}

PROCESS_THREAD(sink_process, ev, data) {
    PROCESS_BEGIN();

    nullnet_set_input_callback(input_callback);
    energest_init();
    printf("Node C: Sink node initialized\n");

    PROCESS_END();
}
