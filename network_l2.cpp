
#include "pico/cyw43_arch.h"
#include "pico/lwip_nosys.h"

#include "cyw43.h"

#include "lwip/etharp.h"
#include "lwip/ip_addr.h"
#include "lwip/ethip6.h"
#include "lwip/dns.h"
#include "lwip/igmp.h"
#include "lwip/tcpip.h"
#include "lwip/dhcp.h"
#include "netif/ethernet.h"

#include "network_l2.hpp"

#include <NEx000.hpp>

NE2000 card_sta;
NE1000 card_sta2;

#include "config.hpp"

static err_t cyw43_netif_output(struct netif *netif, struct pbuf *p) {

    cyw43_t *self = (cyw43_t*)netif->state;
    int itf = netif->name[1] - '0';
    int ret = cyw43_send_ethernet(self, itf, p->tot_len, (void *)p, true);
    if (ret) {
        return ERR_IF;
    }
    return ERR_OK;
}

#if LWIP_IGMP
static err_t cyw43_netif_update_igmp_mac_filter(struct netif *netif, const ip4_addr_t *group, enum netif_mac_filter_action action) {
    cyw43_t *self = (cyw43_t*)netif->state;
    uint8_t mac[] = { 0x01, 0x00, 0x5e, ip4_addr2(group) & 0x7F, ip4_addr3(group), ip4_addr4(group) };

    if (action != IGMP_ADD_MAC_FILTER && action != IGMP_DEL_MAC_FILTER) {
        return ERR_VAL;
    }

    if (cyw43_wifi_update_multicast_filter(self, mac, action == IGMP_ADD_MAC_FILTER)) {
        return ERR_IF;
    }

    return ERR_OK;
}
#endif

static struct netif cy_netif[2];

static err_t cyw43_netif_init(struct netif *netif) {
    netif->linkoutput = cyw43_netif_output;
    netif->output = etharp_output;
    netif->mtu = 1500;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP;
    cyw43_wifi_get_mac((cyw43_t*)netif->state, netif->name[1] - '0', netif->hwaddr);
    netif->hwaddr_len = sizeof(netif->hwaddr);
    #if LWIP_IGMP
    netif_set_igmp_mac_filter(netif, cyw43_netif_update_igmp_mac_filter);
    #endif
    return ERR_OK;
}

extern "C" void cyw43_cb_tcpip_init(cyw43_t *self, int itf) {
    struct netif *n = &cy_netif[itf];
    n->name[0] = 'w';
    n->name[1] = '0' + itf;

    if (itf == CYW43_ITF_STA) {

        netif_input_fn input_func = ethernet_input;

        netif_add(n,
        		(ip4_addr_t *)&Config::WIFI_IP::val.ival,
				(ip4_addr_t *)&Config::WIFI_MASK::val.ival,
				(ip4_addr_t *)&Config::WIFI_GW::val.ival,
        		self, cyw43_netif_init, input_func);

        netif_set_default(n);
        netif_set_up(n);



        netif_set_hostname(n, Config::WIFI_HOSTNAME::val.ival);

        {
        	static ip_addr_t dns;
        	dns = IPADDR4_INIT(Config::WIFI_DNS::val.ival);
        	dns_setserver(0, &dns);
        }
        if(Config::WIFI_DHCP::val.ival)
        {
        	static dhcp dhcp_client;
        	dhcp_set_struct(n, &dhcp_client);
        	dhcp_start(n);
        }
    }
}
extern "C" void cyw43_cb_tcpip_deinit(cyw43_t *self, int itf) {
    struct netif *n = &cy_netif[itf];
	if (itf == CYW43_ITF_STA) {
    	dhcp_stop(n);
    }

    for (struct netif *netif = netif_list; netif != NULL; netif = netif->next) {
        if (netif == n) {
            netif_remove(netif);
            ip_2_ip4(&netif->ip_addr)->addr = 0;
            netif->flags = 0;
        }
    }

}
extern "C" void cyw43_cb_tcpip_set_link_up(cyw43_t *self, int itf) {
    netif_set_link_up(&cy_netif[itf]);
}
extern "C" void cyw43_cb_tcpip_set_link_down(cyw43_t *self, int itf) {
    netif_set_link_down(&cy_netif[itf]);
}
extern "C" void cyw43_cb_process_ethernet(void *cb_data, int itf, size_t len, const uint8_t *buf) {

	//ne200 input
	card_sta.rx_packet(buf,len);
	card_sta2.rx_packet(buf,len);

	cyw43_t *self = (cyw43_t*)cb_data;
    struct netif *netif = &cy_netif[itf];
    if (netif->flags & NETIF_FLAG_LINK_UP) {
        struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
        if (p != NULL) {
            pbuf_take(p, buf, len);
            if (netif->input(p, netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
}

void ne1000_sta_tx(uint8_t* buff, size_t len)
{
	cyw43_send_ethernet(&cyw43_state, CYW43_ITF_STA, len, (void *)buff, false);
}

void network_init()
{
	cyw43_arch_init_with_country(CYW43_COUNTRY_WORLDWIDE); //todo configurize country
	async_context * actx = cyw43_arch_async_context();
	lwip_nosys_init(actx);

	cyw43_arch_enable_sta_mode();
	cyw43_wifi_pm(&cyw43_state, CYW43_DEFAULT_PM & ~0xf);

	uint8_t sta_mac[6];
	cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, sta_mac);
	card_sta.Connect_ISA(0x300,IRQ_Create_Handle(3),ne1000_sta_tx,sta_mac);
	card_sta2.Connect_ISA(0x200,IRQ_Create_Handle(5),ne1000_sta_tx,sta_mac);

}

void network_poll()
{
	cyw43_arch_poll();

	card_sta.update_polled_state();
	card_sta2.update_polled_state();

	const char * ssid = Config::WIFI_SSID::val.ival;
	const char * pw = Config::WIFI_PW::val.ival;

	if(Config::WIFI_CONNECTED::val.ival != CYW43_LINK_JOIN)
	{
	    if (cyw43_state.wifi_join_state)
	        cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);

		std::size_t pw_len = strlen(pw);

		uint32_t auth = pw_len ? CYW43_AUTH_WPA2_AES_PSK : CYW43_AUTH_OPEN;
	    cyw43_wifi_join(&cyw43_state, strlen(ssid), (const uint8_t *)ssid, pw_len, (const uint8_t *)pw, auth, nullptr, CYW43_CHANNEL_NONE);
	}
	int status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
	Config::WIFI_CONNECTED::val.ival = status;
}
