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
 #include <cjson/cJSON.h>
 
#include "em_ctrl.h"
#include "tr_181.h"
#include "em_cli_apis.h"
#include "util.h"

extern em_ctrl_t g_ctrl;
extern char *global_netid;

//#define MAX_PARAM_LEN 128
#define MAX_PARAM_LEN 64

#if 0
//TODO: Rbus abstraction needed for this async method call, it will be enabled once its ready
bus_error_t em_ctrl_t::cmd_setssid(const char *event_name, bus_data_prop_t const *input_data, bus_data_prop_t *output_data, void *user_data)
{
    em_subdoc_info_t *subdoc = NULL;
    unsigned char buff[EM_IO_BUFF_SZ];
    cJSON *json = NULL, *root = NULL, *new_json = NULL, *ssid_list = NULL, *target = NULL, *item = NULL, *ssid_item = NULL, *child = NULL, *next = NULL, *band_arr = NULL, *akm_arr = NULL, *json_obj = NULL;
    char *jsonbuff = NULL, *updated_json = NULL, *new_json_str = NULL;
    bus_data_prop_t const *prop = NULL;
    char ssid[MAX_PARAM_LEN] = {0};
    char passphrase[MAX_PARAM_LEN] = {0};
    char band[MAX_PARAM_LEN] = {0};
    char akms[MAX_PARAM_LEN] = {0};
    char addremove[MAX_PARAM_LEN] = {0};
    int idx = 0;
    size_t json_len = 0;
    
    em_printfout("Received parameters in cmd_setssid");
    prop = input_data;
    while (prop) {
        em_printfout("Param %d: name='%s', value='%s', len=%u", idx, prop->name, (char*)prop->value.raw_data.bytes, prop->value.raw_data_len);
        prop = prop->next_data;
        idx++;
    }

    // Extract parameters from input_data
    prop = input_data;
    while (prop) {
        em_printfout("%s:%d name='%s', value='%.*s', len=%u\n", prop->name, (int)prop->value.raw_data_len, (char*)prop->value.raw_data.bytes, prop->value.raw_data_len, __func__, __LINE__);
        if (strcmp(prop->name, "SSID") == 0) {
            strncpy(ssid, (char*)prop->value.raw_data.bytes, MAX_PARAM_LEN - 1);
            ssid[MAX_PARAM_LEN - 1] = '\0';
        } else if (strcmp(prop->name, "PassPhrase") == 0) {
            strncpy(passphrase, (char*)prop->value.raw_data.bytes, MAX_PARAM_LEN - 1);
            passphrase[MAX_PARAM_LEN - 1] = '\0';
        } else if (strcmp(prop->name, "Band") == 0) {
            strncpy(band, (char*)prop->value.raw_data.bytes, MAX_PARAM_LEN - 1);
            band[MAX_PARAM_LEN - 1] = '\0';
        } else if (strcmp(prop->name, "AKMsAllowed") == 0) {
            strncpy(akms, (char*)prop->value.raw_data.bytes, MAX_PARAM_LEN - 1);
            akms[MAX_PARAM_LEN - 1] = '\0';
        } else if (strcmp(prop->name, "AddRemoveChange") == 0) {
            strncpy(addremove, (char*)prop->value.raw_data.bytes, MAX_PARAM_LEN - 1);
            addremove[MAX_PARAM_LEN - 1] = '\0';
        }
        prop = prop->next_data;
    }
    
    if (ssid[0] == '\0' || passphrase[0] == '\0' || band[0] == '\0' || akms[0] == '\0' || addremove[0] == '\0') {
        em_printfout("ERROR: Missing required parameters in cmd_setssid");
        return bus_error_invalid_input;
    }

    subdoc = (em_subdoc_info_t *)buff;
    strncpy(subdoc->name, "NetworkSSIDList", strlen("NetworkSSIDList"));
    g_ctrl.m_data_model.get_config("OneWifiMesh", subdoc);
    em_printfout("%s:%d: buff=%s \n", __func__, __LINE__, subdoc->buff );
    json = cJSON_Parse(subdoc->buff);
    if (json == NULL) {
        em_printfout("ERROR: Failed to parse JSON from subdoc");
        return bus_error_invalid_input;
    }

    root = cJSON_CreateObject();
    // Add "ID" to the beginning of the JSON object
    new_json = cJSON_CreateObject();
    cJSON_AddStringToObject(new_json, "ID", "OneWifiMesh");

    // Move all items from the original json to new_json
    child = json->child;
    while (child) {
        next = child->next;
        cJSON_DetachItemViaPointer(json, child);
        cJSON_AddItemToObject(new_json, child->string, child);
        child = next;
    }
    cJSON_Delete(json);
    json = new_json;

    cJSON_AddItemToObject(root, "wfa-dataelements:SetSSID", json);
    jsonbuff = cJSON_Print(root);
    em_printfout("%s:%d root: %s\n", __func__, __LINE__, jsonbuff);
    free(jsonbuff);

    // Find or add the SSID entry
    ssid_list = cJSON_GetObjectItem(json, "NetworkSSIDList");
    if (ssid_list == NULL || !cJSON_IsArray(ssid_list)) {
        em_printfout("ERROR: NetworkSSIDList not found or is not an array");
        cJSON_Delete(json);
        return bus_error_invalid_input;
    }
    cJSON_ArrayForEach(item, ssid_list) {
        ssid_item = cJSON_GetObjectItem(item, "SSID");
        if (ssid_item && ssid && strcmp(ssid_item->valuestring, ssid) == 0) {
            target = item;
            em_printfout("Matching SSID found: %s", ssid_item->valuestring);
            break;
        }
        //TBD: If not found, update fronthaul for now
        //check if ssid named private_ssdid exists
        ssid_item = cJSON_GetObjectItem(item, "SSID");
        if (ssid_item && ssid && strcmp(ssid_item->valuestring, "private_ssid") == 0) {
            target = item;
            em_printfout("private_ssid found: %s\n", ssid_item->valuestring);
            break;
        }
    }
    em_printfout("Target SSID entry: %s", target ? "Found" : "Not Found");
    if (target) {
        em_printfout("Replace existing SSID");
        // Replace all fields for the existing SSID entry
        if (ssid[0]) {
            cJSON_ReplaceItemInObject(target, "SSID", cJSON_CreateString(ssid));
        }
        if (passphrase[0]) {
            cJSON_ReplaceItemInObject(target, "PassPhrase", cJSON_CreateString(passphrase));
        }
        if (band[0]) {
            band_arr = cJSON_CreateArray();
            cJSON_AddItemToArray(band_arr, cJSON_CreateString(band));
            cJSON_ReplaceItemInObject(target, "Band", band_arr);
        }
        if (akms[0]) {
            akm_arr = cJSON_CreateArray();
            cJSON_AddItemToArray(akm_arr, cJSON_CreateString(akms));
            cJSON_ReplaceItemInObject(target, "AKMsAllowed", akm_arr);
        }
    } else if (addremove[0] && strcmp(addremove, "Add") == 0) {
        em_printfout("ADD (new SSID)");
        // Add new entry if SSID does not exist
        target = cJSON_CreateObject();
        cJSON_AddItemToArray(ssid_list, target);
        if (ssid[0]) {
            cJSON_AddStringToObject(target, "SSID", ssid);
        }
        if (passphrase[0]) {
            cJSON_AddStringToObject(target, "PassPhrase", passphrase);
        }
        if (band[0]) {
            band_arr = cJSON_CreateArray();
            cJSON_AddItemToArray(band_arr, cJSON_CreateString(band));
            cJSON_AddItemToObject(target, "Band", band_arr);
        }
        if (akms[0]) {
            akm_arr = cJSON_CreateArray();
            cJSON_AddItemToArray(akm_arr, cJSON_CreateString(akms));
            cJSON_AddItemToObject(target, "AKMsAllowed", akm_arr);
        }
    }

    updated_json = cJSON_PrintUnformatted(root);
    json_len = strlen(updated_json);
    if (json_len >= EM_IO_BUFF_SZ) {
        em_printfout("ERROR: JSON too large for buffer!");
        free(updated_json);
        cJSON_Delete(json);
        return bus_error_invalid_input;
    }
    
    memcpy(subdoc->buff, updated_json, json_len);
    subdoc->buff[json_len] = '\0';
    json_obj = cJSON_Parse(subdoc->buff);
    if (json_obj) {
        char *new_json = cJSON_Print(json_obj);
        em_printfout("Updated and formatted JSON:\n%s", new_json);
        free(new_json);
        cJSON_Delete(json_obj);
    } else {
        em_printfout("Invalid JSON in subdoc->buff");
    }

    g_ctrl.io_process(em_bus_event_type_set_ssid, subdoc->buff, strlen(subdoc->buff));
    free(updated_json);
    cJSON_Delete(json);

    return bus_error_success;
}
#endif

