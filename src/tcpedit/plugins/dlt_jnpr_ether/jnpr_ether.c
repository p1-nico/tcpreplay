/* $Id$ */

/*
 *   Copyright (c) 2001-2010 Aaron Turner <aturner at synfin dot net>
 *
 *   The Tcpreplay Suite of tools is free software: you can redistribute it 
 *   and/or modify it under the terms of the GNU General Public License as 
 *   published by the Free Software Foundation, either version 3 of the 
 *   License, or with the authors permission any later version.
 *
 *   The Tcpreplay Suite is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with the Tcpreplay Suite.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include "tcpedit.h"
#include "common.h"
#include "tcpr.h"
#include "dlt_utils.h"
#include "tcpedit_stub.h"
#include "../ethernet.h"
#include "jnpr_ether.h"
#include "../dlt_en10mb/en10mb.h"

static char dlt_name[] = "jnpr_eth";
static char dlt_prefix[] = "jnpr_ether";
static uint16_t dlt_value = DLT_JUNIPER_ETHER;

/**
 * Function to register ourselves.  This function is always called, regardless
 * of what DLT types are being used, so it shouldn't be allocating extra buffers
 * or anything like that (use the dlt_jnpr_ether_init() function below for that).
 * Tasks:
 * - Create a new plugin struct
 * - Fill out the provides/requires bit masks.  Note:  Only specify which fields are
 *   actually in the header.
 * - Add the plugin to the context's plugin chain
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_jnpr_ether_register(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    assert(ctx);

    /* create  a new plugin structure */
    plugin = tcpedit_dlt_newplugin();

    plugin->provides += PLUGIN_MASK_PROTO + PLUGIN_MASK_SRCADDR + PLUGIN_MASK_DSTADDR;
    plugin->requires = 0;

     /* what is our DLT value? */
    plugin->dlt = dlt_value;

    /* set the prefix name of our plugin.  This is also used as the prefix for our options */
    plugin->name = safe_strdup(dlt_name);

    /* 
     * Point to our functions, note, you need a function for EVERY method.  
     * Even if it is only an empty stub returning success.
     */
    plugin->plugin_init = dlt_jnpr_ether_init;
    plugin->plugin_post_init = dlt_jnpr_ether_post_init;
    plugin->plugin_cleanup = dlt_jnpr_ether_cleanup;
    plugin->plugin_parse_opts = dlt_jnpr_ether_parse_opts;
    plugin->plugin_decode = dlt_jnpr_ether_decode;
    plugin->plugin_encode = dlt_jnpr_ether_encode;
    plugin->plugin_proto = dlt_jnpr_ether_proto;
    plugin->plugin_l2addr_type = dlt_jnpr_ether_l2addr_type;
    plugin->plugin_l2len = dlt_jnpr_ether_l2len;
    plugin->plugin_get_layer3 = dlt_jnpr_ether_get_layer3;
    plugin->plugin_merge_layer3 = dlt_jnpr_ether_merge_layer3;
    plugin->plugin_get_mac = dlt_jnpr_ether_get_mac;
    /* add it to the available plugin list */
    return tcpedit_dlt_addplugin(ctx, plugin);
}

 
/**
 * Initializer function.  This function is called only once, if and only iif
 * this plugin will be utilized.  Remember, if you need to keep track of any state, 
 * store it in your plugin->config, not a global!
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_jnpr_ether_init(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    jnpr_ether_config_t *config;
    assert(ctx);
    
    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to initalize unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }
    
    /* allocate memory for our deocde extra data */
    if (sizeof(jnpr_ether_extra_t) > 0)
        ctx->decoded_extra = safe_malloc(sizeof(jnpr_ether_extra_t));

    /* allocate memory for our config data */
    if (sizeof(jnpr_ether_config_t) > 0)
        plugin->config = safe_malloc(sizeof(jnpr_ether_config_t));
    
    config = (jnpr_ether_config_t *)plugin->config;    
    
    return TCPEDIT_OK; /* success */
}

/**
 * Post init function.  This function is called only once after init() and parse_opts()
 * It basically allows decoders to properly initialize sub-plugins.
 */
int 
dlt_jnpr_ether_post_init(tcpeditdlt_t *ctx)
{
    jnpr_ether_config_t *config;
    
    /* do nothing if we're not the decoder */
    if (ctx->decoder->dlt != dlt_value)
        return TCPEDIT_OK;
    
    /* init our subcontext & decoder of en10mb */
    config = (jnpr_ether_config_t *)ctx->encoder->config;
    config->subctx = tcpedit_dlt_init(ctx->tcpedit, DLT_EN10MB);
        
    return TCPEDIT_OK;
}

