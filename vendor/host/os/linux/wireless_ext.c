//------------------------------------------------------------------------------
// <copyright file="wireless_ext.c" company="Atheros">
//    Copyright (c) 2004-2009 Atheros Corporation.  All rights reserved.
//    Copyright (C) 2010 Sony Ericsson Mobile Communications AB
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// Software distributed under the License is distributed on an "AS
// IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
// implied. See the License for the specific language governing
// rights and limitations under the License.
//
//
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

#include "ar6000_drv.h"

static A_UINT8 bcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static void ar6000_set_quality(struct iw_quality *iq, A_INT8 rssi);
extern unsigned int wmitimeout;
extern A_WAITQUEUE_HEAD arEvent;


#if WIRELESS_EXT > 14
/*
 * Encode a WPA or RSN information element as a custom
 * element using the hostap format.
 */
static u_int
encode_ie(void *buf, size_t bufsize,
    const u_int8_t *ie, size_t ielen,
    const char *leader, size_t leader_len)
{
    u_int8_t *p;
    int i;

    if (bufsize < leader_len)
        return 0;
    p = buf;
    memcpy(p, leader, leader_len);
    bufsize -= leader_len;
    p += leader_len;
    for (i = 0; i < ielen && bufsize > 2; i++)
    {
        p += sprintf(p, "%02x", ie[i]);
        bufsize -= 2;
    }
    return (i == ielen ? p - (u_int8_t *)buf : 0);
}
#endif /* WIRELESS_EXT > 14 */

void
ar6000_scan_node(void *arg, bss_t *ni)
{
    struct iw_event iwe;
#if WIRELESS_EXT > 14
    char buf[256];
#endif
    struct ar_giwscan_param *param;
    A_CHAR *current_ev;
    A_CHAR *end_buf;
    struct ieee80211_common_ie  *cie;
    A_CHAR *current_val;
    A_INT32 j;
    A_UINT32 rate_len, data_len = 0;

    param = (struct ar_giwscan_param *)arg;

    current_ev = param->current_ev;
    end_buf = param->end_buf;

    cie = &ni->ni_cie;

    if ((end_buf - current_ev) > IW_EV_ADDR_LEN)
    {
        A_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = SIOCGIWAP;
        iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
        A_MEMCPY(iwe.u.ap_addr.sa_data, ni->ni_macaddr, 6);
        current_ev = iwe_stream_add_event(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe,
                          IW_EV_ADDR_LEN);
    }
    param->bytes_needed += IW_EV_ADDR_LEN;

    data_len = cie->ie_ssid[1] + IW_EV_POINT_LEN;
    if ((end_buf - current_ev) > data_len)
    {
        A_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = SIOCGIWESSID;
        iwe.u.data.flags = 1;
        iwe.u.data.length = cie->ie_ssid[1];
        current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe,
                          &cie->ie_ssid[2]);
    }
    param->bytes_needed += data_len;

    if (cie->ie_capInfo & (IEEE80211_CAPINFO_ESS|IEEE80211_CAPINFO_IBSS)) {
        if ((end_buf - current_ev) > IW_EV_UINT_LEN)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = SIOCGIWMODE;
            iwe.u.mode = cie->ie_capInfo & IEEE80211_CAPINFO_ESS ?
                         IW_MODE_MASTER : IW_MODE_ADHOC;
            current_ev = iwe_stream_add_event(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe,
                          IW_EV_UINT_LEN);
        }
        param->bytes_needed += IW_EV_UINT_LEN;
    }

    if ((end_buf - current_ev) > IW_EV_FREQ_LEN)
    {
        A_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = SIOCGIWFREQ;
        iwe.u.freq.m = cie->ie_chan * 100000;
        iwe.u.freq.e = 1;
        current_ev = iwe_stream_add_event(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe,
                          IW_EV_FREQ_LEN);
    }
    param->bytes_needed += IW_EV_FREQ_LEN;

    if ((end_buf - current_ev) > IW_EV_QUAL_LEN)
    {
        A_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = IWEVQUAL;
        ar6000_set_quality(&iwe.u.qual, ni->ni_snr);
        current_ev = iwe_stream_add_event(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe,
                          IW_EV_QUAL_LEN);
    }
    param->bytes_needed += IW_EV_QUAL_LEN;

    if ((end_buf - current_ev) > IW_EV_POINT_LEN)
    {
        A_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = SIOCGIWENCODE;
        if (cie->ie_capInfo & IEEE80211_CAPINFO_PRIVACY) {
            iwe.u.data.flags = IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
        } else {
            iwe.u.data.flags = IW_ENCODE_DISABLED;
        }
        iwe.u.data.length = 0;
        current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe, "");
    }
    param->bytes_needed += IW_EV_POINT_LEN;

    /* supported bit rate */
    A_MEMZERO(&iwe, sizeof(iwe));
    iwe.cmd = SIOCGIWRATE;
    iwe.u.bitrate.fixed = 0;
    iwe.u.bitrate.disabled = 0;
    iwe.u.bitrate.value = 0;
    current_val = current_ev + IW_EV_LCP_LEN;
    param->bytes_needed += IW_EV_LCP_LEN;

    if (cie->ie_rates != NULL) {
        rate_len = cie->ie_rates[1];
        data_len = (rate_len * (IW_EV_PARAM_LEN - IW_EV_LCP_LEN));
        if ((end_buf - current_ev) > data_len)
        {
            for (j = 0; j < rate_len; j++) {
                    unsigned char val;
                    val = cie->ie_rates[2 + j];
                    iwe.u.bitrate.value =
                        (val >= 0x80)? ((val - 0x80) * 500000): (val * 500000);
                    current_val = iwe_stream_add_value(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                                      param->info,
#endif
                                      current_ev,
                                      current_val,
                                      end_buf,
                                      &iwe,
                                      IW_EV_PARAM_LEN);
            }
        }
        param->bytes_needed += data_len;
    }

    if (cie->ie_xrates != NULL) {
        rate_len = cie->ie_xrates[1];
        data_len = (rate_len * (IW_EV_PARAM_LEN - IW_EV_LCP_LEN));
        if ((end_buf - current_ev) > data_len)
        {
            for (j = 0; j < rate_len; j++) {
                    unsigned char val;
                    val = cie->ie_xrates[2 + j];
                    iwe.u.bitrate.value =
                        (val >= 0x80)? ((val - 0x80) * 500000): (val * 500000);
                    current_val = iwe_stream_add_value(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                                      param->info,
#endif
                                      current_ev,
                                      current_val,
                                      end_buf,
                                      &iwe,
                                      IW_EV_PARAM_LEN);
            }
        }
        param->bytes_needed += data_len;
    }
    /* remove fixed header if no rates were added */
    if ((current_val - current_ev) > IW_EV_LCP_LEN)
        current_ev = current_val;

#if WIRELESS_EXT >= 18
    /* IE */
    if (cie->ie_wpa != NULL) {
        data_len = cie->ie_wpa[1] + 2 + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVGENIE;
            iwe.u.data.length = cie->ie_wpa[1] + 2;
            current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe, cie->ie_wpa);
        }
        param->bytes_needed += data_len;
    }

    if (cie->ie_rsn != NULL && cie->ie_rsn[0] == IEEE80211_ELEMID_RSN) {
        data_len = cie->ie_rsn[1] + 2 + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVGENIE;
            iwe.u.data.length = cie->ie_rsn[1] + 2;
            current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe, cie->ie_rsn);
        }
        param->bytes_needed += data_len;
    }

#endif /* WIRELESS_EXT >= 18 */

    if ((end_buf - current_ev) > IW_EV_CHAR_LEN)
    {
        /* protocol */
        A_MEMZERO(&iwe, sizeof(iwe));
        iwe.cmd = SIOCGIWNAME;
#define CHAN_IS_11A(x)              (!((x >= 2412) && (x <= 2484)))
        if (CHAN_IS_11A(cie->ie_chan)) {
            /* 11a */
            snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11a");
        } else if ((cie->ie_erp) || (cie->ie_xrates)) {
            /* 11g */
            snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11g");
        } else {
            /* 11b */
            snprintf(iwe.u.name, IFNAMSIZ, "IEEE 802.11b");
        }
        current_ev = iwe_stream_add_event(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe, IW_EV_CHAR_LEN);
    }
    param->bytes_needed += IW_EV_CHAR_LEN;

#if WIRELESS_EXT > 14
    A_MEMZERO(&iwe, sizeof(iwe));
    iwe.cmd = IWEVCUSTOM;
    iwe.u.data.length = snprintf(buf, sizeof(buf), "bcn_int=%d", cie->ie_beaconInt);
    data_len = iwe.u.data.length + IW_EV_POINT_LEN;
    if ((end_buf - current_ev) > data_len)
    {
        current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe, buf);
    }
    param->bytes_needed += data_len;

#if WIRELESS_EXT < 18
    if (cie->ie_wpa != NULL) {
        static const char wpa_leader[] = "wpa_ie=";
        data_len = (sizeof(wpa_leader) - 1) + ((cie->ie_wpa[1]+2) * 2) + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = encode_ie(buf, sizeof(buf), cie->ie_wpa,
                                          cie->ie_wpa[1]+2,
                                          wpa_leader, sizeof(wpa_leader)-1);

            if (iwe.u.data.length != 0) {
                current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                              param->info,
#endif
                              current_ev, end_buf, &iwe, buf);
            }
        }
        param->bytes_needed += data_len;
    }

    if (cie->ie_rsn != NULL && cie->ie_rsn[0] == IEEE80211_ELEMID_RSN) {
        static const char rsn_leader[] = "rsn_ie=";
        data_len = (sizeof(rsn_leader) - 1) + ((cie->ie_rsn[1]+2) * 2) + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = encode_ie(buf, sizeof(buf), cie->ie_rsn,
                                          cie->ie_rsn[1]+2,
                                          rsn_leader, sizeof(rsn_leader)-1);

            if (iwe.u.data.length != 0) {
                current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                              param->info,
#endif
                              current_ev, end_buf, &iwe, buf);
            }
        }
        param->bytes_needed += data_len;
    }