bus_error_t bus_set_cb_fwd(char *event_name, raw_data_t *p_data, bus_user_data_t *user_data, bus_set_handler_t cb)
{
    (void)user_data;
    uint32_t s_id;
    bus_error_t err = bus_error_success;
    em_event_t *req;
    bus_resp_get_t *resp = NULL;
    uintptr_t buf;

    do {
        req = (em_event_t *) malloc(sizeof(em_event_t));
        if(!req) {
            err = bus_error_out_of_resources;
            break;
        }
        em_printfout("%s:%d AUTOCONFIG_DEBUG event_name:%s \n", __func__, __LINE__, event_name);
        s_id = g_ctrl.get_next_nb_evt_id();
        req->type = em_event_type_nb;
        req->u.nevt.id = s_id;
        req->u.nevt.type = NB_REQTYPE_METHOD;
        req->u.nevt.u.method.method = event_name;
        req->u.nevt.u.method.in = p_data;
        req->u.nevt.u.method.out = NULL;
        req->u.nevt.u.method.async = NULL;
        req->u.nevt.cb = (void *) cb;

        g_ctrl.push_to_queue(req);

        em_printfout("%s:%d AUTOCONFIG_DEBUG Reading from pipe \n", __func__, __LINE__);
        ssize_t len = read(g_ctrl.get_nb_pipe_rd(), &buf, sizeof(buf));
        assert(len == sizeof(buf));
        resp = (bus_resp_get_t *) buf;
        em_printfout("%s:%d AUTOCONFIG_DEBUG resp->id:%d \n", __func__, __LINE__, resp->id);
        assert(resp->id == s_id);
        err = resp->rc;
        em_printfout("%s:%d Reached End of Do \n", __func__, __LINE__);
    } while(0);

    return err;
}