/**
 * Since this is used in a library, we should manually clean up after ourselves
 * Unless you allocated some memory in dlt_jnpr_ether_init(), this is just an stub.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_jnpr_ether_cleanup(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    jnpr_ether_config_t *config;
    
    assert(ctx);

    
    
    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to cleanup unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }

    if (ctx->decoded_extra != NULL) {
        safe_free(ctx->decoded_extra);
        ctx->decoded_extra = NULL;
    }
    
    if (plugin->config != NULL) {
        /* clean up the en10mb plugin */
        config = (jnpr_ether_config_t *)ctx->encoder->config;
        tcpedit_dlt_cleanup(config->subctx);
        plugin->config = NULL;
    }

    return TCPEDIT_OK; /* success */
}

/**
 * This is where you should define all your AutoGen AutoOpts option parsing.
 * Any user specified option should have it's bit turned on in the 'provides'
 * bit mask.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_jnpr_ether_parse_opts(tcpeditdlt_t *ctx)
{
    assert(ctx);

    return TCPEDIT_OK; /* success */
}

/**
 * Function to decode the layer 2 header in the packet.
 * You need to fill out:
 * - ctx->l2len
 * - ctx->srcaddr
 * - ctx->dstaddr
 * - ctx->proto
 * - ctx->decoded_extra
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_jnpr_ether_decode(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    int jnpr_header_len = 0;
    const u_char *ethernet = NULL;
    jnpr_ether_config_t *config;
    
    
    assert(ctx);
    assert(packet);
    assert(pktlen > JUNIPER_ETHER_HEADER_LEN); /* MAGIC + Static fields + Extension Length */

    config = (jnpr_ether_config_t *)ctx->encoder->config;

    
    /* first, verify magic */
    if (memcmp(packet, JUNIPER_ETHER_MAGIC, JUNIPER_ETHER_MAGIC_LEN) != 0) {
        tcpedit_seterr(ctx->tcpedit, "Invalid magic 0x%02X%02X%02X", 
            packet[0], packet[1], packet[2]);
        return TCPEDIT_ERROR;
    }
    
    /* next make sure the L2 header is present */
    if ((packet[JUNIPER_ETHER_OPTIONS_OFFSET] & JUNIPER_ETHER_L2PRESENT) 
            != JUNIPER_ETHER_L2PRESENT) {
        tcpedit_seterr(ctx->tcpedit, "Frame is missing L2 Header: %x", 
            packet[JUNIPER_ETHER_OPTIONS_OFFSET]);
        return TCPEDIT_ERROR;
    }
    
    /* then get the Juniper header length */
    memcpy(&jnpr_header_len, &packet[JUNIPER_ETHER_EXTLEN_OFFSET], 2);
    
    jnpr_header_len = ntohs(jnpr_header_len) + JUNIPER_ETHER_HEADER_LEN;

    dbgx(1, "jnpr header len: %d", jnpr_header_len);
    /* make sure the packet is big enough to find the Ethernet Header */

    if (pktlen < jnpr_header_len + TCPR_ETH_H) {
        tcpedit_seterr(ctx->tcpedit, "Frame is too short! %d < %d", 
            pktlen, (jnpr_header_len + TCPR_ETH_H));
        return TCPEDIT_ERROR;
    }
    
    ctx->l2len = jnpr_header_len;
    
    /* jump to the appropriate offset */
    ethernet = packet + jnpr_header_len;

    /* let the en10mb plugin decode the rest */
    if (tcpedit_dlt_decode(config->subctx, ethernet, (pktlen - jnpr_header_len)) == TCPEDIT_ERROR)
        return TCPEDIT_ERROR;

    /* copy the subdecoder state to our encoder state */
    if (tcpedit_dlt_copy_decoder_state(ctx, config->subctx) == TCPEDIT_ERROR)
        return TCPEDIT_ERROR;
    
    return TCPEDIT_OK;
}

/**
 * Function to encode the layer 2 header back into the packet.
 * Returns: total packet len or TCPEDIT_ERROR
 */
int 
dlt_jnpr_ether_encode(tcpeditdlt_t *ctx, u_char *packet, int pktlen, _U_ tcpr_dir_t dir)
{
    assert(ctx);
    assert(pktlen > JUNIPER_ETHER_HEADER_LEN); /* MAGIC + Static fields + Extension Length */
    assert(packet);
    
    tcpedit_seterr(ctx->tcpedit, "%s", "DLT_JUNIPER_ETHER plugin does not support packet encoding");
    return TCPEDIT_ERROR;
}

/**
 * Function returns the Layer 3 protocol type of the given packet, or TCPEDIT_ERROR on error
 * Make sure you return this value in NETWORK byte order!
 */