#endif /* WIRELESS_EXT < 18 */

    if (cie->ie_wmm != NULL) {
        static const char wmm_leader[] = "wmm_ie=";
        data_len = (sizeof(wmm_leader) - 1) + ((cie->ie_wmm[1]+2) * 2) + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = encode_ie(buf, sizeof(buf), cie->ie_wmm,
                                          cie->ie_wmm[1]+2,
                                          wmm_leader, sizeof(wmm_leader)-1);
            if (iwe.u.data.length != 0) {
                current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                              param->info,
#endif
                              current_ev, end_buf, &iwe, buf);
            }
        }
        param->bytes_needed += data_len;
    }

    if (cie->ie_ath != NULL) {
        static const char ath_leader[] = "ath_ie=";
        data_len = (sizeof(ath_leader) - 1) + ((cie->ie_ath[1]+2) * 2) + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = encode_ie(buf, sizeof(buf), cie->ie_ath,
                                          cie->ie_ath[1]+2,
                                          ath_leader, sizeof(ath_leader)-1);
            if (iwe.u.data.length != 0) {
                current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                              param->info,
#endif
                              current_ev, end_buf, &iwe, buf);
            }
        }
        param->bytes_needed += data_len;
    }

#ifdef WAPI_ENABLE
    if (cie->ie_wapi != NULL) {
        static const char wapi_leader[] = "wapi_ie=";
        data_len = (sizeof(wapi_leader) - 1) + ((cie->ie_wapi[1] + 2) * 2) + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len) {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = encode_ie(buf, sizeof(buf), cie->ie_wapi,
                                      cie->ie_wapi[1] + 2,
                                      wapi_leader, sizeof(wapi_leader) - 1);
            if (iwe.u.data.length != 0) {
                current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                                              param->info,
#endif
                                              current_ev, end_buf, &iwe, buf);
            }
        }
    }
#endif /* WAPI_ENABLE */

#endif /* WIRELESS_EXT > 14 */

#if WIRELESS_EXT >= 18
    if (cie->ie_wsc != NULL) {
        data_len = (cie->ie_wsc[1] + 2) + IW_EV_POINT_LEN;
        if ((end_buf - current_ev) > data_len)
        {
            A_MEMZERO(&iwe, sizeof(iwe));
            iwe.cmd = IWEVGENIE;
            iwe.u.data.length = cie->ie_wsc[1] + 2;
            current_ev = iwe_stream_add_point(
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
                          param->info,
#endif
                          current_ev, end_buf, &iwe, cie->ie_wsc);
        }
        param->bytes_needed += data_len;
    }
#endif /* WIRELESS_EXT >= 18 */

    param->current_ev = current_ev;
}

int
ar6000_ioctl_giwscan(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_point *data, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    struct ar_giwscan_param param;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    param.current_ev = extra;
    param.end_buf = extra + data->length;
    param.bytes_needed = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
    param.info = info;
#endif

    /* Translate data to WE format */
    wmi_iterate_nodes(ar->arWmi, ar6000_scan_node, &param);

    /* check if bytes needed is greater than bytes consumed */
    if (param.bytes_needed > (param.current_ev - extra))
    {
        /* Request one byte more than needed, because when "data->length" equals bytes_needed,
        it is not possible to add the last event data as all iwe_stream_add_xxxxx() functions
        checks whether (cur_ptr + ev_len) < end_ptr, due to this one more retry would happen*/
        data->length = param.bytes_needed + 1;

        return -E2BIG;
    }

    return 0;
}

extern int reconnect_flag;
/* SIOCSIWESSID */
static int
ar6000_ioctl_siwessid(struct net_device *dev,
                     struct iw_request_info *info,
                     struct iw_point *data, char *ssid)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_STATUS status;
    A_UINT8     arNetworkType;
    A_UINT8 prevMode = ar->arNetworkType;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->bIsDestroyProgress) {
        return -EBUSY;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

#if defined(WIRELESS_EXT)
    if (WIRELESS_EXT >= 20) {
        data->length += 1;
    }
#endif

    /*
     * iwconfig passes a null terminated string with length including this
     * so we need to account for this
     */
    if (data->flags && (!data->length || (data->length == 1) ||
        ((data->length - 1) > sizeof(ar->arSsid))))
    {
        /*
         * ssid is invalid
         */
        return -EINVAL;
    }

    if (ar->arNextMode == AP_NETWORK) {
        /* SSID change for AP network - Will take effect on commit */
        if(A_MEMCMP(ar->arSsid,ssid,32) != 0) {
             ar->arSsidLen = data->length - 1;
            A_MEMCPY(ar->arSsid, ssid, ar->arSsidLen);
            ar->ap_profile_flag = 1; /* There is a change in profile */
        }
        return 0;
    } else if(ar->arNetworkType == AP_NETWORK) {
        A_UINT8 ctr;
        struct sk_buff *skb;

        /* We are switching from AP to STA | IBSS mode, cleanup the AP state */
        for (ctr=0; ctr < AP_MAX_NUM_STA; ctr++) {
            remove_sta(ar, ar->sta_list[ctr].mac, 0);
        }
        A_MUTEX_LOCK(&ar->mcastpsqLock);
        while (!A_NETBUF_QUEUE_EMPTY(&ar->mcastpsq)) {
            skb = A_NETBUF_DEQUEUE(&ar->mcastpsq);
            A_NETBUF_FREE(skb);
        }
        A_MUTEX_UNLOCK(&ar->mcastpsqLock);
    }

    /* Added for bug 25178, return an IOCTL error instead of target returning
       Illegal parameter error when either the BSSID or channel is missing
       and we cannot scan during connect.
     */
    if (data->flags) {
        if (ar->arSkipScan == TRUE &&
            (ar->arChannelHint == 0 ||
             (!ar->arReqBssid[0] && !ar->arReqBssid[1] && !ar->arReqBssid[2] &&
              !ar->arReqBssid[3] && !ar->arReqBssid[4] && !ar->arReqBssid[5])))
        {
            return -EINVAL;
        }
    }

    if (down_interruptible(&ar->arSem)) {
        return -ERESTARTSYS;
    }

    if (ar->bIsDestroyProgress) {
        up(&ar->arSem);
        return -EBUSY;
    }

    if (ar->arTxPending[wmi_get_control_ep(ar->arWmi)]) {
        /*
         * sleep until the command queue drains
         */
        wait_event_interruptible_timeout(arEvent,
            ar->arTxPending[wmi_get_control_ep(ar->arWmi)] == 0, wmitimeout * HZ);
        if (signal_pending(current)) {
            return -EINTR;
        }
    }

    if (!data->flags) {
        arNetworkType = ar->arNetworkType;
        ar6000_init_profile_info(ar);
        ar->arNetworkType = arNetworkType;
    }

    /* Update the arNetworkType */
    ar->arNetworkType = ar->arNextMode;


    if ((prevMode != AP_NETWORK) && ((ar->arSsidLen) || (!data->flags)))
    {
        if ((!data->flags) ||
            (A_MEMCMP(ar->arSsid, ssid, ar->arSsidLen) != 0) ||
            (ar->arSsidLen != (data->length - 1)))
        {
            /*
             * SSID set previously or essid off has been issued.
             *
             * Disconnect Command is issued in two cases after wmi is ready
             * (1) ssid is different from the previous setting
             * (2) essid off has been issued
             *
             */
            if (ar->arWmiReady == TRUE) {
                reconnect_flag = 0;
                status = wmi_setPmkid_cmd(ar->arWmi, ar->arBssid, NULL, 0);
                status = wmi_disconnect_cmd(ar->arWmi);
                A_MEMZERO(ar->arSsid, sizeof(ar->arSsid));
                ar->arSsidLen = 0;
                if (ar->arSkipScan == FALSE) {
                    A_MEMZERO(ar->arReqBssid, sizeof(ar->arReqBssid));
                }
                if (!data->flags) {
                    up(&ar->arSem);
                    return 0;
                }
            } else {
                 up(&ar->arSem);
            }
        }
        else
        {
            /*
             * SSID is same, so we assume profile hasn't changed.
             * If the interface is up and wmi is ready, we issue
             * a reconnect cmd. Issue a reconnect only we are already
             * connected.
             */
            if((ar->arConnected == TRUE) && (ar->arWmiReady == TRUE))
            {
                reconnect_flag = TRUE;
                status = wmi_reconnect_cmd(ar->arWmi,ar->arReqBssid,
                                           ar->arChannelHint);
                up(&ar->arSem);
                if (status != A_OK) {
                    return -EIO;
                }
                return 0;
            }
            else{
                /*
                 * Dont return if connect is pending.
                 */
                if(!(ar->arConnectPending)) {
                    up(&ar->arSem);
                    return 0;
                }
            }
        }
    }

    ar->arSsidLen = data->length - 1;
    A_MEMCPY(ar->arSsid, ssid, ar->arSsidLen);

    if (ar6000_connect_to_ap(ar)!= A_OK) {
        up(&ar->arSem);
        return -EIO; 
    } else {
        up(&ar->arSem);
    }
    return 0;
}

/* SIOCGIWESSID */
static int
ar6000_ioctl_giwessid(struct net_device *dev,
                     struct iw_request_info *info,
                     struct iw_point *data, char *essid)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (!ar->arSsidLen) {
        return -EINVAL;
    }

    data->flags = 1;
    data->length = ar->arSsidLen;
    A_MEMCPY(essid, ar->arSsid, ar->arSsidLen);

    return 0;
}


void ar6000_install_static_wep_keys(AR_SOFTC_T *ar)
{
    A_UINT8 index;
    A_UINT8 keyUsage;

    for (index = WMI_MIN_KEY_INDEX; index <= WMI_MAX_KEY_INDEX; index++) {
        if (ar->arWepKeyList[index].arKeyLen) {
            keyUsage = GROUP_USAGE;
            if (index == ar->arDefTxKeyIndex) {
                keyUsage |= TX_USAGE;
            }
            wmi_addKey_cmd(ar->arWmi,
                           index,
                           WEP_CRYPT,
                           keyUsage,
                           ar->arWepKeyList[index].arKeyLen,
                           NULL,
                           ar->arWepKeyList[index].arKey, KEY_OP_INIT_VAL, NULL,
                           NO_SYNC_WMIFLAG);
        }
    }
}

