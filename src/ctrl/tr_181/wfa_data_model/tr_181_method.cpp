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
#include "em_ctrl.h"
#include "tr_181.h"
#include <assert.h>

bus_error_t tr_181_t::setssid_handler(const char *method_name, raw_data_t *input_data, raw_data_t *output_data, void *async_handle)
{
    // Suppress unused parameter warning if async_handle is not used
    (void)async_handle;

    // Standardize input pointer checks
    if (!input_data || input_data->raw_data_len == 0) {
        em_printfout("Invalid input_data or missing input_props");
        if (output_data) {
            tr_181_t::tr181_set_status_output(output_data, "Failure: missing input_props");
        }
        return bus_error_invalid_input;
    }

    bus_data_prop_t *input_props = static_cast<bus_data_prop_t *>(input_data->raw_data.bytes);
    bus_data_prop_t *output_props = NULL;

    em_printfout("Method='%s' input_data:%s input_len=%u", method_name ? method_name : "(null)", (char *)input_data->raw_data.bytes, input_data->raw_data_len);
    // Log all chained input properties
    for (bus_data_prop_t *p = input_props; p; p = p->next_data) {
        em_printfout("Prop='%s' type=%d len=%u", p->name, p->value.data_type, p->value.raw_data_len);
    }

    em_ctrl_t *ctrl = em_ctrl_t::get_em_ctrl_instance();
    if (!ctrl) {
        em_printfout("Controller unavailable");
        if (output_data) {
            tr_181_t::tr181_set_status_output(output_data, "Failure: controller unavailable");
        }
        return bus_error_invalid_input;
    }

    const char *event = method_name ? method_name : DEVICE_WIFI_DATAELEMENTS_NETWORK_SETSSID_CMD;
    bus_error_t rc = ctrl->cmd_setssid(event, input_props, output_data ? &output_props : NULL, async_handle);

    if (output_data && output_props) {
        output_data->data_type = bus_data_type_property;
        output_data->raw_data.bytes = output_props;
        output_data->raw_data_len = sizeof(bus_data_prop_t);
    }

    return rc;
}

bus_error_t tr_181_t::bus_method_cb_fwd(const char *method_name, raw_data_t *input_data, raw_data_t *output_data, void *async_handle, bus_method_handler_t cb)
{
    uint32_t s_id;
    em_event_t *req;
    bus_error_t rc;
    bus_data_prop_t *input_props = static_cast<bus_data_prop_t *>(input_data->raw_data.bytes);
    bus_data_prop_t *output_props = NULL;
    bus_resp_get_t *resp = NULL;
    uintptr_t buf;
    em_ctrl_t *ctrl = em_ctrl_t::get_em_ctrl_instance();
    dm_easy_mesh_ctrl_t *dm_ctrl = ctrl->get_dm_ctrl();

    if (!input_data || input_data->raw_data_len == 0) {
        em_printfout("Invalid input_data or missing input_props");
        if (output_data) {
            tr_181_t::tr181_set_status_output(output_data, "Failure: missing input_props");
        }
        return bus_error_invalid_input;
    }

    if(!ctrl || !dm_ctrl) {
        em_printfout("Controller unavailable");
        if (output_data) {
            tr_181_t::tr181_set_status_output(output_data, "Failure: controller unavailable");
        }
        return bus_error_invalid_input;
    }

    em_printfout("Method='%s' input_len=%u", method_name ? method_name : "(null)", input_data->raw_data_len);
    // Log all chained input properties
    for (bus_data_prop_t *p = input_props; p; p = p->next_data) {
        em_printfout("Prop='%s' type=%d len=%u", p->name, p->value.data_type, p->value.raw_data_len);
    }

    do {
        req = (em_event_t *) malloc(sizeof(em_event_t));
        if(!req) {
            rc = bus_error_out_of_resources;
            break;
        }

        s_id = dm_ctrl->get_next_nb_evt_id();
        req->type = em_event_type_nb;
        req->u.nevt.id = s_id;
        req->u.nevt.type = NB_REQTYPE_METHOD;
        req->u.nevt.u.method.method = method_name;
        req->u.nevt.u.method.in = input_props;
        req->u.nevt.u.method.out = output_data ? &output_props : NULL;
        req->u.nevt.u.method.async = async_handle;
        req->u.nevt.cb = (void *) cb;

        ctrl->push_to_queue(req);

        ssize_t len = read(dm_ctrl->get_nb_pipe_rd(), &buf, sizeof(buf));
        assert(len == sizeof(buf));
        resp = (bus_resp_get_t *) buf;
        assert(resp->id == s_id);
        rc = resp->rc;

        if (output_data && output_props) {
            output_data->data_type = bus_data_type_property;
            output_data->raw_data.bytes = output_props;
            output_data->raw_data_len = sizeof(bus_data_prop_t);
        }
    } while(0);

    return rc;
}

bus_error_t tr_181_t::ctrl_cmd_client_steer(const char *method_name, raw_data_t *input_data, raw_data_t *output_data, void *async_handle) {
    em_ctrl_t *ctrl = em_ctrl_t::get_em_ctrl_instance();

    if(ctrl != NULL && ctrl->get_dm_ctrl() != NULL) {
        return ctrl->get_dm_ctrl()->bus_method_cb_fwd(method_name, input_data, output_data, async_handle, ctrl->get_dm_ctrl()->ctrl_cmd_client_steer_inner);
    }

    return bus_error_general;
}