int 
dlt_jnpr_ether_proto(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    int jnpr_hdr_len;
    const u_char *ethernet;
    jnpr_ether_config_t *config;

    assert(ctx);
    assert(packet);
    assert(pktlen > JUNIPER_ETHER_HEADER_LEN); /* MAGIC + Static fields + Extension Length */

    config = (jnpr_ether_config_t *)ctx->encoder->config;


    /* next make sure the L2 header is present */
    if ((packet[JUNIPER_ETHER_OPTIONS_OFFSET] & JUNIPER_ETHER_L2PRESENT) 
            != JUNIPER_ETHER_L2PRESENT) {
        tcpedit_seterr(ctx->tcpedit, "Frame is missing L2 Header: %x", 
            packet[JUNIPER_ETHER_OPTIONS_OFFSET]);
        return TCPEDIT_ERROR;
    }

    /* then get the Juniper header length */
    memcpy(&jnpr_hdr_len, &packet[JUNIPER_ETHER_EXTLEN_OFFSET], 2);
    
    jnpr_hdr_len = ntohs(jnpr_hdr_len) + JUNIPER_ETHER_HEADER_LEN;
    ethernet = packet + jnpr_hdr_len;
    
    /* let the en10mb plugin do the rest of the work */
    return tcpedit_dlt_proto(config->subctx, DLT_EN10MB, ethernet, (pktlen - jnpr_hdr_len));
}

/**
 * Function returns a pointer to the layer 3 protocol header or NULL on error
 */
u_char *
dlt_jnpr_ether_get_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen)
{
    int l2len;
    assert(ctx);
    assert(packet);

    /* next make sure the L2 header is present */
    if ((packet[JUNIPER_ETHER_OPTIONS_OFFSET] & JUNIPER_ETHER_L2PRESENT) 
            != JUNIPER_ETHER_L2PRESENT) {
        tcpedit_seterr(ctx->tcpedit, "Frame is missing L2 Header: %x", 
            packet[JUNIPER_ETHER_OPTIONS_OFFSET]);
        return NULL;
    }

    l2len = dlt_jnpr_ether_l2len(ctx, packet, pktlen);

    assert(pktlen >= l2len);

    return tcpedit_dlt_l3data_copy(ctx, packet, pktlen, l2len);
}

/**
 * function merges the packet (containing L2 and old L3) with the l3data buffer
 * containing the new l3 data.  Note, if L2 % 4 == 0, then they're pointing to the
 * same buffer, otherwise there was a memcpy involved on strictly aligned architectures
 * like SPARC
 */
u_char *
dlt_jnpr_ether_merge_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen, u_char *l3data)
{
    int l2len;
    assert(ctx);
    assert(packet);
    assert(l3data);
    
    l2len = dlt_jnpr_ether_l2len(ctx, packet, pktlen);
    
    assert(pktlen >= l2len);
    
    return tcpedit_dlt_l3data_merge(ctx, packet, pktlen, l3data, l2len);
}

/**
 * return a static pointer to the source/destination MAC address
 * return NULL on error/address doesn't exist
 */    
u_char *
dlt_jnpr_ether_get_mac(tcpeditdlt_t *ctx, tcpeditdlt_mac_type_t mac, const u_char *packet, const int pktlen)
{
    const u_char *ethernet = NULL;
    jnpr_ether_config_t *config;
    int jnpr_hdr_len = 0;

    assert(ctx);
    assert(packet);
    assert(pktlen);

    config = (jnpr_ether_config_t *)ctx->encoder->config;

    /* first get the Juniper header length */
    memcpy(&jnpr_hdr_len, &packet[JUNIPER_ETHER_EXTLEN_OFFSET], 2);
    
    jnpr_hdr_len = ntohs(jnpr_hdr_len) + JUNIPER_ETHER_HEADER_LEN;
    ethernet = packet + jnpr_hdr_len;
    
    return dlt_en10mb_get_mac(config->subctx, mac, ethernet, (pktlen - jnpr_hdr_len));
}


/**
 * return the length of the L2 header of the current packet
 */
int
dlt_jnpr_ether_l2len(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    uint16_t len;
    jnpr_ether_config_t *config;

    assert(ctx);
    assert(packet);
    assert(pktlen);
    
    config = (jnpr_ether_config_t *)ctx->encoder->config;

    /* first get the Juniper header length */
    memcpy(&len, &packet[JUNIPER_ETHER_EXTLEN_OFFSET], 2);
    
    len = ntohs(len) + JUNIPER_ETHER_HEADER_LEN;
    dbgx(3, "juniper header len: %u", len);
    
    /* add the 802.3 length */
    len += tcpedit_dlt_l2len(config->subctx, DLT_EN10MB, (packet + len), (pktlen - len));
    dbgx(3, "total l2len: %u", len);
    
    /* and return that */
    return len;
}


tcpeditdlt_l2addr_type_t 
dlt_jnpr_ether_l2addr_type(void)
{
    return ETHERNET;
}