/*
 * SIOCSIWRATE
 */
int
ar6000_ioctl_siwrate(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_param *rrq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_UINT32  kbps;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (rrq->fixed) {
        kbps = rrq->value / 1000;           /* rrq->value is in bps */
    } else {
        kbps = -1;                          /* -1 indicates auto rate */
    }
    if(kbps != -1 && wmi_validate_bitrate(ar->arWmi, kbps) == A_EINVAL)
    {
        AR_DEBUG_PRINTF("BitRate is not Valid %d\n", kbps);
        return -EINVAL;
    }
    ar->arBitRate = kbps;
    if(ar->arWmiReady == TRUE)
    {
        if (wmi_set_bitrate_cmd(ar->arWmi, kbps, -1, -1) != A_OK) {
            return -EINVAL;
        }
    }
    return 0;
}

/*
 * SIOCGIWRATE
 */
int
ar6000_ioctl_giwrate(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_param *rrq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    int ret = 0;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->bIsDestroyProgress) {
        return -EBUSY;
    }

    if (down_interruptible(&ar->arSem)) {
        return -ERESTARTSYS;
    }

    if (ar->bIsDestroyProgress) {
        up(&ar->arSem);
        return -EBUSY;
    }

    if(ar->arWmiReady == TRUE)
    {
        ar->arBitRate = 0xFFFF;
        if (wmi_get_bitrate_cmd(ar->arWmi) != A_OK) {
            up(&ar->arSem);
            return -EIO;
        }
        wait_event_interruptible_timeout(arEvent, ar->arBitRate != 0xFFFF, wmitimeout * HZ);
        if (signal_pending(current)) {
            ret = -EINTR;
        }
    }
    /* If the interface is down or wmi is not ready or the target is not
       connected - return the value stored in the device structure */
    if (!ret) {
        if (ar->arBitRate == -1) {
            rrq->fixed = TRUE;
            rrq->value = 0;
        } else {
            rrq->value = ar->arBitRate * 1000;
        }
    }

    up(&ar->arSem);

    return ret;
}

/*
 * SIOCSIWTXPOW
 */
static int
ar6000_ioctl_siwtxpow(struct net_device *dev,
             struct iw_request_info *info,
             struct iw_param *rrq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_UINT8 dbM;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (rrq->disabled) {
        return -EOPNOTSUPP;
    }

    if (rrq->fixed) {
        if (rrq->flags != IW_TXPOW_DBM) {
            return -EOPNOTSUPP;
        }
        ar->arTxPwr= dbM = rrq->value;
        ar->arTxPwrSet = TRUE;
    } else {
        ar->arTxPwr = dbM = 0;
        ar->arTxPwrSet = FALSE;
    }
    if(ar->arWmiReady == TRUE)
    {
        AR_DEBUG_PRINTF("Set tx pwr cmd %d dbM\n", dbM);
        wmi_set_txPwr_cmd(ar->arWmi, dbM);
    }
    return 0;
}

/*
 * SIOCGIWTXPOW
 */
int
ar6000_ioctl_giwtxpow(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_param *rrq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    int ret = 0;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->bIsDestroyProgress) {
        return -EBUSY;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (down_interruptible(&ar->arSem)) {
        return -ERESTARTSYS;
    }

    if (ar->bIsDestroyProgress) {
        up(&ar->arSem);
        return -EBUSY;
    }

    if((ar->arWmiReady == TRUE) && (ar->arConnected == TRUE))
    {
        ar->arTxPwr = 0;

        if (wmi_get_txPwr_cmd(ar->arWmi) != A_OK) {
            up(&ar->arSem);
            return -EIO;
        }

        wait_event_interruptible_timeout(arEvent, ar->arTxPwr != 0, wmitimeout * HZ);

        if (signal_pending(current)) {
            ret = -EINTR;
         }
    }
   /* If the interace is down or wmi is not ready or target is not connected
      then return value stored in the device structure */

    if (!ret) {
         if (ar->arTxPwrSet == TRUE) {
            rrq->fixed = TRUE;
        }
        rrq->value = ar->arTxPwr;
        rrq->flags = IW_TXPOW_DBM;
        //
        // IWLIST need this flag to get TxPower
        //
        rrq->disabled = 0;
    }

    up(&ar->arSem);

    return ret;
}

/*
 * SIOCSIWRETRY
 * since iwconfig only provides us with one max retry value, we use it
 * to apply to data frames of the BE traffic class.
 */
static int
ar6000_ioctl_siwretry(struct net_device *dev,
             struct iw_request_info *info,
             struct iw_param *rrq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (rrq->disabled) {
        return -EOPNOTSUPP;
    }

    if ((rrq->flags & IW_RETRY_TYPE) != IW_RETRY_LIMIT) {
        return -EOPNOTSUPP;
    }

    if ( !(rrq->value >= WMI_MIN_RETRIES) || !(rrq->value <= WMI_MAX_RETRIES)) {
            return - EINVAL;
    }
    if(ar->arWmiReady == TRUE)
    {
        if (wmi_set_retry_limits_cmd(ar->arWmi, DATA_FRAMETYPE, WMM_AC_BE,
                                     rrq->value, 0) != A_OK){
            return -EINVAL;
        }
    }
    ar->arMaxRetries = rrq->value;
    return 0;
}

/*
 * SIOCGIWRETRY
 */
static int
ar6000_ioctl_giwretry(struct net_device *dev,
             struct iw_request_info *info,
             struct iw_param *rrq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    rrq->disabled = 0;
    switch (rrq->flags & IW_RETRY_TYPE) {
    case IW_RETRY_LIFETIME:
        return -EOPNOTSUPP;
        break;
    case IW_RETRY_LIMIT:
        rrq->flags = IW_RETRY_LIMIT;
        switch (rrq->flags & IW_RETRY_MODIFIER) {
        case IW_RETRY_MIN:
            rrq->flags |= IW_RETRY_MIN;
            rrq->value = WMI_MIN_RETRIES;
            break;
        case IW_RETRY_MAX:
            rrq->flags |= IW_RETRY_MAX;
            rrq->value = ar->arMaxRetries;
            break;
        }
        break;
    }
    return 0;
}

/*
 * SIOCSIWENCODE
 */
static int
ar6000_ioctl_siwencode(struct net_device *dev,
              struct iw_request_info *info,
              struct iw_point *erq, char *keybuf)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    int index;
    A_INT32 auth = 0;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }


    if (ar->arConnected && (ar->arPairwiseCrypto == NONE_CRYPT))
    {
        return -EIO;
    }

    if(ar->arNextMode != AP_NETWORK) {
    /*
     *  Static WEP Keys should be configured before setting the SSID
     */
    if (ar->arSsid[0] && erq->length) {
        return -EIO;
    }
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    index = erq->flags & IW_ENCODE_INDEX;

    if (index && (((index - 1) < WMI_MIN_KEY_INDEX) ||
                  ((index - 1) > WMI_MAX_KEY_INDEX)))
    {
        return -EIO;
    }

    if (erq->flags & IW_ENCODE_DISABLED) {
        /*
         * Encryption disabled
         */
        if (index) {
            /*
             * If key index was specified then clear the specified key
             */
            index--;
            A_MEMZERO(ar->arWepKeyList[index].arKey,
                      sizeof(ar->arWepKeyList[index].arKey));
            ar->arWepKeyList[index].arKeyLen = 0;
        }
        ar->arDot11AuthMode       = OPEN_AUTH;
        ar->arPairwiseCrypto      = NONE_CRYPT;
        ar->arGroupCrypto         = NONE_CRYPT;
        ar->arAuthMode            = NONE_AUTH;
    } else {
        /*
         * Enabling WEP encryption
         */
        if (index) {
            index--;                /* keyindex is off base 1 in iwconfig */
        }

        if (erq->flags & IW_ENCODE_OPEN) {
            auth |= OPEN_AUTH;
            ar->arDefTxKeyIndex = index;
        }
        if (erq->flags & IW_ENCODE_RESTRICTED) {
            auth |= SHARED_AUTH;
        }
		if(!auth)
			auth = OPEN_AUTH;
        if (erq->length) {
            if (!IEEE80211_IS_VALID_WEP_CIPHER_LEN(erq->length)) {
                return -EIO;
            }

            A_MEMZERO(ar->arWepKeyList[index].arKey,
                      sizeof(ar->arWepKeyList[index].arKey));
            A_MEMCPY(ar->arWepKeyList[index].arKey, keybuf, erq->length);
            ar->arWepKeyList[index].arKeyLen = erq->length;
            ar->arDot11AuthMode = auth;
        } else {
            if (ar->arWepKeyList[index].arKeyLen == 0) {
                return -EIO;
            }
            ar->arDefTxKeyIndex = index;

            if(ar->arConnected && ar->arWepKeyList[index].arKeyLen) {
                if(ar->arPrevCrypto == WEP_CRYPT)
                {
                    wmi_addKey_cmd(ar->arWmi,
                            index,
                            WEP_CRYPT,
                            GROUP_USAGE | TX_USAGE,
                            ar->arWepKeyList[index].arKeyLen,
                            NULL,
                            ar->arWepKeyList[index].arKey, KEY_OP_INIT_VAL, NULL,
                            NO_SYNC_WMIFLAG);
                }
            }
        }

        ar->arPairwiseCrypto      = WEP_CRYPT;
        ar->arGroupCrypto         = WEP_CRYPT;
        ar->arAuthMode            = NONE_AUTH;
    }

    if(ar->arNextMode != AP_NETWORK) {
    /*
     * profile has changed.  Erase ssid to signal change
     */
    A_MEMZERO(ar->arSsid, sizeof(ar->arSsid));
    }
    ar->ap_profile_flag = 1; /* There is a change in profile */
    return 0;
}

