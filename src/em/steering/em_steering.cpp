/**
 * Copyright 2023 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <linux/filter.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <openssl/rand.h>
#include "em.h"
#include "em_msg.h"
#include "em_cmd.h"
#include "em_cmd_exec.h"

bool get_target_bss_channel_info(dm_easy_mesh_t *dm, em_cmd_steer_params_t *params)
{
    mac_address_t zero_mac = {0};
    em_op_class_info_t *op_class_info;

    if ((dm == NULL) || (params == NULL) ||
        (memcmp(params->target, zero_mac, sizeof(mac_address_t)) == 0)) {
        return false;
    }

    em_bss_info_t *target_bss = dm->get_bss_info_with_mac(params->target);
    if (target_bss == NULL) {
        em_printfout("%s:%d Target BSS:%s not found in dm (num_bss:%u)\n",
            __func__, __LINE__, util::mac_to_string(params->target).c_str(), dm->get_num_bss());
        return false;
    }

    em_printfout("%s:%d Found target BSS:%s ruid:%s, scanning %u op_class entries\n",
        __func__, __LINE__,
        util::mac_to_string(params->target).c_str(),
        util::mac_to_string(target_bss->ruid.mac).c_str(),
        dm->get_num_op_class());

    unsigned int current_count = 0;
    for (unsigned int i = 0; i < dm->get_num_op_class(); i++) {
        op_class_info = dm->get_op_class_info(i);
        if ((op_class_info == NULL)) {
            continue;
        }
        current_count++;

        if (memcmp(op_class_info->id.ruid, target_bss->ruid.mac, sizeof(mac_address_t)) != 0) {
            em_printfout("%s:%d  [current] op_class[%u] ruid:%s != target ruid (skip)\n",
                __func__, __LINE__, i,
                util::mac_to_string(op_class_info->id.ruid).c_str());
            continue;
        }

        if ((params->target_op_class == 0 || params->target_channel == 0) && 
            (op_class_info->op_class != 0 && op_class_info->channel != 0)) {
            params->target_op_class = op_class_info->op_class;
            params->target_channel = op_class_info->channel;
        }
        em_printfout("%s:%d Target op_class:%u channel:%u (from current)\n",
            __func__, __LINE__, params->target_op_class, params->target_channel);
    }

    em_printfout("%s:%d No current op_class match found (total current entries:%u). Trying capability fallback\n",
        __func__, __LINE__, current_count);

    return true;
}

unsigned char get_steering_ack_reason_code(dm_easy_mesh_t *dm, mac_addr_t sta_mac)
{
    unsigned char reason_code = 0x00;
    mac_address_t zero_mac = {0};

    if ((sta_mac == NULL) || (memcmp(sta_mac, zero_mac, sizeof(mac_address_t)) == 0)) {
        reason_code = 0x00;
        return reason_code;
    }

    if (dm == NULL) {
        reason_code = 0x00;
        return reason_code;
    }

    dm_sta_t *sta = dm->get_first_sta(sta_mac);
    while (sta != NULL) {
        em_sta_info_t *sta_info = sta->get_sta_info();
        if (sta_info == NULL) {
            reason_code = 0x03;
            return reason_code;
        }

        if (sta_info->associated == true) {
            for (unsigned int i = 0; i < dm->get_num_bss(); i++) {
                em_bss_info_t *bss_info = dm->get_bss_info(i);
                if (bss_info == NULL) {
                    continue;
                }

                if (memcmp(bss_info->bssid.mac, sta_info->bssid, sizeof(mac_address_t)) == 0) {
                    reason_code = 0x01;
                    return reason_code;
                }
            }
        }

        sta = dm->get_next_sta(sta_mac, sta);
    }

    reason_code = 0x02;
    return reason_code;
}

int em_steering_t::disassoc_non_11v_client(em_sta_info_t *sta_info, bssid_t bssid)
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    em_cmd_disassoc_params_t	*disassoc_param;
    dm_easy_mesh_t *dm = NULL;
    dm_sta_t *sta = NULL;

    dm = get_data_model();
    sta = dm->find_sta(sta_info->id, bssid);
    if(sta == NULL) {
        em_printfout("%s:%d STA not found\n", __func__, __LINE__);
        return -1;
    }

    disassoc_param = reinterpret_cast<em_cmd_disassoc_params_t *> (buff);
    memset(disassoc_param, 0, sizeof(em_cmd_disassoc_params_t));

    disassoc_param->num = 1;
    disassoc_param->params[0].reason = WLAN_REASON_BSS_TRANSITION_DISASSOC;
    disassoc_param->params[0].silent = false;
    disassoc_param->params[0].disassoc_time = 0;
    memcpy(disassoc_param->params[0].sta_mac, sta_info->id, sizeof(mac_address_t));
    memcpy(disassoc_param->params[0].bssid, bssid, sizeof(bssid_t));

    em_printfout("%s:%d Steering non-11v client: sta=%s, bssid=%s\n", __func__, __LINE__,
        util::mac_to_string(sta_info->id).c_str(), util::mac_to_string(bssid).c_str());
    get_mgr()->io_process(em_bus_event_type_disassoc_sta, reinterpret_cast<unsigned char *> (disassoc_param),
            sizeof(em_cmd_disassoc_params_t));
    
        return 0;
}

int em_steering_t::enforce_client_assoc_ctrl(em_sta_info_t *sta_info, bssid_t bssid)
{
    em_client_assoc_ctrl_req_t *assoc_ctrl_req;
    unsigned char buff[MAX_EM_BUFF_SZ];
    dm_easy_mesh_t *dm = NULL;
    dm_sta_t *sta = NULL;

    dm = get_data_model();
    sta = dm->find_sta(sta_info->id, bssid);
    if(sta == NULL) {
        em_printfout("%s:%d STA not found\n", __func__, __LINE__);
        return -1;
    }
    assoc_ctrl_req = reinterpret_cast<em_client_assoc_ctrl_req_t *> (buff);
    memset(assoc_ctrl_req, 0, sizeof(assoc_ctrl_req));

    memcpy(&assoc_ctrl_req->bssid, &bssid, sizeof(bssid_t));
    assoc_ctrl_req->assoc_control = 0x00;
    assoc_ctrl_req->count = 1;
    assoc_ctrl_req->validity_period = htons(EM_CAC_REQ_VALIDITY_PERIOD);
    memcpy(&assoc_ctrl_req->sta_mac, &sta->get_sta_info()->id, sizeof(mac_address_t));

    get_mgr()->io_process(em_bus_event_type_client_assoc_ctrl_req,
        reinterpret_cast<unsigned char *> (assoc_ctrl_req), sizeof(em_client_assoc_ctrl_req_t));

    return 0;
}

int em_steering_t::send_client_assoc_ctrl_req_msg()
{
    em_cmd_t *pcmd = get_current_cmd();
    dm_easy_mesh_t *dm = get_data_model();

    if (!pcmd || !dm)
        return -1;

    for (unsigned int i = 0; i < pcmd->m_param.u.disassoc_params.num; i++) {
        em_disassoc_params_t *disassoc_param = &pcmd->m_param.u.disassoc_params.params[i];

        for (unsigned int j = 0; j < dm->m_num_bss; j++) {
            if ((memcmp(disassoc_param->bssid, dm->m_bss[j].m_bss_info.bssid.mac, sizeof(bssid_t)) == 0) &&
                (memcmp(dm->m_bss[j].m_bss_info.ruid.mac, get_radio_interface_mac(), sizeof(mac_address_t)) == 0)) {

                em_client_assoc_ctrl_req_t assoc_ctrl;
                memset(&assoc_ctrl, 0, sizeof(assoc_ctrl));

                memcpy(&assoc_ctrl.bssid, &disassoc_param->bssid, sizeof(bssid_t));

                assoc_ctrl.count = 1;
                memcpy(&assoc_ctrl.sta_mac, &disassoc_param->sta_mac, sizeof(mac_address_t));

                if (disassoc_param->disassoc_time > 0) {
                    // BLOCK
                    assoc_ctrl.assoc_control  = 0x00;
                    assoc_ctrl.validity_period = htons(static_cast<uint16_t>(disassoc_param->disassoc_time));
                } else {
                    // UNBLOCK
                    assoc_ctrl.assoc_control  = 0x01;
                    assoc_ctrl.validity_period = 0;
                }

                send_client_assoc_ctrl_req_msg(&assoc_ctrl);
            }
        }
    }

    set_state(em_state_ctrl_configured);
    return 0;
}

int em_steering_t::send_client_assoc_ctrl_req_msg(em_client_assoc_ctrl_req_t *assoc_ctrl)
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    unsigned short  msg_type = em_msg_type_client_assoc_ctrl_req;
    size_t len = 0;
    em_cmdu_t *cmdu;
    em_tlv_t *tlv;
    unsigned char *tmp = buff;
    unsigned short type = htons(ETH_P_1905);
    dm_easy_mesh_t *dm;

    dm = get_data_model();

    memcpy(tmp, dm->get_agent_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, dm->get_ctrl_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, reinterpret_cast<unsigned char *> (&type), sizeof(unsigned short));
    tmp += sizeof(unsigned short);
    len += sizeof(unsigned short);

    cmdu = reinterpret_cast<em_cmdu_t *> (tmp);

    memset(tmp, 0, sizeof(em_cmdu_t));
    cmdu->type = htons(msg_type);
    cmdu->id = htons(get_mgr()->get_next_msg_id());
    cmdu->last_frag_ind = 1;
    cmdu->relay_ind = 0;

    tmp += sizeof(em_cmdu_t);
    len += sizeof(em_cmdu_t);

    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_client_assoc_ctrl_req;
    size_t tlv_value_len = sizeof(bssid_t) + sizeof(unsigned char) +
        sizeof(unsigned short) + sizeof(unsigned char) + (static_cast<size_t>(assoc_ctrl->count) * sizeof(mac_address_t));
    memcpy(tlv->value, assoc_ctrl, tlv_value_len);
    tlv->len = htons(tlv_value_len);

    tmp += (sizeof (em_tlv_t) + tlv_value_len);
    len += (sizeof (em_tlv_t) + tlv_value_len);

    // End of message
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_eom;
    tlv->len = 0;

    tmp += (sizeof (em_tlv_t));
    len += (sizeof (em_tlv_t));

    if (em_msg_t(em_msg_type_client_assoc_ctrl_req, em_profile_type_3, buff, static_cast<unsigned int> (len)).validate(errors) == 0) {
        printf("Client Assoc Control Request msg validation failed\n");
        return -1;
    }

    if (send_frame(buff, static_cast<unsigned int> (len))  < 0) {
        printf("%s:%d: Client Assoc Control Request msg send failed, error:%d\n", __func__, __LINE__, errno);
        return -1;
    }

    m_client_assoc_ctrl_req_tx_cnt++;
    printf("%s:%d: Client Assoc Control Request (%d) Send Successful\n", __func__, __LINE__, m_client_assoc_ctrl_req_tx_cnt);

    return static_cast<int> (len);
}

int em_steering_t::send_client_steering_req_msg()
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    unsigned short  msg_type = em_msg_type_client_steering_req;
    short sz = 0;
    size_t len = 0;
    em_cmdu_t *cmdu;
    em_tlv_t *tlv;
    unsigned char *tmp = buff;
    unsigned short type = htons(ETH_P_1905);
    dm_easy_mesh_t *dm;
    em_device_info_t *dev_info;
    bool use_profile2;
    em_cmd_steer_params_t *params = &get_current_cmd()->m_param.u.steer_params;
    dm_sta_t *sta;
    em_sta_info_t *sta_info;
    mac_addr_str_t sta_mac_str;

    dm = get_data_model();
    dev_info = dm->get_device_info();
    sta = dm->get_first_sta(params->sta_mac);
    sta_info = sta->get_sta_info();

    if ((params->target_op_class == 0) || (params->target_channel == 0)) {
        bool resolved = get_target_bss_channel_info(dm, params);

        // Fallback: search across all data models (target BSS may be on a different device)
        if (!resolved && get_mgr() != NULL) {
            em_t *iter_em = static_cast<em_t *>(hash_map_get_first(get_mgr()->m_em_map));
            while (iter_em != NULL && !resolved) {
                dm_easy_mesh_t *iter_dm = iter_em->get_data_model();
                if (iter_dm != NULL && iter_dm != dm) {
                    resolved = get_target_bss_channel_info(iter_dm, params);
                }
                iter_em = static_cast<em_t *>(hash_map_get_next(get_mgr()->m_em_map, iter_em));
            }
        }

        if (resolved) {
            em_printfout("%s:%d resolved target op_class:%u channel:%u from target bssid:%s",
                __func__, __LINE__, params->target_op_class, params->target_channel,
                util::mac_to_string(params->target).c_str());
        } else {
            em_printfout("%s:%d unable to resolve target op_class/channel from target bssid:%s",
                __func__, __LINE__, util::mac_to_string(params->target).c_str());
        }
    }

    use_profile2 = (dev_info != NULL && dev_info->profile >= em_profile_type_2);
    em_printfout("%s:%d profile:%d use_profile2:%d multi_band_cap:%d sta_mac:%s \n", __func__, __LINE__, dev_info->profile,
        use_profile2, sta_info->multi_band_cap, dm_easy_mesh_t::macbytes_to_string(params->sta_mac, sta_mac_str));

    memcpy(tmp, dm->get_agent_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, dm->get_ctrl_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, reinterpret_cast<unsigned char *> (&type), sizeof(unsigned short));
    tmp += sizeof(unsigned short);
    len += sizeof(unsigned short);

    cmdu = reinterpret_cast<em_cmdu_t *> (tmp);

    memset(tmp, 0, sizeof(em_cmdu_t));
    cmdu->type = htons(msg_type);
    cmdu->id = htons(get_mgr()->get_next_msg_id());
    cmdu->last_frag_ind = 1;
    cmdu->relay_ind = 0;

    tmp += sizeof(em_cmdu_t);
    len += sizeof(em_cmdu_t);

    // 17.2.29 Steering Request TLV/ Profile-2 Steering Request TLV 17.2.57
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    if (use_profile2 && (sta_info->multi_band_cap)) {
        // 17.2.57 Profile-2 Steering Request TLV
        tlv->type = em_tlv_type_profile2_steering_request;
        sz = create_profile2_btm_request_tlv(tlv->value);
    } else {
        // 17.2.29 Steering Request TLV
        tlv->type = em_tlv_type_steering_request;
        sz = create_btm_request_tlv(tlv->value);
    }
    tlv->len = htons(static_cast<short unsigned int> (sz));

    em_printfout("%s:%d tlv->type:%d \n", __func__, __LINE__, tlv->type);
	tmp += (sizeof(em_tlv_t) + static_cast<size_t> (sz));
	len += (sizeof(em_tlv_t) + static_cast<size_t> (sz));

    // End of message
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_eom;
    tlv->len = 0;

    tmp += (sizeof (em_tlv_t));
    len += (sizeof (em_tlv_t));

    if (em_msg_t(em_msg_type_client_steering_req, em_profile_type_3, buff, static_cast<unsigned int> (len)).validate(errors) == 0) {
        printf("Client Steering Request msg validation failed\n");
        return -1;
    }

    em_printfout("%s:%d sending frame len:%d \n", __func__, __LINE__, len);
    util::print_hex_dump(len, buff);
    if (send_frame(buff, static_cast<unsigned int> (len))  < 0) {
        printf("%s:%d: Client Steering Request msg send failed, error:%d\n", __func__, __LINE__, errno);
        return -1;
    }

    m_client_steering_req_tx_cnt++;
    printf("%s:%d: Client Steering Request (%d) Send Successful\n", __func__, __LINE__, m_client_steering_req_tx_cnt);

    return static_cast<int> (len);
}

int em_steering_t::send_btm_report_msg()
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    unsigned short  msg_type = em_msg_type_client_steering_btm_rprt;
    size_t len = 0;
    em_cmdu_t *cmdu;
    em_tlv_t *tlv;
    unsigned char *tmp = buff;
    short sz = 0;
    unsigned short type = htons(ETH_P_1905);
    dm_easy_mesh_t *dm = get_data_model();
    em_device_info_t *dev_info;

    dev_info = dm->get_device_info();
    if (dev_info == NULL) {
        printf("%s:%d: Device info not found\n", __func__, __LINE__);
        return -1;
    }

    memcpy(tmp, dm->get_controller_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, dm->get_agent_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, reinterpret_cast<unsigned char *> (&type), sizeof(unsigned short));
    tmp += sizeof(unsigned short);
    len += sizeof(unsigned short);

    cmdu = reinterpret_cast<em_cmdu_t *> (tmp);

    memset(tmp, 0, sizeof(em_cmdu_t));
    cmdu->type = htons(msg_type);
    cmdu->id = htons(get_mgr()->get_next_msg_id());
    cmdu->last_frag_ind = 1;

    tmp += sizeof(em_cmdu_t);
    len += sizeof(em_cmdu_t);

    // 17.2.30 Steering BTM Report TLV format
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_steering_btm_rprt;
    sz = create_btm_report_tlv(tlv->value);
    tlv->len = htons(static_cast<short unsigned int> (sz));

	tmp += (sizeof(em_tlv_t) + static_cast<size_t> (sz));
	len += (sizeof(em_tlv_t) + static_cast<size_t> (sz));

    // End of message
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_eom;
    tlv->len = 0;

    tmp += (sizeof(em_tlv_t));
    len += (sizeof(em_tlv_t));

    if (em_msg_t(em_msg_type_client_steering_btm_rprt, dev_info->profile, buff, static_cast<unsigned int> (len)).validate(errors) == 0) {
        printf("%s:%d: Steering BTM report validation failed\n", __func__, __LINE__);
        return -1;
    }

    if (send_frame(buff, static_cast<unsigned int> (len))  < 0) {
        printf("%s:%d: Steering BTM report send failed, error:%d\n", __func__, __LINE__, errno);
        return -1;
    }

    printf("%s:%d: Steering BTM report send success\n", __func__, __LINE__);

    return static_cast<int> (len);
}

int em_steering_t::send_1905_ack_message(mac_addr_t sta_mac, unsigned short msg_id)
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    unsigned short  msg_type = em_msg_type_1905_ack;
    size_t len = 0;
    em_cmdu_t *cmdu;
    em_tlv_t *tlv;
    unsigned char *tmp = buff;
    short sz = 0;
    unsigned char reason_code = 0;
    unsigned short type = htons(ETH_P_1905);
    dm_easy_mesh_t *dm = get_data_model();

    if(dm == NULL) {
        printf("%s:%d: Data model not found\n", __func__, __LINE__);
        return -1;
    }

    em_printfout("%s:%d sta_mac: %s, msg_id: %d \n", __func__, __LINE__,
        util::mac_to_string(sta_mac).c_str(), msg_id);

    memcpy(tmp, dm->get_controller_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, dm->get_agent_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    em_printfout("%s:%d ctrl_mac: %s, agent_mac: %s\n", __func__, __LINE__, util::mac_to_string(dm->get_controller_interface_mac()).c_str(),
        util::mac_to_string(dm->get_agent_al_interface_mac()).c_str());

    memcpy(tmp, reinterpret_cast<unsigned char *> (&type), sizeof(unsigned short));
    tmp += sizeof(unsigned short);
    len += sizeof(unsigned short);

    cmdu = reinterpret_cast<em_cmdu_t *> (tmp);

    memset(tmp, 0, sizeof(em_cmdu_t));
    cmdu->type = htons(msg_type);
    cmdu->id = htons(msg_id);
    cmdu->last_frag_ind = 1;

    tmp += sizeof(em_cmdu_t);
    len += sizeof(em_cmdu_t);

    //17.2.36 Error Code TLV format
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_error_code;
    reason_code = get_steering_ack_reason_code(dm, sta_mac);
    em_printfout("%s:%d reason_code: 0x%02x", __func__, __LINE__, reason_code);
    sz = create_error_code_tlv(tlv->value, reason_code, sta_mac);
    tlv->len = htons(static_cast<short unsigned int> (sz));

    tmp += (sizeof(em_tlv_t) + static_cast<size_t> (sz));
    len += (sizeof(em_tlv_t) + static_cast<size_t> (sz));

    // End of message
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_eom;
    tlv->len = 0;

    tmp += (sizeof(em_tlv_t));
    len += (sizeof(em_tlv_t));

    if (em_msg_t(em_msg_type_1905_ack, em_profile_type_3, buff, static_cast<unsigned int> (len)).validate(errors) == 0) {
        printf("%s:%d: 1905 ACK validation failed\n", __func__, __LINE__);
        return -1;
    }

    em_printfout("%s:%d sending frame len:%d \n", __func__, __LINE__, len);
    util::print_hex_dump(len, buff);
    if (send_frame(buff, static_cast<unsigned int> (len))  < 0) {
        printf("%s:%d: 1905 ACK send failed, error:%d\n", __func__, __LINE__, errno);
        return -1;
    }
    printf("%s:%d: 1905 ACK send success\n", __func__, __LINE__);

    return static_cast<int> (len);
}

int em_steering_t::send_1905_ack_message(unsigned short msg_id)
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    unsigned short  msg_type = em_msg_type_1905_ack;
    size_t len = 0;
    em_cmdu_t *cmdu;
    em_tlv_t *tlv;
    unsigned char *tmp = buff;
    short sz = 0;
    unsigned char reason_code = 0;
    unsigned short type = htons(ETH_P_1905);
    dm_easy_mesh_t *dm = get_data_model();

    if(dm == NULL) {
        printf("%s:%d: Data model not found\n", __func__, __LINE__);
        return -1;
    }

    memcpy(tmp, dm->get_agent_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, dm->get_controller_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    em_printfout("%s:%d ctrl_mac: %s, agent_mac: %s\n", __func__, __LINE__, util::mac_to_string(dm->get_controller_interface_mac()).c_str(),
        util::mac_to_string(dm->get_agent_al_interface_mac()).c_str());

    memcpy(tmp, reinterpret_cast<unsigned char *> (&type), sizeof(unsigned short));
    tmp += sizeof(unsigned short);
    len += sizeof(unsigned short);

    cmdu = reinterpret_cast<em_cmdu_t *> (tmp);

    memset(tmp, 0, sizeof(em_cmdu_t));
    cmdu->type = htons(msg_type);
    cmdu->id = htons(msg_id);
    cmdu->last_frag_ind = 1;

    tmp += sizeof(em_cmdu_t);
    len += sizeof(em_cmdu_t);

    // End of message
    tlv = reinterpret_cast<em_tlv_t *> (tmp);
    tlv->type = em_tlv_type_eom;
    tlv->len = 0;

    tmp += (sizeof(em_tlv_t));
    len += (sizeof(em_tlv_t));

    if (em_msg_t(em_msg_type_1905_ack, em_profile_type_3, buff, static_cast<unsigned int> (len)).validate(errors) == 0) {
        printf("%s:%d: 1905 ACK validation failed\n", __func__, __LINE__);
        return 0;
    }

    em_printfout("%s:%d sending frame len:%d \n", __func__, __LINE__, len);
    util::print_hex_dump(len, buff);
    if (send_frame(buff, static_cast<unsigned int> (len))  < 0) {
        printf("%s:%d: 1905 ACK send failed, error:%d\n", __func__, __LINE__, errno);
        return 0;
    }
    printf("%s:%d: 1905 ACK send success\n", __func__, __LINE__);

    return static_cast<int> (len);
}

short em_steering_t::create_btm_request_tlv(unsigned char *buff)
{
    size_t len = 0;
    em_steering_req_t *req = reinterpret_cast<em_steering_req_t *> (buff);
    em_cmd_steer_params_t *params = &get_current_cmd()->m_param.u.steer_params;

    //memcpy(&req->bssid, get_data_model()->m_bss[0].m_bss_info.bssid.mac, sizeof(bssid_t));
    memcpy(&req->bssid, params->source, sizeof(bssid_t));
    req->req_mode                           = static_cast<unsigned char>(params->request_mode) & 0x01;
    req->btm_dissoc_imminent                = params->disassoc_imminent;
    req->btm_abridged                       = params->btm_abridged;
    req->btm_link_removal_imminent = params->link_removal_imminent;
    if(params->request_mode == 1)
    {
        //ignore this
    req->steering_opportunity_window        = 0;
    } else {
        req->steering_opportunity_window    = static_cast<short unsigned int> (params->steer_opportunity_win);
    }
    req->btm_dissoc_timer                   = htons(static_cast<uint16_t> (params->btm_disassociation_timer));
    req->sta_list_count                     = 1;
    memcpy(req->sta_mac_addr, params->sta_mac, sizeof(mac_addr_t));
    req->target_bssid_list_count            = 1;
    memcpy(req->target_bssids, params->target, sizeof(mac_addr_t));
    req->target_bss_op_class                = static_cast<unsigned char> (params->target_op_class);
    req->target_bss_channel_num             = static_cast<unsigned char> (params->target_channel);

    len += sizeof(em_steering_req_t);

    return static_cast<short> (len);
}

short em_steering_t::create_btm_report_tlv(unsigned char *buff)
{
    short len = 0;
    unsigned char *tmp = buff;
    em_cmd_t *pcmd = get_current_cmd();
    em_cmd_btm_report_params_t  *btm_report_param = &pcmd->m_param.u.btm_report_params;

    memcpy(tmp, &btm_report_param->source, sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += static_cast<short> (sizeof(mac_address_t));

    memcpy(tmp, &btm_report_param->sta_mac, sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += static_cast<short> (sizeof(mac_address_t));

    memcpy(tmp, &btm_report_param->status_code, sizeof(unsigned char));
    tmp += sizeof(unsigned char);
    len += static_cast<short> (sizeof(unsigned char));

    // Per EasyMesh 17.2.30: Target BSSID is present only when status_code == 0 (accepted)
    if (btm_report_param->status_code == BTM_STATUS_ACCEPT) {
        memcpy(tmp, &btm_report_param->target, sizeof(mac_address_t));
        tmp += sizeof(mac_address_t);
        len += static_cast<short> (sizeof(mac_address_t));
    }

    return len;
}

short em_steering_t::create_error_code_tlv(unsigned char *buff, int val, mac_addr_t sta_mac)
{
    short len = 0;
    unsigned char *tmp = buff;

    memcpy(tmp, &val, sizeof(unsigned char));
    tmp += sizeof(unsigned char);
    len += static_cast<short> (sizeof(unsigned char));

    memcpy(tmp, sta_mac, sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += static_cast<short> (sizeof(mac_address_t));

    return len;
}

short em_steering_t::create_profile2_btm_request_tlv(unsigned char *buff)
{
    size_t len = 0;
    em_profile2_steering_req_t *req = reinterpret_cast<em_profile2_steering_req_t *> (buff);
    em_cmd_steer_params_t *params = &get_current_cmd()->m_param.u.steer_params;

    memcpy(&req->bssid, params->source, sizeof(bssid_t));
    req->req_mode                           = static_cast<unsigned char>(params->request_mode) & 0x01;
    req->btm_dissoc_imminent                = params->disassoc_imminent;
    req->btm_abridged                       = params->btm_abridged;
    req->reserved                           = 0;
    if (params->request_mode == 1) {
        req->steering_opportunity_window    = 0;
    } else {
        req->steering_opportunity_window    = static_cast<unsigned short>(params->steer_opportunity_win);
    }
    req->btm_dissoc_timer                   = htons(static_cast<uint16_t>(params->btm_disassociation_timer));
    req->sta_list_count                     = static_cast<unsigned char>(0x01);
    memcpy(req->sta_mac_addr, params->sta_mac, sizeof(mac_addr_t));
    req->target_bssid_list_count            = static_cast<unsigned char>(0x01);
    memcpy(req->target_bss_info.target_bssid, params->target, sizeof(bssid_t));
    req->target_bss_info.target_bss_op_class    = static_cast<unsigned char>(params->target_op_class);
    req->target_bss_info.target_bss_channel_num = static_cast<unsigned char>(params->target_channel);
    req->target_bss_info.target_bss_reason_code = static_cast<unsigned char>(0x06);

    len += sizeof(em_profile2_steering_req_t);

    return static_cast<short>(len);
}

int em_steering_t::handle_client_steering_req(unsigned char *buff, unsigned int len)
{
    em_tlv_t *tlv;
    em_cmdu_t *cmdu;
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    mac_addr_str_t mac_str, bssid_str;
    mac_addr_t sta_mac;
    bssid_t source_bssid;
    em_steering_req_t *steer_req_p1 = NULL;
    em_profile2_steering_req_t *steer_req_p2 = NULL;
    unsigned char req_mode = 0, *tlv_ptr = NULL, *end_ptr = NULL;
    bool disassoc_imminent = false, is_profile2 = false, sta_is_11v = false;
    unsigned int disassoc_timer = 0, steer_opp_win = 0;
    dm_easy_mesh_t *dm = NULL;
    dm_sta_t *sta = NULL;
    em_sta_info_t *sta_info = NULL;

    if (em_msg_t(em_msg_type_client_steering_req, em_profile_type_2, buff, len).validate(errors) == 0) {
        printf("%s:%d:Client Steering Request message validation failed\n", __func__, __LINE__);
        return -1;
    }

    // Walk TLVs to find either Profile-2 Steering Request (0xC3) or Profile-1 Steering Request (0x9B)
    tlv = reinterpret_cast<em_tlv_t *> (buff + sizeof(em_raw_hdr_t) + sizeof(em_cmdu_t));

    tlv_ptr = reinterpret_cast<unsigned char *>(tlv);
    end_ptr = buff + len;
    cmdu = reinterpret_cast<em_cmdu_t *> (buff + sizeof(em_raw_hdr_t));

    dm = get_data_model();

    while (tlv_ptr < end_ptr) {
        em_tlv_t *cur_tlv = reinterpret_cast<em_tlv_t *>(tlv_ptr);
        if (cur_tlv->type == em_tlv_type_eom) {
            break;
        }
        em_printfout("%s:%d cur_tlv->type: %d \n", __func__, __LINE__, cur_tlv->type);
        if (cur_tlv->type == em_tlv_type_profile2_steering_request) {
            // Profile-2 Steering Request TLV found — use it (preferred)
            is_profile2 = true;
            steer_req_p2 = reinterpret_cast<em_profile2_steering_req_t *>(&cur_tlv->value);
            dm_easy_mesh_t::macbytes_to_string(steer_req_p2->sta_mac_addr, mac_str);
            memcpy(sta_mac, steer_req_p2->sta_mac_addr, sizeof(mac_addr_t));
            memcpy(source_bssid, steer_req_p2->bssid, sizeof(bssid_t));

            req_mode = steer_req_p2->req_mode;
            disassoc_imminent = steer_req_p2->btm_dissoc_imminent;
            disassoc_timer = ntohs(steer_req_p2->btm_dissoc_timer);
            steer_opp_win = steer_req_p2->steering_opportunity_window;
            sta = dm->find_sta(steer_req_p2->sta_mac_addr, steer_req_p2->bssid);

            printf("%s:%d Received Profile-2 steer req for sta=%s, bssid=%s\n", __func__, __LINE__,
                mac_str, dm_easy_mesh_t::macbytes_to_string(steer_req_p2->bssid, bssid_str));
            em_printfout("%s:%d target_op_class=%d, target_channel=%d, target_bssid=%s, reason_code=%d\n",
                __func__, __LINE__,
                steer_req_p2->target_bss_info.target_bss_op_class,
                steer_req_p2->target_bss_info.target_bss_channel_num,
                dm_easy_mesh_t::macbytes_to_string(steer_req_p2->target_bss_info.target_bssid, bssid_str),
                steer_req_p2->target_bss_info.target_bss_reason_code);
            break;    
        } else if (cur_tlv->type == em_tlv_type_steering_request) {
            // Fallback to Profile-1 Steering Request TLV
            steer_req_p1 = reinterpret_cast<em_steering_req_t *>(&cur_tlv->value);
            dm_easy_mesh_t::macbytes_to_string(steer_req_p1->sta_mac_addr, mac_str);
            memcpy(sta_mac, steer_req_p1->sta_mac_addr, sizeof(mac_addr_t));
            memcpy(source_bssid, steer_req_p1->bssid, sizeof(bssid_t));

            req_mode = steer_req_p1->req_mode;
            disassoc_imminent = steer_req_p1->btm_dissoc_imminent;
            disassoc_timer = ntohs(steer_req_p1->btm_dissoc_timer);
            steer_opp_win = steer_req_p1->steering_opportunity_window;
            sta = dm->find_sta(steer_req_p1->sta_mac_addr, steer_req_p1->bssid); 

            printf("%s:%d Received Profile-1 steer req for sta=%s, bssid=%s\n", __func__, __LINE__,
                mac_str, dm_easy_mesh_t::macbytes_to_string(steer_req_p1->bssid, bssid_str));
            em_printfout("%s:%d target_op_class=%d, target_channel=%d, target_bssid=%s\n",
                __func__, __LINE__, steer_req_p1->target_bss_op_class,
                steer_req_p1->target_bss_channel_num,
                dm_easy_mesh_t::macbytes_to_string(steer_req_p1->target_bssids, bssid_str));
            break;
        }
        tlv_ptr += sizeof(em_tlv_t) + ntohs(cur_tlv->len);
    }

    if(!steer_req_p1 && !steer_req_p2) {
        em_printfout("%s:%d: No Steering Request TLV (Profile-1 or Profile-2) found in message\n", __func__, __LINE__);
        send_1905_ack_message(sta_mac, ntohs(cmdu->id));
        return -1;
    }

    if(sta == NULL) {
        em_printfout("%s:%d: STA not found\n", __func__, __LINE__);
        send_1905_ack_message(sta_mac, ntohs(cmdu->id));
        return -1;
    }
    sta_info = sta->get_sta_info();

    em_printfout("%s:%d Sending ACK message for sta=%s, bssid=%s\n", __func__, __LINE__, mac_str, bssid_str);
    send_1905_ack_message(sta_mac, ntohs(cmdu->id));

    em_printfout("%s:%d Enforcing Client Assoc control request for sta=%s, bssid=%s\n", __func__, __LINE__, mac_str, bssid_str);
    if(enforce_client_assoc_ctrl(sta_info, source_bssid) != 0) {
        em_printfout("%s:%d Failed to enforce Client Assoc control request for sta=%s, bssid=%s\n", __func__, __LINE__, mac_str, bssid_str);
    }

    sta_is_11v = is_profile2 || sta_info->multi_band_cap;

    em_printfout("%s:%d is_profile2:%d multi_band_cap:%d sta_is_11v:%d req_mode:%d\n",
        __func__, __LINE__, is_profile2, sta_info->multi_band_cap, sta_is_11v, req_mode);

    if(!sta_is_11v && (req_mode & 0x01) == em_steering_mandate_t) {
        em_printfout("%s:%d Triggering Disassociation for non-11v client: sta=%s, bssid=%s\n", __func__, __LINE__,
            mac_str, bssid_str);

        if(disassoc_non_11v_client(sta_info, source_bssid) != 0) {
            em_printfout("%s:%d Failed to disassociate non-11v client: sta=%s, bssid=%s\n", __func__, __LINE__,
                mac_str, bssid_str);
        }

        return 0;
    } else if(!sta_is_11v) {
        //Agent should decide how to steer non-11v client in case of steering opportunity
        return 0;
    }

    //Steering 11v capable client by sending BSS Transition Request
    if(is_profile2) {
        get_mgr()->io_process(em_bus_event_type_bss_tm_req_profile_2,
            reinterpret_cast<unsigned char *>(steer_req_p2), sizeof(em_profile2_steering_req_t));
    } else {
        get_mgr()->io_process(em_bus_event_type_bss_tm_req,
            reinterpret_cast<unsigned char *>(steer_req_p1), sizeof(em_steering_req_t));
    }

    em_printfout("%s:%d req_mode: %d, disassoc_imminent: %d, disassoc_timer: %d\n", __func__, __LINE__, req_mode, disassoc_imminent, disassoc_timer);
    // Sta disassoc timer for steering mandate with disassoc_imminent
    if (req_mode == 0x01 && disassoc_imminent && disassoc_timer > 0) {
        em_sta_timer_t start_disassoc_timer_params;

        if(sta_info->sta_timer_active == em_sta_timer_type_disassoc) {
            em_printfout("%s:%d Disassoc timer already active for STA %s\n", __func__, __LINE__, util::mac_to_string(sta_info->id).c_str());
            return 0;
        }
        memset(&start_disassoc_timer_params, 0, sizeof(start_disassoc_timer_params));
        start_disassoc_timer_params.type = em_sta_timer_type_disassoc;
        memcpy(start_disassoc_timer_params.sta_mac, sta_info->id, sizeof(mac_address_t));
        memcpy(start_disassoc_timer_params.source_bssid, source_bssid, sizeof(bssid_t));
        start_disassoc_timer_params.duration_ms = (unsigned int)(disassoc_timer * 1.024);

        get_mgr()->io_process(em_bus_event_type_start_sta_timer, reinterpret_cast<unsigned char *>(&start_disassoc_timer_params), sizeof(em_sta_timer_t));
        sta_info->sta_timer_active |= em_sta_timer_type_disassoc;

        em_printfout("Queued disassoc timer: %u sec for STA %s \n", disassoc_timer, util::mac_to_string(sta_info->id).c_str());
    } else if ((req_mode & 0x01) == em_steering_opportunity_t && steer_opp_win > 0) {
        em_sta_timer_t start_steer_opp_timer_params;

        if(sta_info->sta_timer_active == em_sta_timer_type_steer_opp || sta_info->sta_timer_active == em_sta_timer_type_disassoc) {
            em_printfout("%s:%d Timer already active for STA %s type:%d\n", __func__, __LINE__, util::mac_to_string(sta_info->id).c_str(), sta_info->sta_timer_active);
            return 0;
        }
        memset(&start_steer_opp_timer_params, 0, sizeof(start_steer_opp_timer_params));
        start_steer_opp_timer_params.type = em_sta_timer_type_steer_opp;
        memcpy(start_steer_opp_timer_params.sta_mac, sta_info->id, sizeof(mac_address_t));
        memcpy(start_steer_opp_timer_params.source_bssid, source_bssid, sizeof(bssid_t));
        start_steer_opp_timer_params.duration_ms = steer_opp_win * 1000;

        em_printfout("Starting steer opportunity window: %u sec for STA %s\n", steer_opp_win, util::mac_to_string(sta_info->id).c_str());
        get_mgr()->io_process(em_bus_event_type_start_sta_timer, reinterpret_cast<unsigned char *>(&start_steer_opp_timer_params), sizeof(em_sta_timer_t));
        sta_info->sta_timer_active |= em_sta_timer_type_steer_opp;
    }

    return 0;
}

int em_steering_t::handle_client_steering_report(unsigned char *buff, unsigned int len)
{
    em_tlv_t *tlv;
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    em_steering_btm_rprt_t *btm_rprt;
    dm_easy_mesh_t *dm;
    em_device_info_t *dev_info;
    em_cmdu_t *cmdu;

    dm = get_data_model();
    dev_info = dm->get_device_info();
    if (dev_info == NULL) {
        printf("%s:%d: Device info not found\n", __func__, __LINE__);
        return -1;    
    }

    if (em_msg_t(em_msg_type_client_steering_btm_rprt, dev_info->profile, buff, len).validate(errors) == 0) {
        printf("%s:%d:Client Steering Request message validation failed\n",__func__,__LINE__);
        return -1;
    }

    cmdu = reinterpret_cast<em_cmdu_t *> (buff + sizeof(em_raw_hdr_t));
    tlv = reinterpret_cast<em_tlv_t *> (buff + sizeof(em_raw_hdr_t) + sizeof(em_cmdu_t));

    btm_rprt = reinterpret_cast<em_steering_btm_rprt_t *> (&tlv->value);

    mac_addr_str_t mac_str, target_str;
    dm_easy_mesh_t::macbytes_to_string(btm_rprt->sta_mac_addr, mac_str);
    dm_easy_mesh_t::macbytes_to_string(btm_rprt->target_bssid, target_str);
    em_printfout("%s:%d BTM Report: sta=%s status=%d target=%s\n",
        __func__, __LINE__, mac_str, btm_rprt->btm_status_code, target_str);

    if (btm_rprt->btm_status_code == BTM_STATUS_ACCEPT) {
        // STA accepted the BTM — steering succeeded on Controller side
        em_printfout("%s:%d BTM accepted by sta=%s, steering succeeded\n", __func__, __LINE__, mac_str);
    } else {
        // STA rejected the BTM — log the rejection reason and retry
        em_printfout("%s:%d BTM rejected by sta=%s (status=%d), will retry steering request\n",
            __func__, __LINE__, mac_str, btm_rprt->btm_status_code);
    }
    send_1905_ack_message(ntohs(cmdu->id));

    return 0;
}

int em_steering_t::handle_ack_msg(unsigned char *buff, unsigned int len)
{
    em_printfout("%s:%d: Received ACK message\n", __func__, __LINE__);
    if(get_state() == em_state_agent_steer_btm_rpt_pending || get_state() == em_state_agent_steer_complete) {
        set_state(em_state_agent_configured);
    } else if(get_state() == em_state_ctrl_sta_steer_pending) {
        set_state(em_state_ctrl_steer_req_ack_rcvd);
    }

    em_printfout("%s:%d: Set state to %d\n", __func__, __LINE__, get_state());
    return 0;
}

int em_steering_t::handle_client_assoc_ctrl_req(unsigned char *buff, unsigned int len)
{
    printf("%s:%d: Recived Client Assoc Control Request from Controller\n", __func__, __LINE__);

    em_tlv_t *tlv;
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    em_client_assoc_ctrl_req_t *assoc_ctrl_req;

    em_raw_hdr_t *hdr = reinterpret_cast<em_raw_hdr_t *> (buff);
    em_cmdu_t *cmdu = reinterpret_cast<em_cmdu_t *> (buff + sizeof(em_raw_hdr_t));
    unsigned short msg_id = ntohs(cmdu->id);
    dm_easy_mesh_t *dm = get_data_model();
    bool has_errors = false;

    if (em_msg_t(em_msg_type_client_assoc_ctrl_req, em_profile_type_3, buff, len).validate(errors) == 0) {
        printf("%s:%d: Client Association Control Request message validation failed\n", __func__, __LINE__);
        return -1;
    }

    tlv = reinterpret_cast<em_tlv_t *> (buff + sizeof(em_raw_hdr_t) + sizeof(em_cmdu_t));
    assoc_ctrl_req = reinterpret_cast<em_client_assoc_ctrl_req_t *> (&tlv->value);

    if (assoc_ctrl_req->assoc_control == 0x00) {  // Block
        for (unsigned char i = 0; i < assoc_ctrl_req->count; i++) {
            mac_address_t sta_mac;
            memcpy(sta_mac, assoc_ctrl_req->sta_mac, sizeof(mac_address_t));
            // Check for associated STAs if blocking
            if (dm->is_sta_associated(assoc_ctrl_req->bssid, sta_mac)) {
                // Send Ack with error
                printf("%s:%d: Sending ack with Error Code TLV\n", __func__, __LINE__);
                send_1905_ack_message(sta_mac, msg_id);
                has_errors = true;
            }
        }
    }

    if (!has_errors) {
        send_1905_ack_message(assoc_ctrl_req->sta_mac, msg_id);
    }

    // Send the association control request to OneWifi for HAL enforcement
    get_mgr()->io_process(em_bus_event_type_client_assoc_ctrl_req,
        reinterpret_cast<unsigned char *> (assoc_ctrl_req), sizeof(em_client_assoc_ctrl_req_t)
	+ assoc_ctrl_req->count * sizeof(mac_address_t));

    return 0;
}

int em_steering_t::send_steering_complete_msg()
{
    unsigned char buff[MAX_EM_BUFF_SZ];
    char *errors[EM_MAX_TLV_MEMBERS] = {0};
    unsigned short msg_type = em_msg_type_steering_complete;
    size_t len = 0;
    em_cmdu_t *cmdu;
    em_tlv_t *tlv;
    unsigned char *tmp = buff;
    unsigned short type = htons(ETH_P_1905);
    dm_easy_mesh_t *dm = get_data_model();
    em_device_info_t *dev_info;

    dev_info = dm->get_device_info();
    if (dev_info == NULL) {
        printf("%s:%d: Device info not found\n", __func__, __LINE__);
        return -1;
    }

    // Dest: Controller AL MAC
    memcpy(tmp, dm->get_controller_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    // Src: Agent AL MAC
    memcpy(tmp, dm->get_agent_al_interface_mac(), sizeof(mac_address_t));
    tmp += sizeof(mac_address_t);
    len += sizeof(mac_address_t);

    memcpy(tmp, reinterpret_cast<unsigned char *>(&type), sizeof(unsigned short));
    tmp += sizeof(unsigned short);
    len += sizeof(unsigned short);

    cmdu = reinterpret_cast<em_cmdu_t *>(tmp);
    memset(tmp, 0, sizeof(em_cmdu_t));
    cmdu->type = htons(msg_type);
    cmdu->id = htons(get_mgr()->get_next_msg_id());
    cmdu->last_frag_ind = 1;
    cmdu->relay_ind = 0;

    tmp += sizeof(em_cmdu_t);
    len += sizeof(em_cmdu_t);

    // Steering Complete has no TLVs per §17.1.30 — just End of Message
    tlv = reinterpret_cast<em_tlv_t *>(tmp);
    tlv->type = em_tlv_type_eom;
    tlv->len = 0;

    tmp += sizeof(em_tlv_t);
    len += sizeof(em_tlv_t);

    if (em_msg_t(em_msg_type_steering_complete, dev_info->profile, buff,
            static_cast<unsigned int>(len)).validate(errors) == 0) {
        em_printfout("%s:%d: Steering Complete msg validation failed\n", __func__, __LINE__);
        return -1;
    }

    if (send_frame(buff, static_cast<unsigned int>(len)) < 0) {
        em_printfout("%s:%d: Steering Complete msg send failed, error:%d\n", __func__, __LINE__, errno);
        return -1;
    }

    em_printfout("%s:%d Steering Complete message sent successfully\n", __func__, __LINE__);
    return static_cast<int>(len);
}

int em_steering_t::handle_steering_complete(unsigned char *buff, unsigned int len)
{
    em_cmdu_t *cmdu;
    dm_easy_mesh_t *dm = get_data_model();

    if(dm == NULL) {
        printf("%s:%d: Data model not found\n", __func__, __LINE__);
        return -1;
    }

    em_printfout("%s:%d Received Steering Complete message from Agent, sending Ack\n", __func__, __LINE__);
    cmdu = reinterpret_cast<em_cmdu_t *> (buff + sizeof(em_raw_hdr_t));

    send_1905_ack_message(ntohs(cmdu->id));

    return 0;
}

void em_steering_t::process_ctrl_state()
{
    switch (get_state()) {
        case em_state_ctrl_sta_steer_pending:
            send_client_steering_req_msg();
            break;

        case em_state_ctrl_sta_disassoc_pending:
            send_client_assoc_ctrl_req_msg();
            break;

        default:
            break;
    }
}

void em_steering_t::process_agent_state()
{
    em_printfout("%s:%d: Process agent state: %d\n", __func__, __LINE__, get_state());
    switch (get_state()) {
        case em_state_agent_steer_btm_rpt_pending:
            send_btm_report_msg();
            break;

        case em_state_agent_steer_complete:
            send_steering_complete_msg();
            break;

        default:
            break;
    }
}

void em_steering_t::process_msg(unsigned char *data, unsigned int len)
{
    em_cmdu_t *cmdu = reinterpret_cast<em_cmdu_t *> (data + sizeof(em_raw_hdr_t));

    switch (htons(cmdu->type)) {
        case em_msg_type_client_steering_req:
            handle_client_steering_req(data, len);
            break;

        case em_msg_type_client_steering_btm_rprt:
            handle_client_steering_report(data, len);
            break;

        case em_msg_type_client_assoc_ctrl_req:
            handle_client_assoc_ctrl_req(data, len);
            break;

        case em_msg_type_1905_ack:
            handle_ack_msg(data, len);
            break;

        case em_msg_type_steering_complete:
            handle_steering_complete(data, len);
            break;

        default:
            break;
    }
}

em_steering_t::em_steering_t()
{
    m_client_steering_req_tx_cnt = 0;
    m_client_assoc_ctrl_req_tx_cnt = 0;
}

em_steering_t::~em_steering_t()
{

}
