/**
 * Copyright 2026 Comcast Cable Communications Management, LLC
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

#include "tr_181.h"
#include "util.h"
#include <cjson/cJSON.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "em_ctrl.h"

// Utility: Trim leading/trailing whitespace from a string (in-place)
void tr_181_t::tr181_trim_whitespace(char *str)
{
    if (!str) {
        return;
    }
    char *start = str;
    while (isspace(static_cast<unsigned char>(*start))) {
        start++;
    }
    if (*start == '\0') {
        str[0] = '\0';
        return;
    }
    char *end = start + strlen(start) - 1;
    while (end > start && isspace(static_cast<unsigned char>(*end))) {
        end--;
    }
    size_t len = static_cast<size_t>(end - start + 1);
    if (start != str) {
        memmove(str, start, len);
    }
    str[len] = '\0';
}

// Create a HaulType array from a single value (Fronthaul/Backhaul only)
cJSON *tr_181_t::create_haultype_array(const char *haul_val)
{
    if (!haul_val || *haul_val == '\0') return NULL;
    if (strchr(haul_val, ',')) {
        em_printfout("ERROR: HaulType must be a single value (no commas): '%s'", haul_val);
        return NULL;
    }
    char tok_buf[TR181_HAULTYPE_MAX_LEN + 1];
    size_t val_len = strnlen(haul_val, sizeof(tok_buf));
    if (val_len >= sizeof(tok_buf)) {
        em_printfout("ERROR: HaulType too long");
        return NULL;
    }
    memcpy(tok_buf, haul_val, val_len + 1);
    tr181_trim_whitespace(tok_buf);
    if (*tok_buf == '\0') {
        em_printfout("ERROR: HaulType empty after trimming");
        return NULL;
    }
    if (strcmp(tok_buf, "Fronthaul") != 0 && strcmp(tok_buf, "Backhaul") != 0) {
        em_printfout("ERROR: Invalid HaulType value '%s' (expected Fronthaul/Backhaul)", tok_buf);
        return NULL;
    }
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        return NULL;
    }
    cJSON *val = cJSON_CreateString(tok_buf);
    if (!val) {
        cJSON_Delete(arr);
        return NULL;
    }
    cJSON_AddItemToArray(arr, val);
    return arr;
}

// Return true if item's HaulType array contains the provided value
bool tr_181_t::item_matches_haultype(const cJSON *item, const char *haul_val)
{
    if (!item || !haul_val || *haul_val == '\0') return false;
    char tok_buf[TR181_HAULTYPE_MAX_LEN + 1];
    size_t val_len = strnlen(haul_val, sizeof(tok_buf));
    if (val_len >= sizeof(tok_buf)) {
        return false;
    }
    memcpy(tok_buf, haul_val, val_len + 1);
    tr181_trim_whitespace(tok_buf);
    if (*tok_buf == '\0') return false;
    const cJSON *haul_arr = cJSON_GetObjectItem(item, "HaulType");
    if (!haul_arr || !cJSON_IsArray(haul_arr)) return false;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, haul_arr) {
        if (cJSON_IsString(entry) && entry->valuestring && strcmp(entry->valuestring, tok_buf) == 0) {
            return true;
        }
    }
    return false;
}

// Format HaulType array as comma-separated string
size_t tr_181_t::format_haultype_list(const cJSON *item, char *out, size_t out_len)
{
    if (!out || out_len == 0) return 0;
    out[0] = '\0';
    if (!item) return 0;
    const cJSON *haul_arr = cJSON_GetObjectItem(item, "HaulType");
    if (!haul_arr || !cJSON_IsArray(haul_arr)) return 0;
    bool first = true;
    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, haul_arr) {
        if (!cJSON_IsString(entry) || !entry->valuestring) continue;
        size_t len = strlen(out);
        if (len >= out_len - 1) break;
        snprintf(out + len, out_len - len, "%s%s", first ? "" : ",", entry->valuestring);
        first = false;
    }
    return strlen(out);
}

// Create a string property with the given name and value.
bus_data_prop_t *tr_181_t::tr181_alloc_string_prop(const char *name, const char *value)
{
    if (!name || !value) return nullptr;

    bus_data_prop_t *prop = static_cast<bus_data_prop_t *>(calloc(1, sizeof(bus_data_prop_t)));
    if (!prop) return nullptr;

    size_t name_len = strnlen(name, sizeof(prop->name) - 1U);
    memcpy(prop->name, name, name_len);
    prop->name[name_len] = '\0';
    prop->name_len = static_cast<uint32_t>(name_len);
    prop->is_data_set = true;
    prop->status = bus_error_success;

    size_t value_len = strlen(value);
    prop->value.data_type = bus_data_type_string;
    prop->value.raw_data.bytes = malloc(value_len + 1U);
    if (!prop->value.raw_data.bytes) {
        free(prop);
        return nullptr;
    }
    memcpy(prop->value.raw_data.bytes, value, value_len + 1U);
    prop->value.raw_data_len = static_cast<unsigned int>(value_len + 1U);
    return prop;
}

// Build a "Status" output property with the given status string.
bus_data_prop_t *tr_181_t::tr181_set_status_output_prop(const char *status)
{
    return status ? tr181_alloc_string_prop("Status", status) : nullptr;
}

// Populate output_data with a "Status" property containing the given status string.
void tr_181_t::tr181_set_status_output(raw_data_t *output_data, const char *status)
{
    if (!output_data) return;
    bus_data_prop_t *prop = tr181_set_status_output_prop(status);
    if (!prop) return;
    output_data->data_type = bus_data_type_property;
    output_data->raw_data.bytes = prop;
    output_data->raw_data_len = sizeof(bus_data_prop_t);
}

// Copy a string property value into a destination buffer, ensuring proper type and null-termination.
bool tr_181_t::tr181_copy_prop_string(const bus_data_prop_t *prop, char *dst, size_t dst_len)
{
    if (!prop || !dst || (dst_len == 0U)) {
        return false;
    }
    if ((prop->value.data_type != bus_data_type_string) || !prop->value.raw_data.bytes) {
        return false;
    }

    const char *src = static_cast<const char *>(prop->value.raw_data.bytes);
    size_t len = prop->value.raw_data_len;
    if (len == 0U) {
        len = strnlen(src, dst_len - 1U);
    } else if (src[len - 1U] == '\0') {
        len -= 1U;
    }
    if (len >= dst_len) {
        len = dst_len - 1U;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
    return true;
}

bus_error_t tr_181_t::get_sta_mac_from_event_name(char *event_name, mac_addr_str_t sta_mac_out)
{
    dm_easy_mesh_ctrl_t *dm_ctrl;
    dm_easy_mesh_t *dm;
    const char *name;
    char instance[MAX_INSTANCE_LEN] = {0};
    bool is_num = false;

    if (event_name == nullptr || em_ctrl_t::get_em_ctrl_instance() == nullptr) {
        return bus_error_invalid_input;
    }

    dm_ctrl = em_ctrl_t::get_em_ctrl_instance()->get_dm_ctrl();
    if (dm_ctrl == nullptr) {
        return bus_error_invalid_input;
    }

    sta_mac_out[0] = '\0';

    static constexpr const char *dataelements_network_prefix = DATAELEMS_NETWORK; // "Device.WiFi.DataElements.Network."
    const size_t prefix_len = std::strlen(dataelements_network_prefix);

    if (std::strncmp((const char *)event_name, dataelements_network_prefix, prefix_len) != 0) {
        return bus_error_invalid_namespace;
    }

    // Expected suffix format:
    // Device.<d>.Radio.<r>.BSS.<b>.STA.<s>.   (trailing '.' may exist)
    name = (const char *)event_name + prefix_len;

    // Device instance -> dm object
    name = dm_ctrl->get_table_instance(name, instance, sizeof(instance), &is_num);
    if (!is_num || instance[0] == '\0') {
        return bus_error_invalid_namespace;
    }

    dm = dm_ctrl->get_dm_easy_mesh(instance, true);
    if (dm == nullptr) {
        return bus_error_invalid_namespace;
    }

    // Radio instance
    name = dm_ctrl->get_table_instance(name, instance, sizeof(instance), &is_num);
    const int radio_instance = std::atoi(instance);
    if (radio_instance <= 0 || static_cast<unsigned int>(radio_instance) > dm->m_num_radios) {
        return bus_error_invalid_namespace;
    }

    dm_radio_t *radio = &dm->m_radio[radio_instance - 1];
    if (radio == nullptr) {
        return bus_error_invalid_input;
    }

    em_radio_info_t *ri = radio->get_radio_info();
    if (ri == nullptr) {
        return bus_error_invalid_input;
    }

    // BSS instance (instance within selected radio)
    name = dm_ctrl->get_table_instance(name, instance, sizeof(instance), &is_num);
    if (!is_num || instance[0] == '\0') {
        return bus_error_invalid_namespace;
    }
    const int bss_instance = std::atoi(instance);
    if (bss_instance <= 0) {
        return bus_error_invalid_namespace;
    }

    em_bss_info_t *bi = nullptr;
    int bss_count_for_radio = 0;
    for (unsigned int i = 0; i < dm->m_num_bss; i++) {
        em_bss_info_t *cur_bss = dm->get_bss_info(i);
        if (cur_bss == nullptr) {
            continue;
        }

        if (std::memcmp(ri->id.ruid, cur_bss->ruid.mac, sizeof(mac_address_t)) == 0) {
            ++bss_count_for_radio;
            if (bss_count_for_radio == bss_instance) {
                bi = cur_bss;
                break;
            }
        }
    }

    if (bi == nullptr) {
        return bus_error_invalid_namespace;
    }

    // STA instance (instance within selected BSS)
    name = dm_ctrl->get_table_instance(name, instance, sizeof(instance), &is_num);
    if (!is_num || instance[0] == '\0') {
        return bus_error_invalid_namespace;
    }
    const int sta_instance = std::atoi(instance);
    if (sta_instance <= 0) {
        return bus_error_invalid_namespace;
    }

    dm_sta_t *sta = dm_ctrl->get_dm_sta(dm, bi, sta_instance);
    if (sta == nullptr) {
        return bus_error_invalid_namespace;
    }

    em_sta_info_t *si = sta->get_sta_info();
    if (si == nullptr) {
        return bus_error_invalid_input;
    }

    dm_easy_mesh_t::macbytes_to_string(si->id, sta_mac_out);

    return bus_error_success;
}

cJSON *tr_181_t::find_target_sta(cJSON *sta_list_obj, const char *sta_mac)
{
    if (sta_list_obj == nullptr || sta_mac == nullptr) return nullptr;

    // Case 1: STA list as array
    if (cJSON_IsArray(sta_list_obj)) {
        cJSON *sta = nullptr;
        cJSON_ArrayForEach(sta, sta_list_obj) {
            cJSON *mac = cJSON_GetObjectItemCaseSensitive(sta, "MACAddress");
            if (cJSON_IsString(mac) && util::mac_equals(mac->valuestring, sta_mac)) {
                return sta;
            }
        }
        return nullptr;
    }

    // Case 2: object keyed by MAC
    if (cJSON_IsObject(sta_list_obj)) {
        for (cJSON *it = sta_list_obj->child; it != nullptr; it = it->next) {
            // key is MAC
            if (it->string != nullptr && util::mac_equals(it->string, sta_mac)) {
                return it;
            }
            // value may be STA object with MACAddress
            if (cJSON_IsObject(it)) {
                cJSON *mac = cJSON_GetObjectItemCaseSensitive(it, "MACAddress");
                if (cJSON_IsString(mac) && util::mac_equals(mac->valuestring, sta_mac)) {
                    return it;
                }
            }
        }

        // Case 3: wrapper object { "STAList": [...] }
        cJSON *inner_list = cJSON_GetObjectItemCaseSensitive(sta_list_obj, "STAList");
        if (inner_list != nullptr) {
            return find_target_sta(inner_list, sta_mac);
        }
    }

    return nullptr;
}