static int
ar6000_ioctl_giwencode(struct net_device *dev,
              struct iw_request_info *info,
              struct iw_point *erq, char *key)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_UINT8 keyIndex;
    struct ar_wep_key *wk;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ar->arPairwiseCrypto == NONE_CRYPT) {
        erq->length = 0;
        erq->flags = IW_ENCODE_DISABLED;
    } else {
        if (ar->arPairwiseCrypto == WEP_CRYPT) {
            /* get the keyIndex */
            keyIndex = erq->flags & IW_ENCODE_INDEX;
            if (0 == keyIndex) {
                keyIndex = ar->arDefTxKeyIndex;
            } else if ((keyIndex - 1 < WMI_MIN_KEY_INDEX) ||
                       (keyIndex - 1 > WMI_MAX_KEY_INDEX))
            {
                keyIndex = WMI_MIN_KEY_INDEX;
            } else {
                keyIndex--;
            }
            erq->flags = keyIndex + 1;
            erq->flags &= ~IW_ENCODE_DISABLED;
            wk = &ar->arWepKeyList[keyIndex];
            if (erq->length > wk->arKeyLen) {
                erq->length = wk->arKeyLen;
            }
            if (wk->arKeyLen) {
                A_MEMCPY(key, wk->arKey, erq->length);
            }
        } else {
            erq->flags &= ~IW_ENCODE_DISABLED;
            if (ar->user_saved_keys.keyOk) {
                erq->length = ar->user_saved_keys.ucast_ik.ik_keylen;
                if (erq->length) {
                    A_MEMCPY(key, ar->user_saved_keys.ucast_ik.ik_keydata, erq->length);
                }
            } else {
                erq->length = 1;    // not really printing any key but let iwconfig know enc is on
            }
        }
        if (ar->arDot11AuthMode & OPEN_AUTH) {
            erq->flags |= IW_ENCODE_OPEN;
        }
        if (ar->arDot11AuthMode & SHARED_AUTH) {
            erq->flags |= IW_ENCODE_RESTRICTED;
        }
    }

    return 0;
}

static int ar6000_ioctl_siwpower(struct net_device *dev,
                 struct iw_request_info *info,
                 union iwreq_data *wrqu, char *extra)
{
#if defined(WIRELESS_EXT) && WIRELESS_EXT > 20
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    WMI_POWER_MODE power_mode;

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (wrqu->power.disabled)
        power_mode = MAX_PERF_POWER;
    else
        power_mode = REC_POWER;

    if (wmi_powermode_cmd(ar->arWmi, power_mode) < 0)
        return -EIO;
#endif
    return 0;
}

static int ar6000_ioctl_giwpower(struct net_device *dev,
                 struct iw_request_info *info,
                 union iwreq_data *wrqu, char *extra)
{
#if defined(WIRELESS_EXT) && WIRELESS_EXT > 20
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }
    if(!ar->arWmi){
        return -EIO;
    }
    return wmi_get_power_mode_cmd(ar->arWmi);
#else
        return 0;
#endif
}

static int ar6000_ioctl_siwgenie(struct net_device *dev,
                 struct iw_request_info *info,
                 struct iw_point *dwrq,
                 char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
#ifdef WAPI_ENABLE
    A_UINT8    *ie = dwrq->pointer;
    A_UINT8     ie_type = ie[0];
    A_UINT16    ie_length = dwrq->length;
    A_UINT8     wapi_ie[128];
#endif /* WAPI_ENABLE */

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

#ifdef WAPI_ENABLE
    if (ie_type == IEEE80211_ELEMID_WAPI) {
        if (ie_length > 0) {
            if (copy_from_user(wapi_ie, ie, ie_length)) {
                return -EIO;
            }
        }
        wmi_set_appie_cmd(ar->arWmi, WMI_FRAME_ASSOC_REQ, ie_length, wapi_ie);
    } else if (ie_length == 0) {
        wmi_set_appie_cmd(ar->arWmi, WMI_FRAME_ASSOC_REQ, ie_length, wapi_ie);
    }
#endif /* WAPI_ENABLE */

    return 0;
}

static int ar6000_ioctl_giwgenie(struct net_device *dev,
                 struct iw_request_info *info,
                 struct iw_point *dwrq,
                 char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }
    dwrq->length = 0;
    dwrq->flags = 0;
    return 0;
}

/*
 * SIOCSIWAUTH
 */
static int
ar6000_ioctl_siwauth(struct net_device *dev,
              struct iw_request_info *info,
              struct iw_param *data, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_BOOL profChanged;
    A_UINT16 param;
    A_INT32 ret = 0;
    A_INT32 value;
#if defined(WIRELESS_EXT) && WIRELESS_EXT > 20
    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    param = data->flags & IW_AUTH_INDEX;
    value = data->value;
    profChanged = TRUE;
    ret = 0;

    switch (param) {
        case IW_AUTH_WPA_VERSION:
            if (value & IW_AUTH_WPA_VERSION_DISABLED) {
                ar->arAuthMode = NONE_AUTH;
            } else if (value & IW_AUTH_WPA_VERSION_WPA) {
                    ar->arAuthMode = WPA_AUTH_;
            } else if (value & IW_AUTH_WPA_VERSION_WPA2) {
                    ar->arAuthMode = WPA2_AUTH;
            } else {
                ret = -1;
                profChanged    = FALSE;
            }
            break;
        case IW_AUTH_CIPHER_PAIRWISE:
            if (value & IW_AUTH_CIPHER_NONE) {
                ar->arPairwiseCrypto = NONE_CRYPT;
                ar->arPairwiseCryptoLen = 0;
            } else if (value & IW_AUTH_CIPHER_WEP40) {
                ar->arPairwiseCrypto = WEP_CRYPT;
                ar->arPairwiseCryptoLen = IEEE80211_WEP_KEYLEN;
            } else if (value & IW_AUTH_CIPHER_TKIP) {
                ar->arPairwiseCrypto = TKIP_CRYPT;
                ar->arPairwiseCryptoLen = 0;
            } else if (value & IW_AUTH_CIPHER_CCMP) {
                ar->arPairwiseCrypto = AES_CRYPT;
                ar->arPairwiseCryptoLen = 0;
            } else if (value & IW_AUTH_CIPHER_WEP104) {
                ar->arPairwiseCrypto = WEP_CRYPT;
                ar->arPairwiseCryptoLen = IEEE80211_WEP104_KEYLEN;
            } else {
                ret = -1;
                profChanged    = FALSE;
            }
            break;
        case IW_AUTH_CIPHER_GROUP:
            if (value & IW_AUTH_CIPHER_NONE) {
                ar->arGroupCrypto = NONE_CRYPT;
                ar->arGroupCryptoLen = 0;
            } else if (value & IW_AUTH_CIPHER_WEP40) {
                ar->arGroupCrypto = WEP_CRYPT;
                ar->arGroupCryptoLen = IEEE80211_WEP_KEYLEN;
            } else if (value & IW_AUTH_CIPHER_TKIP) {
                ar->arGroupCrypto = TKIP_CRYPT;
                ar->arGroupCryptoLen = 0;
            } else if (value & IW_AUTH_CIPHER_CCMP) {
                ar->arGroupCrypto = AES_CRYPT;
                ar->arGroupCryptoLen = 0;
            } else if (value & IW_AUTH_CIPHER_WEP104) {
                ar->arGroupCrypto = WEP_CRYPT;
                ar->arGroupCryptoLen = IEEE80211_WEP104_KEYLEN;
            } else {
                ret = -1;
                profChanged    = FALSE;
            }
            break;
        case IW_AUTH_KEY_MGMT:
            if (value & IW_AUTH_KEY_MGMT_PSK) {
                if (WPA_AUTH_ == ar->arAuthMode) {
                    ar->arAuthMode = WPA_PSK_AUTH;
                } else if (WPA2_AUTH == ar->arAuthMode) {
                    ar->arAuthMode = WPA2_PSK_AUTH;
                } else {
                    ret = -1;
                }
            } else if (!(value & IW_AUTH_KEY_MGMT_802_1X)) {
                ar->arAuthMode = NONE_AUTH;
            }
            break;
        case IW_AUTH_TKIP_COUNTERMEASURES:
            wmi_set_tkip_countermeasures_cmd(ar->arWmi, value);
            profChanged    = FALSE;
            break;
        case IW_AUTH_DROP_UNENCRYPTED:
            profChanged    = FALSE;
            break;
        case IW_AUTH_80211_AUTH_ALG:
            ar->arDot11AuthMode = 0;
            if (value & IW_AUTH_ALG_OPEN_SYSTEM) {
                ar->arDot11AuthMode  |= OPEN_AUTH;
            }
            if (value & IW_AUTH_ALG_SHARED_KEY) {
                ar->arDot11AuthMode  |= SHARED_AUTH;
            }
            if (value & IW_AUTH_ALG_LEAP) {
                ar->arDot11AuthMode   = LEAP_AUTH;
            }
            if(ar->arDot11AuthMode == 0) {
                ret = -1;
                profChanged    = FALSE;
            }
            break;
        case IW_AUTH_WPA_ENABLED:
            if (!value) {
                ar->arAuthMode = NONE_AUTH;
            }
            break;
        case IW_AUTH_RX_UNENCRYPTED_EAPOL:
            profChanged    = FALSE;
            break;
        case IW_AUTH_ROAMING_CONTROL:
            profChanged    = FALSE;
            break;
        case IW_AUTH_PRIVACY_INVOKED:
            if (!value) {
                ar->arPairwiseCrypto = NONE_CRYPT;
                ar->arPairwiseCryptoLen = 0;
                ar->arGroupCrypto = NONE_CRYPT;
                ar->arGroupCryptoLen = 0;
            }
            break;
#ifdef WAPI_ENABLE
        case IW_AUTH_WAPI_ENABLED:
            ar->arWapiEnable = value;
            break;
#endif
        default:
           ret = -1;
           profChanged    = FALSE;
           break;
    }

    if (profChanged == TRUE) {
        /*
         * profile has changed.  Erase ssid to signal change
         */
        A_MEMZERO(ar->arSsid, sizeof(ar->arSsid));
    }
#endif /* WIRELESS_EXT > 20 */
    return ret;
}

