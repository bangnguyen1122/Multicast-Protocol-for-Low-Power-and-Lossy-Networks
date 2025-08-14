#include "contiki.h"
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ipv6/multicast/uip-mcast6.h"

#include <string.h>
#include <stdio.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define MCAST_SINK_UDP_PORT 3001 /* host byte order */
#define MAX_PACKETS 35

static uint16_t total_received = 0;
static uint16_t duplicates = 0;
static uint16_t consecutive_duplicates = 0;
static uint32_t received_ids[MAX_PACKETS];
static uint16_t unique_count = 0;

static int is_duplicate(uint32_t id) {
  int i; 
  for(i = 0; i < unique_count; i++) {
    if(received_ids[i] == id) {
      return 1; /* duplicate */
    }
  }
  return 0;
}

static struct uip_udp_conn *sink_conn;
static uint16_t count;

#define UIP_IP_BUF   ((struct uip_ip_hdr *)&uip_buf[UIP_LLH_LEN])

#if !NETSTACK_CONF_WITH_IPV6 || !UIP_CONF_ROUTER || !UIP_CONF_IPV6_MULTICAST || !UIP_CONF_IPV6_RPL
#error "This example can not work with the current contiki configuration"
#error "Check the values of: NETSTACK_CONF_WITH_IPV6, UIP_CONF_ROUTER, UIP_CONF_IPV6_RPL"
#endif
/*---------------------------------------------------------------------------*/
PROCESS(mcast_sink_process, "Multicast Sink");
AUTOSTART_PROCESSES(&mcast_sink_process);
/*---------------------------------------------------------------------------*/
static void tcpip_handler(void) {
  if(uip_newdata()) {
    uint32_t id = uip_ntohl(*((uint32_t *)(uip_appdata)));
    total_received++;

    if(is_duplicate(id)) {
      duplicates++;
      consecutive_duplicates++;
    } else {
      received_ids[unique_count++] = id;
      consecutive_duplicates = 0; /* reset when encountering new package */
    }

    printf("Received: [0x%08lx], TTL %u, total %u, duplicates %u, consecutive duplicates %u\n",
        (unsigned long)id,
        UIP_IP_BUF->ttl,
        total_received,
        duplicates,
        consecutive_duplicates);

    /* calculate efficiency and print it out now */
    unsigned int efficiency_pct = 0;
    if(total_received > 0) {
      efficiency_pct = (unsigned int)(((total_received - duplicates) * 100u + total_received / 2u) / total_received);
    }

    printf("Sink: Total received: %u, Duplicates: %u, Efficiency: %u%%\n",
           total_received, duplicates, efficiency_pct);

    /* check threshold MAX_PACKETS */
    if(unique_count >= MAX_PACKETS) {
      printf("Exiting process gracefully...\n");
      process_exit(&mcast_sink_process);
    }
  }
}


/*---------------------------------------------------------------------------*/
static uip_ds6_maddr_t *join_mcast_group(void) {
  uip_ipaddr_t addr;
  uip_ds6_maddr_t *rv;

  /* First, set our v6 global */
  uip_ip6addr(&addr, 0xaaaa, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&addr, &uip_lladdr);
  uip_ds6_addr_add(&addr, 0, ADDR_AUTOCONF);

  /*
   * IPHC will use stateless multicast compression for this destination
   * (M=1, DAC=0), with 32 inline bits (1E 89 AB CD)
   */
  uip_ip6addr(&addr, 0xFF1E,0,0,0,0,0,0x89,0xABCD);
  rv = uip_ds6_maddr_add(&addr);

  if(rv) {
    PRINTF("Joined multicast group ");
    PRINT6ADDR(&uip_ds6_maddr_lookup(&addr)->ipaddr);
    PRINTF("\n");
  }
  return rv;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mcast_sink_process, ev, data) {
  PROCESS_BEGIN();

  PRINTF("Multicast Engine: '%s'\n", UIP_MCAST6.name);

  if(join_mcast_group() == NULL) {
    PRINTF("Failed to join multicast group\n");
    PROCESS_EXIT();
  }

  count = 0;

  sink_conn = udp_new(NULL, UIP_HTONS(0), NULL);
  udp_bind(sink_conn, UIP_HTONS(MCAST_SINK_UDP_PORT));

  PRINTF("Listening: ");
  PRINT6ADDR(&sink_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
        UIP_HTONS(sink_conn->lport), UIP_HTONS(sink_conn->rport));

  while(1) {
    PROCESS_YIELD();
    if(ev == tcpip_event) {
      tcpip_handler();
    }
  }

  PROCESS_END();
}
