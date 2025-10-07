#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/packetbuf.h"
#include "sys/energest.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h> // For PRIu64

#define SEND_INTERVAL (CLOCK_SECOND * 5)
#define RETRY_INTERVAL (CLOCK_SECOND * 2)
#define MAX_RETRIES 3
#define RSSI_THRESHOLD -85

typedef struct {
    linkaddr_t source;
    linkaddr_t dest;
    uint8_t seq_num;
    uint8_t ack;
    uint8_t relay_flag;
    char payload[50];
} custom_packet_t;

static custom_packet_t packet;
static uint8_t ack_received = 0;
static uint8_t retries;

PROCESS(source_process, "Source Node");
AUTOSTART_PROCESSES(&source_process);

static void log_energy() {
    energest_flush();
    printf("Node A Energy - CPU: %" PRIu64 ", TX: %" PRIu64 ", RX: %" PRIu64 ", LPM: %" PRIu64 "\n",
           energest_type_time(ENERGEST_TYPE_CPU),
           energest_type_time(ENERGEST_TYPE_TRANSMIT),
           energest_type_time(ENERGEST_TYPE_LISTEN),
           energest_type_time(ENERGEST_TYPE_LPM));
}

static void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    if (len == sizeof(custom_packet_t)) {
        custom_packet_t *ack_packet = (custom_packet_t *)data;
        if (ack_packet->ack && ack_packet->seq_num == packet.seq_num) {
            printf("Node A: Received ACK for packet %d from %d.%d\n", ack_packet->seq_num, src->u8[0], src->u8[1]);
            ack_received = 1;
        }
    }
}

PROCESS_THREAD(source_process, ev, data) {
    static struct etimer send_timer, retry_timer;
    linkaddr_t relay = { .u8 = {2, 0} };
    linkaddr_t sink = { .u8 = {3, 0} };

    PROCESS_BEGIN();

    nullnet_set_input_callback(input_callback);
    energest_init();
    printf("Node A: Starting source process\n");

    while (1) {
        etimer_set(&send_timer, SEND_INTERVAL);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));

        retries = 0;
        ack_received = 0;

        // Prepare packet
        packet.seq_num++;
        packet.ack = 0;
        packet.relay_flag = 0;
        linkaddr_copy(&packet.source, &linkaddr_node_addr);
        linkaddr_copy(&packet.dest, &sink);
        snprintf(packet.payload, sizeof(packet.payload), "Payload %d", packet.seq_num);

        printf("Node A: Sending packet %d to Sink\n", packet.seq_num);

        while (!ack_received && retries < MAX_RETRIES) {
            nullnet_buf = (uint8_t *)&packet;
            nullnet_len = sizeof(packet);
            NETSTACK_NETWORK.output(&sink);

            printf("Node A: Retry %d for packet %d\n", retries + 1, packet.seq_num);
            retries++;

            etimer_set(&retry_timer, RETRY_INTERVAL);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&retry_timer));
        }

        if (!ack_received) {
            printf("Node A: No ACK for packet %d. Activating relay node.\n", packet.seq_num);
            packet.relay_flag = 1;
            nullnet_buf = (uint8_t *)&packet;
            nullnet_len = sizeof(packet);
            NETSTACK_NETWORK.output(&relay);
        }

        log_energy();
    }

    PROCESS_END();
}