static int ar6000_ioctl_giwauth(struct net_device *dev,
                struct iw_request_info *info,
                struct iw_param *dwrq,
                char *extra)
{
#if defined(WIRELESS_EXT) && WIRELESS_EXT > 20
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_UINT16 param;
    __attribute__((unused)) A_INT32 ret;
    struct iw_param *data = dwrq;

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    param = data->flags & IW_AUTH_INDEX;
    ret = 0;
    data->value = 0;


    switch (param) {
        case IW_AUTH_WPA_VERSION:
            if (ar->arAuthMode == NONE_AUTH) {
                data->value |= IW_AUTH_WPA_VERSION_DISABLED;
            } else if (ar->arAuthMode == WPA_AUTH_) {
                data->value |= IW_AUTH_WPA_VERSION_WPA;
            } else if (ar->arAuthMode == WPA2_AUTH) {
                data->value |= IW_AUTH_WPA_VERSION_WPA2;
            } else {
                ret = -1;
            }
            break;
        case IW_AUTH_CIPHER_PAIRWISE:
            if (ar->arPairwiseCrypto == NONE_CRYPT) {
                data->value |= IW_AUTH_CIPHER_NONE;
            } else if (ar->arPairwiseCrypto == WEP_CRYPT) {
                if (ar->arPairwiseCryptoLen == 13) {
                    data->value |= IW_AUTH_CIPHER_WEP104;
                } else {
                    data->value |= IW_AUTH_CIPHER_WEP40;
                }
            } else if (ar->arPairwiseCrypto == TKIP_CRYPT) {
                data->value |= IW_AUTH_CIPHER_TKIP;
            } else if (ar->arPairwiseCrypto == AES_CRYPT) {
                data->value |= IW_AUTH_CIPHER_CCMP;
            } else {
                ret = -1;
            }
            break;
        case IW_AUTH_CIPHER_GROUP:
            if (ar->arGroupCrypto == NONE_CRYPT) {
                    data->value |= IW_AUTH_CIPHER_NONE;
            } else if (ar->arGroupCrypto == WEP_CRYPT) {
                if (ar->arGroupCryptoLen == 13) {
                    data->value |= IW_AUTH_CIPHER_WEP104;
                } else {
                    data->value |= IW_AUTH_CIPHER_WEP40;
                }
            } else if (ar->arGroupCrypto == TKIP_CRYPT) {
                data->value |= IW_AUTH_CIPHER_TKIP;
            } else if (ar->arGroupCrypto == AES_CRYPT) {
                data->value |= IW_AUTH_CIPHER_CCMP;
            } else {
                ret = -1;
            }
            break;
        case IW_AUTH_KEY_MGMT:
            if ((ar->arAuthMode == WPA_PSK_AUTH) ||
                (ar->arAuthMode == WPA2_PSK_AUTH)) {
                data->value |= IW_AUTH_KEY_MGMT_PSK;
            } else if ((ar->arAuthMode == WPA_AUTH_) ||
                       (ar->arAuthMode == WPA2_AUTH)) {
                data->value |= IW_AUTH_KEY_MGMT_802_1X;
            }
            break;
        case IW_AUTH_TKIP_COUNTERMEASURES:
            // TODO. Save countermeassure enable/disable
            data->value = 0;
            break;
        case IW_AUTH_DROP_UNENCRYPTED:
            break;
        case IW_AUTH_80211_AUTH_ALG:
            if (ar->arDot11AuthMode == OPEN_AUTH) {
                data->value |= IW_AUTH_ALG_OPEN_SYSTEM;
            } else if (ar->arDot11AuthMode == SHARED_AUTH) {
                data->value |= IW_AUTH_ALG_SHARED_KEY;
            } else if (ar->arDot11AuthMode == LEAP_AUTH) {
                data->value |= IW_AUTH_ALG_LEAP;
            } else {
                ret = -1;
            }
            break;
        case IW_AUTH_WPA_ENABLED:
            if (ar->arAuthMode == NONE_AUTH) {
                data->value = 0;
            } else {
                data->value = 1;
            }
            break;
        case IW_AUTH_RX_UNENCRYPTED_EAPOL:
            break;
        case IW_AUTH_ROAMING_CONTROL:
            break;
        case IW_AUTH_PRIVACY_INVOKED:
            if (ar->arPairwiseCrypto == NONE_CRYPT) {
                data->value = 0;
            } else {
                data->value = 1;
            }
            break;
#ifdef WAPI_ENABLE
        case IW_AUTH_WAPI_ENABLED:
            data->value = ar->arWapiEnable;
            break;
#endif /* WAPI_ENABLE */
        default:
           ret = -1;
           break;
    }
#endif /* WIRELESS_EXT > 20 */
    return 0;
}


/*
 * SIOCSIWPMKSA
 */
static int
ar6000_ioctl_siwpmksa(struct net_device *dev,
              struct iw_request_info *info,
              struct iw_point *data, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_INT32 ret;
    A_STATUS status;
    struct iw_pmksa *pmksa;

    pmksa = (struct iw_pmksa *)extra;

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    ret = 0;
    status = A_OK;

    switch (pmksa->cmd) {
        case IW_PMKSA_ADD:
            status = wmi_setPmkid_cmd(ar->arWmi, pmksa->bssid.sa_data, pmksa->pmkid, TRUE);
            break;
        case IW_PMKSA_REMOVE:
            status = wmi_setPmkid_cmd(ar->arWmi, pmksa->bssid.sa_data, pmksa->pmkid, FALSE);
            break;
        case IW_PMKSA_FLUSH:
            if (ar->arConnected == TRUE) {
                status = wmi_setPmkid_cmd(ar->arWmi, ar->arBssid, NULL, 0);
            }
            break;
        default:
            ret=-1;
            break;
    }
    if (status != A_OK) {
        ret = -1;
    }

    return ret;
}


#ifdef WAPI_ENABLE
#define PN_INIT 0x5c365c36

static int ar6000_set_wapi_key(struct net_device *dev,
              struct iw_request_info *info,
              struct iw_point *erq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
    KEY_USAGE   keyUsage = 0;
    A_INT32     keyLen;
    A_UINT8     *keyData;
    A_INT32     index;
    A_UINT32    *PN;
    A_INT32     i;
    A_STATUS    status;
    A_UINT8     wapiKeyRsc[16];
    CRYPTO_TYPE keyType = WAPI_CRYPT;
    const A_UINT8 broadcastMac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

    index = erq->flags & IW_ENCODE_INDEX;
    if (index && (((index - 1) < WMI_MIN_KEY_INDEX) ||
                ((index - 1) > WMI_MAX_KEY_INDEX))) {
        return -EIO;
    }

    index--;
    if (index < 0 || index > 3) {
        return -EIO;
    }
    keyData = (A_UINT8 *)(ext + 1);
    keyLen = erq->length - sizeof(struct iw_encode_ext);
    A_MEMCPY(wapiKeyRsc, ext->tx_seq, sizeof(wapiKeyRsc));

    if (A_MEMCMP(ext->addr.sa_data, broadcastMac, sizeof(broadcastMac)) == 0) {
        keyUsage |= GROUP_USAGE;
        PN = (A_UINT32 *)wapiKeyRsc;
        for (i = 0; i < 4; i++) {
            PN[i] = PN_INIT;
        }
    } else {
        index += 4;
        keyUsage |= PAIRWISE_USAGE;
    }
    status = wmi_addKey_cmd(ar->arWmi,
                            index,
                            keyType,
                            keyUsage,
                            keyLen,
                            wapiKeyRsc,
                            keyData,
                            KEY_OP_INIT_WAPIPN,
                            NULL,
                            SYNC_BEFORE_WMIFLAG);
    if (A_OK != status) {
        return -EIO;
    }
    return 0;
}
#endif /* WAPI_ENABLE */


/*
 * SIOCSIWENCODEEXT
 */
static int
ar6000_ioctl_siwencodeext(struct net_device *dev,
              struct iw_request_info *info,
              struct iw_point *erq, char *extra)
{
#if defined(WIRELESS_EXT) && WIRELESS_EXT > 20


    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    A_INT32 index;
    struct iw_encode_ext *ext;
    KEY_USAGE keyUsage;
    A_INT32 keyLen;
    A_UINT8 *keyData;
    A_UINT8 keyRsc[8];
    A_STATUS status;
    CRYPTO_TYPE keyType;
#ifdef USER_KEYS
    struct ieee80211req_key ik;
#endif /* USER_KEYS */

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

#ifdef USER_KEYS
    ar->user_saved_keys.keyOk = FALSE;
#endif /* USER_KEYS */

    index = erq->flags & IW_ENCODE_INDEX;

    if (index && (((index - 1) < WMI_MIN_KEY_INDEX) ||
                ((index - 1) > WMI_MAX_KEY_INDEX)))
    {
        return -EIO;
    }

    ext = (struct iw_encode_ext *)extra;
    if (erq->flags & IW_ENCODE_DISABLED) {
        /*
         * Encryption disabled
         */
        if (index) {
            /*
             * If key index was specified then clear the specified key
             */
            index--;
            A_MEMZERO(ar->arWepKeyList[index].arKey,
                    sizeof(ar->arWepKeyList[index].arKey));
            ar->arWepKeyList[index].arKeyLen = 0;
        }
    } else {
        /*
         * Enabling WEP encryption
         */
        if (index) {
            index--;                /* keyindex is off base 1 in iwconfig */
        }

        keyUsage = 0;
        keyLen = erq->length - sizeof(struct iw_encode_ext);

        if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
            keyUsage = TX_USAGE;
            ar->arDefTxKeyIndex = index;
            // Just setting the key index
            if (keyLen == 0) {
                return 0;
            }
        }

        if (keyLen <= 0) {
            return -EIO;
        }

        /* key follows iw_encode_ext */
        keyData = (A_UINT8 *)(ext + 1);