bus_error_t bus_method_cb_fwd(char const* methodName, raw_data_t *inParams, raw_data_t *outParams, void *asyncHandle, bus_method_handler_t cb)
{
    uint32_t s_id;
    bus_error_t err = bus_error_success;
    em_event_t *req;
    bus_resp_get_t *resp = NULL;
    uintptr_t buf;

    do {
        req = (em_event_t *) malloc(sizeof(em_event_t));
        if(!req) {
            err = bus_error_out_of_resources;
            break;
        }
        em_printfout("%s:%d AUTOCONFIG_DEBUG methodName:%s \n", __func__, __LINE__, methodName);
        s_id = g_ctrl.get_next_nb_evt_id();
        req->type = em_event_type_nb;
        req->u.nevt.id = s_id;
        req->u.nevt.type = NB_REQTYPE_METHOD;
        req->u.nevt.u.method.method = methodName;
        req->u.nevt.u.method.in = inParams;
        req->u.nevt.u.method.out = outParams;
        req->u.nevt.u.method.async = asyncHandle;
        req->u.nevt.cb = (void *) cb;

        g_ctrl.push_to_queue(req);

        em_printfout("%s:%d AUTOCONFIG_DEBUG Reading from pipe \n", __func__, __LINE__);
        ssize_t len = read(g_ctrl.get_nb_pipe_rd(), &buf, sizeof(buf));
        assert(len == sizeof(buf));
        resp = (bus_resp_get_t *) buf;
        em_printfout("%s:%d AUTOCONFIG_DEBUG resp->id:%d \n", __func__, __LINE__, resp->id);
        assert(resp->id == s_id);
        err = resp->rc;
        em_printfout("%s:%d Reached End of Do \n", __func__, __LINE__);
    } while(0);

    return err;
}