        switch (ext->alg) {
            case IW_ENCODE_ALG_WEP:
                keyType = WEP_CRYPT;
#ifdef USER_KEYS
                ik.ik_type = IEEE80211_CIPHER_WEP;
#endif /* USER_KEYS */
                if (!IEEE80211_IS_VALID_WEP_CIPHER_LEN(keyLen)) {
                    return -EIO;
                }

                /* Check whether it is static wep. 
                 * Allow to set if we called wmi_disconnect() */
                if (!ar->arSsid[0] && ar->arSsidLen==0) {
                    A_MEMZERO(ar->arWepKeyList[index].arKey,
                            sizeof(ar->arWepKeyList[index].arKey));
                    A_MEMCPY(ar->arWepKeyList[index].arKey, keyData, keyLen);
                    ar->arWepKeyList[index].arKeyLen = keyLen;

                    return 0;
                }
                break;
            case IW_ENCODE_ALG_TKIP:
                keyType = TKIP_CRYPT;
#ifdef USER_KEYS
                ik.ik_type = IEEE80211_CIPHER_TKIP;
#endif /* USER_KEYS */
                break;
            case IW_ENCODE_ALG_CCMP:
                keyType = AES_CRYPT;
#ifdef USER_KEYS
                ik.ik_type = IEEE80211_CIPHER_AES_CCM;
#endif /* USER_KEYS */
                break;
#ifdef WAPI_ENABLE
            case IW_ENCODE_ALG_SM4:
                if (ar->arWapiEnable) {
                    return ar6000_set_wapi_key(dev, info, erq, extra);
                } else {
                    return -EIO;
                }
#endif
            default:
                return -EIO;
        }


        if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
            keyUsage |= GROUP_USAGE;
        } else {
            keyUsage |= PAIRWISE_USAGE;
        }

        if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID) {
            A_MEMCPY(keyRsc, ext->rx_seq, sizeof(keyRsc));
        } else {
            A_MEMZERO(keyRsc, sizeof(keyRsc));
        }

        if (((WPA_PSK_AUTH == ar->arAuthMode) || (WPA2_PSK_AUTH == ar->arAuthMode)) &&
                (GROUP_USAGE & keyUsage))
        {
            A_UNTIMEOUT(&ar->disconnect_timer);
        }

        if(ar->arConnected == TRUE && ar->arPrevCrypto == NONE_CRYPT)
        {
            if(keyLen)
            {
                ar->arWepKeyList[index].arKeyLen = keyLen;
                A_MEMZERO(ar->arWepKeyList[index].arKey,sizeof(ar->arWepKeyList[index].arKey));
                A_MEMCPY(ar->arWepKeyList[index].arKey, keyData, keyLen);
            }
        }
        else
        {
            status = wmi_addKey_cmd(ar->arWmi, index, keyType, keyUsage,
                    keyLen, keyRsc,
                    keyData, KEY_OP_INIT_VAL,
                    ext->addr.sa_data,
                    SYNC_BOTH_WMIFLAG);
            if (status != A_OK) {
                return -EIO;
            }
        }

#ifdef USER_KEYS
        ik.ik_keyix = index;
        ik.ik_keylen = keyLen;
        memcpy(ik.ik_keydata, keyData, keyLen);
        memcpy(&ik.ik_keyrsc, keyRsc, sizeof(keyRsc));
        memcpy(ik.ik_macaddr, ext->addr.sa_data, ETH_ALEN);
        if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
            memcpy(&ar->user_saved_keys.bcast_ik, &ik,
                    sizeof(struct ieee80211req_key));
        } else {
            memcpy(&ar->user_saved_keys.ucast_ik, &ik,
                    sizeof(struct ieee80211req_key));
        }
        ar->user_saved_keys.keyOk = TRUE;
#endif /* USER_KEYS */
    }
#endif
    return 0;
}


static int ar6000_ioctl_giwencodeext(struct net_device *dev,
                     struct iw_request_info *info,
                     struct iw_point *dwrq,
                     char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ar->arPairwiseCrypto == NONE_CRYPT) {
        dwrq->length = 0;
        dwrq->flags = IW_ENCODE_DISABLED;
    } else {
        dwrq->length = 0;
    }
    return 0;
}


/*
 * SIOCGIWNAME
 */
int
ar6000_ioctl_giwname(struct net_device *dev,
           struct iw_request_info *info,
           char *name, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    switch (ar->arPhyCapability) {
    case (WMI_11A_CAPABILITY):
        strncpy(name, "AR6000 802.11a", IFNAMSIZ);
        break;
    case (WMI_11G_CAPABILITY):
        strncpy(name, "AR6000 802.11g", IFNAMSIZ);
        break;
    case (WMI_11AG_CAPABILITY):
        strncpy(name, "AR6000 802.11ag", IFNAMSIZ);
        break;
    default:
        strncpy(name, "AR6000 802.11", IFNAMSIZ);
        break;
    }

    return 0;
}

/*
 * SIOCSIWFREQ
 */
int
ar6000_ioctl_siwfreq(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_freq *freq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    /*
     * We support limiting the channels via wmiconfig.
     *
     * We use this command to configure the channel hint for the connect cmd
     * so it is possible the target will end up connecting to a different
     * channel.
     */
    if (freq->e > 1) {
        return -EINVAL;
    } else if (freq->e == 1) {
        ar->arChannelHint = freq->m / 100000;
    } else {
        ar->arChannelHint = wlan_ieee2freq(freq->m);
    }

    ar->ap_profile_flag = 1; /* There is a change in profile */

    A_PRINTF("channel hint set to %d\n", ar->arChannelHint);
    return 0;
}

/*
 * SIOCGIWFREQ
 */
int
ar6000_ioctl_giwfreq(struct net_device *dev,
                struct iw_request_info *info,
                struct iw_freq *freq, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ar->arNetworkType == AP_NETWORK) {
        if(ar->arChannelHint) {
            freq->m = ar->arChannelHint * 100000;
        } else {
            return -EINVAL;
        }
    } else {
        if (ar->arConnected != TRUE) {
            return -EINVAL;
        } else {
            freq->m = ar->arBssChannel * 100000;
        }
    }

    freq->e = 1;

    return 0;
}

/*
 * SIOCSIWMODE
 */
int
ar6000_ioctl_siwmode(struct net_device *dev,
            struct iw_request_info *info,
            __u32 *mode, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    switch (*mode) {
    case IW_MODE_INFRA:
        ar->arNextMode = INFRA_NETWORK;
        break;
    case IW_MODE_ADHOC:
        ar->arNextMode = ADHOC_NETWORK;
        break;
    case IW_MODE_MASTER:
        ar->arNextMode = AP_NETWORK;
        break;
    default:
        return -EINVAL;
    }

    /* clear all shared parameters between AP and STA|IBSS modes when we
     * switch between them. Switch between STA & IBSS modes does'nt clear
     * the shared profile. This is as per the original design for switching
     * between STA & IBSS.
     */
    if (ar->arNetworkType == AP_NETWORK || ar->arNextMode == AP_NETWORK) {
        ar->arDot11AuthMode      = OPEN_AUTH;
        ar->arAuthMode           = NONE_AUTH;
        ar->arPairwiseCrypto     = NONE_CRYPT;
        ar->arPairwiseCryptoLen  = 0;
        ar->arGroupCrypto        = NONE_CRYPT;
        ar->arGroupCryptoLen     = 0;
        ar->arChannelHint        = 0;
        ar->arBssChannel         = 0;
        A_MEMZERO(ar->arBssid, sizeof(ar->arBssid));
        A_MEMZERO(ar->arSsid, sizeof(ar->arSsid));
        ar->arSsidLen = 0;
    }

    /* SSID has to be cleared to trigger a profile change while switching
     * between STA & IBSS modes having the same SSID
     */
    if (ar->arNetworkType != ar->arNextMode) {
        A_MEMZERO(ar->arSsid, sizeof(ar->arSsid));
    }

    return 0;
}

/*
 * SIOCGIWMODE
 */
int
ar6000_ioctl_giwmode(struct net_device *dev,
            struct iw_request_info *info,
            __u32 *mode, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    switch (ar->arNetworkType) {
    case INFRA_NETWORK:
        *mode = IW_MODE_INFRA;
        break;
    case ADHOC_NETWORK:
        *mode = IW_MODE_ADHOC;
        break;
    case AP_NETWORK:
        *mode = IW_MODE_MASTER;
        break;
    default:
        return -EIO;
    }
    return 0;
}

/*
 * SIOCSIWSENS
 */
int
ar6000_ioctl_siwsens(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_param *sens, char *extra)
{
    return 0;
}

/*
 * SIOCGIWSENS
 */
int
ar6000_ioctl_giwsens(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_param *sens, char *extra)
{
    sens->value = 0;
    sens->fixed = 1;

    return 0;
}

/*
 * SIOCGIWRANGE
 */
int
ar6000_ioctl_giwrange(struct net_device *dev,
             struct iw_request_info *info,
             struct iw_point *data, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    struct iw_range *range = (struct iw_range *) extra;
    int i, ret = 0;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->bIsDestroyProgress) {
        return -EBUSY;
    }

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (down_interruptible(&ar->arSem)) {
        return -ERESTARTSYS;
    }

    if (ar->bIsDestroyProgress) {
        up(&ar->arSem);
        return -EBUSY;
    }

    ar->arNumChannels = -1;
    A_MEMZERO(ar->arChannelList, sizeof (ar->arChannelList));

    if (wmi_get_channelList_cmd(ar->arWmi) != A_OK) {
        up(&ar->arSem);
        return -EIO;
    }

    wait_event_interruptible_timeout(arEvent, ar->arNumChannels != -1, wmitimeout * HZ);

    if (signal_pending(current)) {
        up(&ar->arSem);
        return -EINTR;
    }

    data->length = sizeof(struct iw_range);
    A_MEMZERO(range, sizeof(struct iw_range));

    range->txpower_capa = 0;

    range->min_pmp = 1 * 1024;
    range->max_pmp = 65535 * 1024;
    range->min_pmt = 1 * 1024;
    range->max_pmt = 1000 * 1024;
    range->pmp_flags = IW_POWER_PERIOD;
    range->pmt_flags = IW_POWER_TIMEOUT;
    range->pm_capa = 0;

    range->we_version_compiled = WIRELESS_EXT;
    range->we_version_source = 13;

    range->retry_capa = IW_RETRY_LIMIT;
    range->retry_flags = IW_RETRY_LIMIT;
    range->min_retry = 0;
    range->max_retry = 255;

    range->num_frequency = range->num_channels = ar->arNumChannels;
    for (i = 0; i < ar->arNumChannels; i++) {
        range->freq[i].i = wlan_freq2ieee(ar->arChannelList[i]);
        range->freq[i].m = ar->arChannelList[i] * 100000;
        range->freq[i].e = 1;
         /*
         * Linux supports max of 32 channels, bail out once you
         * reach the max.
         */
        if (i == IW_MAX_FREQUENCIES) {
            break;
        }
    }

    /* Max quality is max field value minus noise floor */
    range->max_qual.qual  = 0xff - 161;

    /*
     * In order to use dBm measurements, 'level' must be lower
     * than any possible measurement (see iw_print_stats() in
     * wireless tools).  It's unclear how this is meant to be
     * done, but setting zero in these values forces dBm and
     * the actual numbers are not used.
     */
    range->max_qual.level = 0;
    range->max_qual.noise = 0;

    range->sensitivity = 3;

    range->max_encoding_tokens = 4;
    /* XXX query driver to find out supported key sizes */
    range->num_encoding_sizes = 3;
    range->encoding_size[0] = 5;        /* 40-bit */
    range->encoding_size[1] = 13;       /* 104-bit */
    range->encoding_size[2] = 16;       /* 128-bit */

    range->num_bitrates = 0;

    /* estimated maximum TCP throughput values (bps) */
    range->throughput = 22000000;

    range->min_rts = 0;
    range->max_rts = 2347;
    range->min_frag = 256;
    range->max_frag = 2346;

    up(&ar->arSem);

    return ret;
}

/*
 * SIOCSIWPRIV
 *
 */
int
ar6000_ioctl_siwpriv(struct net_device *dev,
              struct iw_request_info *info,
              struct iw_param *data, char *extra)
{
#ifdef ANDROID_ECLAIR_ENV
    int ret = 0;
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    static WMI_SCAN_PARAMS_CMD scParams = {0, 0, 0, 0, 0,
                                           WMI_SHORTSCANRATIO_DEFAULT,
                                           DEFAULT_SCAN_CTRL_FLAGS,
                                           0};
	
    if (strcasecmp((A_UCHAR *)data->value, "LINKSPEED") == 0)
    {
        if(ar->arWmiReady == TRUE)
        {
            ar->arBitRate = 0xFFFF;
            wmi_get_bitrate_cmd(ar->arWmi);
            wait_event_interruptible_timeout(arEvent, ar->arBitRate != 0xFFFF, wmitimeout * HZ);
        }
        sprintf((A_UCHAR *)data->value, "LinkSpeed %d",ar->arBitRate/1000);		
        return ret;
    }
    else if (strcasecmp((A_UCHAR *)data->value, "MACADDR") == 0)
    {
        wait_event_interruptible_timeout(arEvent, dev->dev_addr[5] != 0, 3000);
        sprintf((A_UCHAR *)data->value, "Macaddr = %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x", dev->dev_addr[0],dev->dev_addr[1],dev->dev_addr[2],dev->dev_addr[3],dev->dev_addr[4],dev->dev_addr[5]);
        return ret;
    }
    else if (strcasecmp((A_UCHAR *)data->value, "START") == 0)
    {
        if(ar->arWmiReady == TRUE)
        {

            /* Enable foreground scanning */
            if (wmi_scanparams_cmd(ar->arWmi, scParams.fg_start_period,
                                       scParams.fg_end_period,
                                       scParams.bg_period,
                                       scParams.minact_chdwell_time,
                                       scParams.maxact_chdwell_time,
                                       scParams.pas_chdwell_time,
                                       scParams.shortScanRatio,
                                       scParams.scanCtrlFlags,
                                       scParams.max_dfsch_act_time,
                                       scParams.maxact_scan_per_ssid) != A_OK)
            {
                    ret = -EIO;
            }
            if (ar->arSsidLen) {
                if (ar6000_connect_to_ap(ar) != A_OK)
                {
                    ret = -EIO;
                }
            }

            if (-EIO != ret)
            {
                sprintf((A_UCHAR *)data->value, "OK");
            }
            return ret;
        }
    }
    else if (strcasecmp((A_UCHAR *)data->value, "STOP") == 0)
    {
        if(ar->arWmiReady == TRUE)
        {
            AR6000_SPIN_LOCK(&ar->arLock, 0);
            if (ar->arConnected == TRUE || ar->arConnectPending == TRUE) {
                AR6000_SPIN_UNLOCK(&ar->arLock, 0);
                wmi_disconnect_cmd(ar->arWmi);
            } else {
                AR6000_SPIN_UNLOCK(&ar->arLock, 0);
            }

            if (wmi_scanparams_cmd(ar->arWmi, 0xFFFF, 0, 0, 0, 0, 0, 0, 0xFF, 0, 0) != A_OK)
            {
                ret = -EIO;
            }
	
            if (-EIO != ret)
            {
                sprintf((A_UCHAR *)data->value, "OK");
            }
            return ret;
        }
    }
    else if (strcasecmp((A_UCHAR *)data->value, "RSSI") == 0)
    {
        if(ar->arWmiReady == TRUE)
        {
            if (ar->arConnected == TRUE)
            {
                if(wmi_get_stats_cmd(ar->arWmi) != A_OK) {
                    ret = -EIO;
                }
                else
                {
                    wait_event_interruptible_timeout(arEvent, ar->statsUpdatePending == FALSE, wmitimeout * HZ);
                    if (ar->arRssi < 0)
                    {
                        sprintf((A_UCHAR *)data->value, "%s rssi %d",ar->arSsid,ar->arRssi);
                    }
                    else
                    {
                        ret = -EIO;
                    }
                }
                return ret;
            }
            else
            {
                sprintf((A_UCHAR *)data->value, "OK");
            }
            return ret;
        }
    }
	

#endif
    return -EOPNOTSUPP; /*for now*/
}

/*
 * SIOCSIWAP
 * This ioctl is used to set the desired bssid for the connect command.
 */
int
ar6000_ioctl_siwap(struct net_device *dev,
              struct iw_request_info *info,
              struct sockaddr *ap_addr, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ap_addr->sa_family != ARPHRD_ETHER) {
        return -EIO;
    }

    if (A_MEMCMP(&ap_addr->sa_data, bcast_mac, AR6000_ETH_ADDR_LEN) == 0) {
        A_MEMZERO(ar->arReqBssid, sizeof(ar->arReqBssid));
    } else {
        A_MEMCPY(ar->arReqBssid, &ap_addr->sa_data,  sizeof(ar->arReqBssid));
    }

    return 0;
}

/*
 * SIOCGIWAP
 */
int
ar6000_ioctl_giwap(struct net_device *dev,
              struct iw_request_info *info,
              struct sockaddr *ap_addr, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ar->arNetworkType == AP_NETWORK) {
        A_MEMCPY(&ap_addr->sa_data, dev->dev_addr, ATH_MAC_LEN);
        ap_addr->sa_family = ARPHRD_ETHER;
        return 0;
    }

    if (ar->arConnected != TRUE) {
        return -EINVAL;
    }

    A_MEMCPY(&ap_addr->sa_data, ar->arBssid, sizeof(ar->arBssid));
    ap_addr->sa_family = ARPHRD_ETHER;

    return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
/*
 * SIOCSIWMLME
 */
int
ar6000_ioctl_siwmlme(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_point *data, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->bIsDestroyProgress) {
        return -EBUSY;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }


    if (down_interruptible(&ar->arSem)) {
        return -ERESTARTSYS;
    }

    if (ar->bIsDestroyProgress) {
        up(&ar->arSem);
        return -EBUSY;
    }

    if (data->pointer && data->length == sizeof(struct iw_mlme)) {

        A_UINT8 arNetworkType;
        struct iw_mlme mlme;

        if (copy_from_user(&mlme, data->pointer, sizeof(struct iw_mlme)))
            return -EIO;

        switch (mlme.cmd) {

            case IW_MLME_DEAUTH:
                /* ignore it */
                break;

            case IW_MLME_DISASSOC:
                if ((ar->arConnected != TRUE) ||
                    (memcmp(ar->arBssid, mlme.addr.sa_data, 6) != 0)) {

                    up(&ar->arSem);
                    return -EINVAL;
                }
                wmi_setPmkid_cmd(ar->arWmi, ar->arBssid, NULL, 0);
                arNetworkType = ar->arNetworkType;
                ar6000_init_profile_info(ar);
                ar->arNetworkType = arNetworkType;
                reconnect_flag = 0;
                wmi_disconnect_cmd(ar->arWmi);
                A_MEMZERO(ar->arSsid, sizeof(ar->arSsid));
                ar->arSsidLen = 0;
                if (ar->arSkipScan == FALSE) {
                    A_MEMZERO(ar->arReqBssid, sizeof(ar->arReqBssid));
                }
                break;

            case IW_MLME_AUTH:
                /* fall through */
            case IW_MLME_ASSOC:
                /* fall through */
            default:
                up(&ar->arSem);
                return -EOPNOTSUPP;
        }
    }

    up(&ar->arSem);
    return 0;
}
#endif /* LINUX_VERSION_CODE */

/*
 * SIOCGIWAPLIST
 */
int
ar6000_ioctl_iwaplist(struct net_device *dev,
            struct iw_request_info *info,
            struct iw_point *data, char *extra)
{
    return -EIO;            /* for now */
}

/*
 * SIOCSIWSCAN
 */