bus_error_t validate_ssid_input_data (cJSON *input_data, const char *input_name) {
    cJSON *item = NULL;
    em_printfout("%s:%d AUTOCONFIG_DEBUG validating input_data:%s input_name:%s\n", __func__, __LINE__, input_data->string, input_name);

    if(strncmp(input_name, "SSID", strlen("SSID")) == 0) {
        if(!cJSON_IsString(input_data) || strlen(input_data->valuestring) > MAX_PARAM_LEN) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG SSID must be a string with a max of 64 characters\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
    } else if(strncmp(input_name, "AddRemoveChange", strlen("AddRemoveChange")) == 0) {
        if(!cJSON_IsString(input_data) || !(strncmp(input_data->valuestring, "Add", strlen("Add")) == 0 || strncmp(input_data->valuestring, "Remove", strlen("Remove")) == 0 || 
            strncmp(input_data->valuestring, "Change", strlen("Change")) == 0)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG AddRemoveChange must be a string with one or many of these values: Add, Remove or Change\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
    } else if(strncmp(input_name, "PassPhrase", strlen("PassPhrase")) == 0) {
        if(!cJSON_IsString(input_data) || strlen(input_data->valuestring) < 8 || strlen(input_data->valuestring) > MAX_PARAM_LEN) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG PassPhrase must be a string b/w 8-63 characters\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
    } else if(strncmp(input_name, "Enable", strlen("Enable")) == 0) {
        if(!cJSON_IsBool(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG Enable must be a boolean\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
    } else if(strncmp(input_name, "Band", strlen("Band")) == 0) {
        if(!cJSON_IsArray(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG Band must be an array\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
        cJSON_ArrayForEach(item, input_data) {
            if(!cJSON_IsString(item) || !(strncmp(item->valuestring, "2.4", strlen("2.4")) == 0 || strncmp(item->valuestring, "5", strlen("5")) == 0 || 
                strncmp(item->valuestring, "6", strlen("6")) == 0)) {
                em_printfout("%s:%d AUTOCONFIG_DEBUG Band must be string with one or many of these values: 2.4, 5 or 6\n", __func__, __LINE__);
                return bus_error_invalid_input;
            }
        }
    } else if(strncmp(input_name, "AKMsAllowed", strlen("AKMsAllowed")) == 0) {
        if(!cJSON_IsArray(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG AKMsAllowed must be an array\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
        cJSON_ArrayForEach(item, input_data) {
            if(!cJSON_IsString(item) || !(strncmp(item->valuestring, "psk", strlen("psk")) == 0 || strncmp(item->valuestring, "dpp", strlen("dpp")) == 0 || 
                strncmp(item->valuestring, "sae", strlen("sae")) == 0 || strncmp(item->valuestring, "psk+sae", strlen("psk+sae")) == 0 || 
                strncmp(item->valuestring, "dpp+sae", strlen("dpp+sae")) == 0 || strncmp(item->valuestring, "dpp+psk+sae", strlen("dpp+psk+sae")) == 0) ||
                (strncmp(item->valuestring, "SuiteSelector", strlen("SuiteSelector")) == 0)) {
                em_printfout("%s:%d AUTOCONFIG_DEBUG AKMs must be string with one or many of these values: psk, sae, dpp, psk+sae, dpp+sae, dpp+psk+sae or SuiteSelector\n", __func__, __LINE__);
                return bus_error_invalid_input;
            }
        }
    } else if(strncmp(input_name, "SuiteSelector", strlen("SuiteSelector")) == 0) {
        if(!cJSON_IsString(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG SuiteSelector must be a string\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
        for(char ch: std::string(input_data->valuestring)) {
            if(!isxdigit(ch)) {
                em_printfout("%s:%d SuiteSelector should be in Hex Format. %c is not a valid hex character\n", __func__, __LINE__, ch);
                return bus_error_invalid_input;
            }
        }
    } else if(strncmp(input_name, "AdvertisementEnabled", strlen("AdvertisementEnabled")) == 0) {
        if(!cJSON_IsBool(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG AdvertisementEnabled must be a boolean\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
    } else if(strncmp(input_name, "MFPConfig", strlen("MFPConfig")) == 0) {
        if(!cJSON_IsString(input_data) || !((strncmp(input_data->valuestring, "Disabled", strlen("Disabled")) == 0) ||
            (strncmp(input_data->valuestring, "Optional", strlen("Optional")) == 0) || (strncmp(input_data->valuestring, "Required", strlen("Required")) == 0))) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG MFPConfig must be a string\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
    } else if(strncmp(input_name, "MobilityDomain", strlen("MobilityDomain")) == 0) {
        if(!cJSON_IsString(input_data) && !cJSON_IsArray(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG MobilityDomain must be a string or array of strings\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
        if(cJSON_IsArray(input_data)) {
            cJSON_ArrayForEach(item, input_data) {
                for(char ch: std::string(item->valuestring)) {
                    if(!isxdigit(ch) || ch == ':') {
                        em_printfout("%s:%d MobilityDomain should be in Hex Format. %c is not a valid hex character\n", __func__, __LINE__, ch);
                        return bus_error_invalid_input;
                    }
                }
            }
        } else {
            for(char ch: std::string(input_data->valuestring)) {
                if(!isxdigit(ch) || ch == ':') {
                    em_printfout("%s:%d MobilityDomain should be in Hex Format. %c is not a valid hex character\n", __func__, __LINE__, ch);
                    return bus_error_invalid_input;
                }
            }
        }
    } else if(strncmp(input_name, "HaulType", strlen("HaulType")) == 0) {
        if(!cJSON_IsArray(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG HaulType must be an array\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
        cJSON_ArrayForEach(item, input_data) {
            if(!cJSON_IsString(item) || !(strncmp(item->valuestring, "Fronthaul", strlen("Fronthaul")) == 0 || strncmp(item->valuestring, "Backhaul", strlen("Backhaul")) == 0 ||
                strncmp(item->valuestring, "IoT", strlen("IoT")) == 0 || strncmp(item->valuestring, "Configurator", strlen("Configurator")) == 0 || strncmp(item->valuestring, "Hotspot", strlen("Hotspot")) == 0)) {
                em_printfout("%s:%d AUTOCONFIG_DEBUG HaulType must be string with one or many of these values: Fronthaul, Backhaul, IoT, Configurator, Hotspot\n", __func__, __LINE__);
                return bus_error_invalid_input;
            }
        }
    } else if(strncmp(input_name, "Type", strlen("Type")) == 0) {
        if(!cJSON_IsString(input_data)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG Type must be a string\n", __func__, __LINE__);
            return bus_error_invalid_input;
        }
    }
    em_printfout("%s:%d AUTOCONFIG_DEBUG validation successful for input_name:%s\n", __func__, __LINE__, input_name);

    return bus_error_success; 
}

#define decode_input_data(input_data, input_key, input_value) \
{ \
    input_value = cJSON_GetObjectItem(input_data, input_key); \
    if(input_value == NULL) { \ 
        em_printfout("%s:%d Input is NULL for key:%s\n", __func__, __LINE__, input_key); \
    } else { \
        input_value = cJSON_DetachItemViaPointer(input_data, input_value); \
        char *print_input_value = cJSON_Print(input_value); \
        em_printfout("%s:%d AUTOCONFIG_DEBUG print_input_value:%s \n", __func__, __LINE__, print_input_value); \
        free(print_input_value); \
        bus_error_t ret = validate_ssid_input_data(input_value, input_key); \
        if(ret != bus_error_success) { \
            cJSON_Delete(input_json); \
            return ret; \
        } \
    } \
} \

#define for_each_arg(input_json, item, ssid_args) \
    for(std::string str_ssid_args : ssid_args) { \
        if(cJSON_HasObjectItem(input_json, str_ssid_args.c_str())) { \
            em_printfout("%s:%d AUTOCONFIG_DEBUG input_json has Object:%s\n", __func__, __LINE__, str_ssid_args.c_str()); \
            cJSON *param_ssid_args = NULL; \
            decode_input_data(input_json, str_ssid_args.c_str(), param_ssid_args); \
            char *print_input_json = cJSON_Print(input_json); \
            em_printfout("%s:%d AUTOCONFIG_DEBUG print_input_json:%s \n", __func__, __LINE__, print_input_json); \
            free(print_input_json); \
            cJSON_ReplaceItemInObject(item, str_ssid_args.c_str(), param_ssid_args); \
        } \
    } \
 

bus_error_t em_ctrl_t::ctrl_cmd_ssid_set(char *event_name, raw_data_t *p_data, bus_user_data_t *user_data) {
    (void)user_data;
    em_subdoc_info_t *subdoc = NULL;
    unsigned char buff[EM_IO_BUFF_SZ];
    cJSON *json = NULL, *input_json = NULL, *input_json_item = NULL, *item = NULL, *root = NULL, *child = NULL, *next = NULL, *new_json = NULL, *json_obj = NULL, *get_haul_type = NULL, \
    *input_haultype = NULL, *haul_type_arr = NULL, *ssid_list = NULL, *haul_type_item = NULL, *input_ssid = NULL, *input_addremovechange = NULL, *input_json_args = NULL;
    /**target = NULL, *input_ssid = NULL, *input_addremovechange = NULL, *input_enable = NULL,\
    *input_passphrase = NULL, *input_band = NULL, *input_akms = NULL, *input_suite_selector = NULL, *input_mfp_config = NULL, *input_mobility_domain = NULL, \
    *input_advertisement_enabled = NULL, *input_type = NULL, *get_haul_type = NULL;*/
    std::vector<std::string> set_ssid_args = {"PassPhrase", "Enable", "Band", "AKMsAllowed", "SuiteSelector", "AdvertisementEnabled", "MFPConfig", "MobilityDomain", "Type"};
    em_network_node_t *updated_ssid_network_tree = NULL;
    em_cmd_params_t *ssid_cmd_params = g_ctrl.m_ctrl_cmd->get_param();
    char *jsonbuff = NULL, *updated_json = NULL, *haul_type = NULL;
    unsigned int json_len = 0, count_haultype = 0,ret = 0;
    bool found = false;

    em_printfout("%s:%d AUTOCONFIG_DEBUG event_name:%s data_type:%d data_len:%d input:%s \n", __func__, __LINE__,
        event_name, p_data->data_type, p_data->raw_data_len, (char *) p_data->raw_data.bytes);

    if(!p_data || p_data->raw_data_len < 0 || p_data->raw_data_len >= EM_IO_BUFF_SZ) {
        em_printfout("ERROR: Incorrect Input parameters in cmd_ssid_set\n");
        return bus_error_invalid_input;
    }

    input_json = cJSON_Parse((char *)p_data->raw_data.bytes);
    if(input_json == NULL) {
        em_printfout("ERROR: Failed to parse JSON from input_data; Provide input according to SetSSID Method\n");
        return bus_error_invalid_input;
    }

    char *print_input_json = cJSON_Print(input_json);
    em_printfout("%s:%d AUTOCONFIG_DEBUG input_json:%d print_input_json:%s has_object:%d \n", __func__, __LINE__, input_json->type, print_input_json, cJSON_HasObjectItem(input_json, "parameters"));
    free(print_input_json);
    if(input_json->child != NULL) {
        input_json_args = input_json->child;
    } else {
        em_printfout("ERROR: Incorrect JSON Format\n");
        cJSON_Delete(input_json);
        return bus_error_invalid_input;
    }
    char *print_json_child = cJSON_Print(input_json_args);
    em_printfout("%s:%d AUTOCONFIG_DEBUG print_json_child:%s \n", __func__, __LINE__, print_json_child);
    free(print_json_child);

    decode_input_data(input_json_args, "SSID", input_ssid);
    decode_input_data(input_json_args, "AddRemoveChange", input_addremovechange);
    decode_input_data(input_json_args, "HaulType", input_haultype);
    if(input_ssid == NULL || input_addremovechange == NULL) {
        em_printfout("ERROR: SSID or AddRemoveChange not found in input_data\n");
        cJSON_Delete(input_json);
        return bus_error_invalid_input;
    }
    em_printfout("%s:%d AUTOCONFIG_DEBUG ssid:%s AddRemoveChange:%s size_of_input_haultype:%d \n", __func__, __LINE__, input_ssid->valuestring,
        input_addremovechange->valuestring, cJSON_GetArraySize(input_haultype));

    /*if(strncmp(input_addremovechange->valuestring, "Add", strlen("Add")) == 0 || strncmp(input_addremovechange->valuestring, "Remove", strlen("Remove")) == 0) {
        em_printfout("%s:%d AUTOCONFIG_DEBUG Adding or Removing SSID is not supported\n", __func__, __LINE__);
        return bus_error_invalid_input;
    }*/

    subdoc = (em_subdoc_info_t *)buff;
    strncpy(subdoc->name, "NetworkSSIDList", strlen("NetworkSSIDList"));
    g_ctrl.m_data_model.get_config("OneWifiMesh", subdoc);
    if(subdoc->buff == NULL) {
        em_printfout("%s:%d ERROR: subdoc->buff is NULL\n", __func__, __LINE__);
        cJSON_Delete(input_json);
        return bus_error_invalid_input;
    }
    em_printfout("%s:%d AUTOCONFIG_DEBUG name:%s \n", __func__, __LINE__, subdoc->name);
    em_printfout("%s:%d AUTOCONFIG_DEBUG buff:%s \n", __func__, __LINE__, subdoc->buff);

    json = cJSON_Parse(subdoc->buff);
    if(json == NULL) {
        em_printfout("ERROR: Failed to parse JSON from subdoc\n");
        cJSON_Delete(input_json);
        return bus_error_invalid_input;
    }

    root = cJSON_CreateObject();
    em_printfout("%s:%d AUTOCONFIG_DEBUG root: %p \n", __func__, __LINE__, root);
    new_json = cJSON_CreateObject();
    cJSON_AddStringToObject(new_json, "ID", "OneWifiMesh");

    child = json->child;
    while (child) {
        char *print_json_child = cJSON_Print(child);
        em_printfout("%s:%d AUTOCONFIG_DEBUG print_json_child:%s \n", __func__, __LINE__, print_json_child);
        next = child->next;
        cJSON_DetachItemViaPointer(json, child);
        cJSON_AddItemToObject(new_json, child->string, child);
        child = next;
        free(print_json_child);
    }
    cJSON_Delete(json);
    json = new_json;

    cJSON_AddItemToObject(root, "wfa-dataelements:SetSSID", json);
    jsonbuff = cJSON_Print(root);
    em_printfout("%s:%d root: %s\n", __func__, __LINE__, jsonbuff);
    free(jsonbuff);

    ssid_list = cJSON_GetObjectItem(json, "NetworkSSIDList");
    if(ssid_list == NULL || !cJSON_IsArray(ssid_list)) {
        em_printfout("ERROR: NetworkSSIDList not found or is not an array\n");
        cJSON_Delete(json);
        cJSON_Delete(input_json);
        return bus_error_invalid_input;
    }

    cJSON_ArrayForEach(item, ssid_list) {
        haul_type_arr = cJSON_GetObjectItem(item, "HaulType");
        cJSON_ArrayForEach(haul_type_item, haul_type_arr) {
            haul_type = cJSON_GetStringValue(haul_type_item);
            em_printfout("%s:%d AUTOCONFIG_DEBUG haul_type:%s\n", __func__, __LINE__, haul_type);
        }

        if(!cJSON_GetArraySize(input_haultype) && strncmp(haul_type, "Fronthaul", strlen("Fronthaul")) == 0) {
            cJSON_ReplaceItemInObject(item, "SSID", input_ssid);
            for_each_arg(input_json_args, item, set_ssid_args);
            break;
        }

        cJSON_ArrayForEach(get_haul_type, input_haultype) {
            char *input_haul_type = cJSON_GetStringValue(get_haul_type);
            em_printfout("%s:%d AUTOCONFIG_DEBUG input_haul_type:%s count_haultype:%d \n", __func__, __LINE__, input_haul_type, count_haultype);
            if( strncmp(haul_type, input_haul_type, strlen(input_haul_type)) == 0 ) {
                cJSON_ReplaceItemInObject(item, "SSID", input_ssid);
                for_each_arg(input_json_args, item, set_ssid_args);
                count_haultype++;
            }
        }
        if(count_haultype == cJSON_GetArraySize(input_haultype)) {
            em_printfout("%s:%d AUTOCONFIG_DEBUG count_haultype:%d is equal to ArraySize:%d\n", __func__, __LINE__, count_haultype, cJSON_GetArraySize(input_haultype));
            break;
        }
    }

    updated_json = cJSON_PrintUnformatted(root);
    json_len = strlen(updated_json);
    if (json_len >= EM_IO_BUFF_SZ) {
        em_printfout("ERROR: JSON too large for buffer!");
        free(updated_json);
        cJSON_Delete(json);
        cJSON_Delete(input_json);
        return bus_error_invalid_input;
    }

    memcpy(subdoc->buff, updated_json, json_len);
    subdoc->buff[json_len] = '\0';
    json_obj = cJSON_Parse(subdoc->buff);
    if (json_obj) {
        char *new_json = cJSON_Print(json_obj);
        em_printfout("Updated and formatted JSON:\n%s", new_json);
        free(new_json);
        cJSON_Delete(json_obj);
    } else {
        em_printfout("Invalid JSON in subdoc->buff");
    }

    updated_ssid_network_tree = get_network_tree(subdoc->buff);
    if(updated_ssid_network_tree == NULL) {
        em_printfout("ERROR: Failed to get network tree\n");
        return bus_error_invalid_input;
    }
    ssid_cmd_params->net_node = updated_ssid_network_tree;

    em_printfout("%s:%d AUTOCONFIG_DEBUG calling io_process \n", __func__, __LINE__);
    g_ctrl.io_process(em_bus_event_type_set_ssid, subdoc->buff, strlen(subdoc->buff), ssid_cmd_params);
    free(updated_json);
    cJSON_Delete(json);
    em_printfout("%s:%d AUTOCONFIG_DEBUG Delete input_json \n", __func__, __LINE__);
    cJSON_Delete(input_json);
    em_printfout("%s:%d AUTOCONFIG_DEBUG calling return bus_error_success \n", __func__, __LINE__);

    return bus_error_success;
}

bus_error_t em_ctrl_t::ctrl_cmd_ssid_set_inner(char const* methodName, raw_data_t *inParams, raw_data_t *outParams, void *asyncHandle) {
    em_printfout("%s:%d AUTOCONFIG_DEBUG methodName:%s \n", __func__, __LINE__, methodName);
    outParams = (raw_data_t *) malloc(sizeof(raw_data_t));
    if(outParams == NULL) {
        em_printfout("%s:%d AUTOCONFIG_DEBUG outParams is NULL\n", __func__, __LINE__);
        return bus_error_out_of_resources;
    }
    std::string status = "Status: Success";
    outParams->raw_data_len = 16;
    outParams->raw_data.bytes = (void *)status.c_str();
    outParams->data_type = bus_data_type_string;

    if(inParams->raw_data_len > 0) em_printfout("%s:%d AUTOCONFIG_DEBUG input_params:%s \n", __func__, __LINE__, (char *)inParams->raw_data.bytes);
    return bus_error_success;
}

bus_error_t em_ctrl_t::ctrl_cmd_ssid_set_outer(char *event_name, raw_data_t *p_data, bus_user_data_t *user_data) {
    em_printfout("%s:%d AUTOCONFIG_DEBUG event_name:%s \n", __func__, __LINE__, event_name);
    return bus_set_cb_fwd(event_name, p_data, user_data, ctrl_cmd_ssid_set);
}

bus_error_t em_ctrl_t::ctrl_cmd_ssid_set_method(char const* methodName, raw_data_t *inParams, raw_data_t *outParams, void *asyncHandle) {
    em_printfout("%s:%d AUTOCONFIG_DEBUG methodName:%s \n", __func__, __LINE__, methodName);
    return bus_method_cb_fwd(methodName, inParams, outParams, asyncHandle, ctrl_cmd_ssid_set_inner);
}