int
ar6000_ioctl_siwscan(struct net_device *dev,
                     struct iw_request_info *info,
                     struct iw_point *data, char *extra)
{
#define ACT_DWELLTIME_DEFAULT   105
#define HOME_TXDRAIN_TIME       100
#define SCAN_INT                HOME_TXDRAIN_TIME + ACT_DWELLTIME_DEFAULT
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);
    int ret = 0;

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    if ( ar->scan_triggered > 0 ) {
        ++ar->scan_triggered;
        if (ar->scan_triggered < 5) {
            return 0;
        } else {
            AR_DEBUG_PRINTF("Scan request is triggered over 5 times. Not scan complete event\n");
        }
    }

     /* We ask for everything from the target */
    if (wmi_bssfilter_cmd(ar->arWmi, ALL_BSS_FILTER, 0) != A_OK) {
        printk("Couldn't set filtering\n");
        ret = -EIO;
    }

#if WIRELESS_EXT >= 18
    if (data->pointer && (data->length == sizeof(struct iw_scan_req)))
    {
        if ((data->flags & IW_SCAN_THIS_ESSID) == IW_SCAN_THIS_ESSID)
        {
            struct iw_scan_req req;
            if (copy_from_user(&req, data->pointer, sizeof(struct iw_scan_req)))
                return -EIO;
            if (wmi_probedSsid_cmd(ar->arWmi, 1, SPECIFIC_SSID_FLAG, req.essid_len, req.essid) != A_OK)
                return -EIO;
        }
        else
        {
            if (wmi_probedSsid_cmd(ar->arWmi, 1, DISABLE_SSID_FLAG, 0, NULL) != A_OK)
                return -EIO;
        }
    }
    else
    {
        if (wmi_probedSsid_cmd(ar->arWmi, 1, DISABLE_SSID_FLAG, 0, NULL) != A_OK)
            return -EIO;
    }
#endif

    if (wmi_startscan_cmd(ar->arWmi, WMI_LONG_SCAN, FALSE, FALSE, \
                          0, 200, 0, NULL) != A_OK) {
        ret = -EIO;
    }

    if (ret == 0) {
        ar->scan_triggered = 1;
    }

    return ret;
#undef  ACT_DWELLTIME_DEFAULT
#undef HOME_TXDRAIN_TIME
#undef SCAN_INT
}


/*
 * Units are in db above the noise floor. That means the
 * rssi values reported in the tx/rx descriptors in the
 * driver are the SNR expressed in db.
 *
 * If you assume that the noise floor is -95, which is an
 * excellent assumption 99.5 % of the time, then you can
 * derive the absolute signal level (i.e. -95 + rssi).
 * There are some other slight factors to take into account
 * depending on whether the rssi measurement is from 11b,
 * 11g, or 11a.   These differences are at most 2db and
 * can be documented.
 *
 * NB: various calculations are based on the orinoco/wavelan
 *     drivers for compatibility
 */
static void
ar6000_set_quality(struct iw_quality *iq, A_INT8 rssi)
{
    if (rssi < 0) {
        iq->qual = 0;
    } else {
        iq->qual = rssi;
    }

    /* NB: max is 94 because noise is hardcoded to 161 */
    if (iq->qual > 94)
        iq->qual = 94;

    iq->noise = 161;        /* -95dBm */
    iq->level = iq->noise + iq->qual;
    iq->updated = 7;
}


int
ar6000_ioctl_siwcommit(struct net_device *dev,
                     struct iw_request_info *info,
                     struct iw_point *data, char *extra)
{
    AR_SOFTC_T *ar = (AR_SOFTC_T *)netdev_priv(dev);

    if (is_iwioctl_allowed(ar->arNextMode, info->cmd) != A_OK) {
        A_PRINTF("wext_ioctl: cmd=0x%x not allowed in this mode\n", info->cmd);
        return -EOPNOTSUPP;
    }

    if (ar->arWmiReady == FALSE) {
        return -EIO;
    }

    if (ar->arWlanState == WLAN_DISABLED) {
        return -EIO;
    }

    AR_DEBUG_PRINTF("AP: SSID %s freq %d authmode %d dot11 auth %d"\
                    " PW crypto %d GRP crypto %d\n",
                    ar->arSsid, ar->arChannelHint,
                    ar->arAuthMode, ar->arDot11AuthMode,
                    ar->arPairwiseCrypto, ar->arGroupCrypto);

    ar6000_ap_mode_profile_commit(ar);

    /* if there is a profile switch from STA|IBSS mode to AP mode,
     * update the host driver association state for the STA|IBSS mode.
     */
    if (ar->arNetworkType != AP_NETWORK && ar->arNextMode == AP_NETWORK) {
        ar->arConnectPending = FALSE;
        ar->arConnected = FALSE;
        /* Stop getting pkts from upper stack */
        netif_stop_queue(ar->arNetDev);
        A_MEMZERO(ar->arBssid, sizeof(ar->arBssid));
        ar->arBssChannel = 0;
        ar->arBeaconInterval = 0;

        /* Flush the Tx queues */
        ar6000_TxDataCleanup(ar);

        /* Start getting pkts from upper stack */
        netif_wake_queue(ar->arNetDev);
    }

    return 0;
}

/* Structures to export the Wireless Handlers */
static const iw_handler ath_handlers[] = {
    (iw_handler) ar6000_ioctl_siwcommit,        /* SIOCSIWCOMMIT */
    (iw_handler) ar6000_ioctl_giwname,          /* SIOCGIWNAME */
    (iw_handler) NULL,                          /* SIOCSIWNWID */
    (iw_handler) NULL,                          /* SIOCGIWNWID */
    (iw_handler) ar6000_ioctl_siwfreq,          /* SIOCSIWFREQ */
    (iw_handler) ar6000_ioctl_giwfreq,          /* SIOCGIWFREQ */
    (iw_handler) ar6000_ioctl_siwmode,          /* SIOCSIWMODE */
    (iw_handler) ar6000_ioctl_giwmode,          /* SIOCGIWMODE */
    (iw_handler) ar6000_ioctl_siwsens,          /* SIOCSIWSENS */
    (iw_handler) ar6000_ioctl_giwsens,          /* SIOCGIWSENS */
    (iw_handler) NULL /* not _used */,          /* SIOCSIWRANGE */
    (iw_handler) ar6000_ioctl_giwrange,         /* SIOCGIWRANGE */
    (iw_handler) ar6000_ioctl_siwpriv,          /* SIOCSIWPRIV */
    (iw_handler) NULL /* kernel code */,        /* SIOCGIWPRIV */
    (iw_handler) NULL /* not used */,           /* SIOCSIWSTATS */
    (iw_handler) NULL /* kernel code */,        /* SIOCGIWSTATS */
    (iw_handler) NULL,                          /* SIOCSIWSPY */
    (iw_handler) NULL,                          /* SIOCGIWSPY */
    (iw_handler) NULL,                          /* SIOCSIWTHRSPY */
    (iw_handler) NULL,                          /* SIOCGIWTHRSPY */
    (iw_handler) ar6000_ioctl_siwap,            /* SIOCSIWAP */
    (iw_handler) ar6000_ioctl_giwap,            /* SIOCGIWAP */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13)
    (iw_handler) ar6000_ioctl_siwmlme,          /* SIOCSIWMLME */
#else
    (iw_handler) NULL,                          /* -- hole -- */
#endif  /* LINUX_VERSION_CODE */
    (iw_handler) ar6000_ioctl_iwaplist,         /* SIOCGIWAPLIST */
    (iw_handler) ar6000_ioctl_siwscan,          /* SIOCSIWSCAN */
    (iw_handler) ar6000_ioctl_giwscan,          /* SIOCGIWSCAN */
    (iw_handler) ar6000_ioctl_siwessid,         /* SIOCSIWESSID */
    (iw_handler) ar6000_ioctl_giwessid,         /* SIOCGIWESSID */
    (iw_handler) NULL,                          /* SIOCSIWNICKN */
    (iw_handler) NULL,                          /* SIOCGIWNICKN */
    (iw_handler) NULL,                          /* -- hole -- */
    (iw_handler) NULL,                          /* -- hole -- */
    (iw_handler) ar6000_ioctl_siwrate,          /* SIOCSIWRATE */
    (iw_handler) ar6000_ioctl_giwrate,          /* SIOCGIWRATE */
    (iw_handler) NULL,           /* SIOCSIWRTS */
    (iw_handler) NULL,           /* SIOCGIWRTS */
    (iw_handler) NULL,          /* SIOCSIWFRAG */
    (iw_handler) NULL,          /* SIOCGIWFRAG */
    (iw_handler) ar6000_ioctl_siwtxpow,         /* SIOCSIWTXPOW */
    (iw_handler) ar6000_ioctl_giwtxpow,         /* SIOCGIWTXPOW */
    (iw_handler) ar6000_ioctl_siwretry,         /* SIOCSIWRETRY */
    (iw_handler) ar6000_ioctl_giwretry,         /* SIOCGIWRETRY */
    (iw_handler) ar6000_ioctl_siwencode,        /* SIOCSIWENCODE */
    (iw_handler) ar6000_ioctl_giwencode,        /* SIOCGIWENCODE */
    (iw_handler) ar6000_ioctl_siwpower,         /* SIOCSIWPOWER */
    (iw_handler) ar6000_ioctl_giwpower,         /* SIOCGIWPOWER */
    (iw_handler) NULL,                          /* -- hole -- */
    (iw_handler) NULL,                          /* -- hole -- */
    (iw_handler) ar6000_ioctl_siwgenie,         /* SIOCSIWGENIE */
    (iw_handler) ar6000_ioctl_giwgenie,         /* SIOCGIWGENIE */
    (iw_handler) ar6000_ioctl_siwauth,          /* SIOCSIWAUTH */
    (iw_handler) ar6000_ioctl_giwauth,          /* SIOCGIWAUTH */
    (iw_handler) ar6000_ioctl_siwencodeext,     /* SIOCSIWENCODEEXT */
    (iw_handler) ar6000_ioctl_giwencodeext,     /* SIOCGIWENCODEEXT */
    (iw_handler) ar6000_ioctl_siwpmksa,         /* SIOCSIWPMKSA */

};

struct iw_handler_def ath_iw_handler_def = {
    .standard         = (iw_handler *)ath_handlers,
    .num_standard     = ARRAY_SIZE(ath_handlers),
    .private          = NULL,
    .num_private      = 0,
};
