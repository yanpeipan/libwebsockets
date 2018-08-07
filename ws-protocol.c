/*
 * ws-client
 *
 * Copyright (C) 2018 yanpeipan <yanpeipan_82@qq.com>
 */

#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>
#include <stdio.h> 
#include <json-c/json.h>

typedef enum code {
    CODE_OK = 0
} CODE;

typedef enum command {
    CMD_PING = 0,
    CMD_PONG = 1,
    CMD_VEHICLE_LOGIN = 2,
    CMD_VEHICLE_INFO = 3,
    CMD_VEHICLE_LIST = 4,
    CMD_USER_LOGIN = 5,
    CMD_USER_INFO = 6,
    CMD_VEHICLE_HAILING = 7,
    CMD_VEHICLE_PARKING = 8,
    CMD_VEHICLE_HAILING_END = 9,
    CMD_VEHICLE_PARKING_END = 10,
    CMD_VEHICLE_ABOARD = 11,
    CMD_ADMIN_LOGIN = 20
} Command;

typedef struct message {
    int cmd;
    int code;
    void *body;
} *Message;

typedef struct info {
    double lat;
    double lng;
} *Info;

const char *get_hailing_end_msg(int err)
{
    struct json_object *obj = json_object_new_array();

    json_object_array_add(obj, json_object_new_int(CMD_VEHICLE_HAILING_END));
    json_object_array_add(obj, json_object_new_int(err));

    return json_object_to_json_string(obj);
}

const char *get_parking_end_msg(int err)
{
    struct json_object *obj = json_object_new_array();

    json_object_array_add(obj, json_object_new_int(CMD_VEHICLE_PARKING_END));
    json_object_array_add(obj, json_object_new_int(err));

    return json_object_to_json_string(obj);
}

const char *get_info_msg(double lat, double lng)
{
    struct json_object *obj = json_object_new_array();

    json_object_array_add(obj, json_object_new_int(CMD_VEHICLE_INFO));
    json_object_array_add(obj, json_object_new_double(lat));
    json_object_array_add(obj, json_object_new_double(lng));

    return json_object_to_json_string(obj);
}

const char *get_login_msg(const char *vin)
{
    struct json_object *obj = json_object_new_array();

    json_object_array_add(obj, json_object_new_int(CMD_VEHICLE_LOGIN));
    json_object_array_add(obj, json_object_new_string(vin));
    
    return json_object_to_json_string(obj);
}

Message parse_msg(char *in) {
    struct json_object *obj;
    Message msg = (struct message *)malloc(sizeof(struct message));

    obj = json_tokener_parse(in);
    if (obj == NULL) {
        lwsl_err("parse err: %s", in);
    }
    msg->cmd = json_object_get_int(json_object_array_get_idx(obj, 0));
    msg->code = json_object_get_int(json_object_array_get_idx(obj, 1));

    switch (msg->cmd) {
        case CMD_VEHICLE_LOGIN:
            break;
        case CMD_VEHICLE_INFO:
            break;
        case CMD_VEHICLE_HAILING:
            msg->code = 0;
            struct info i;

            i.lat = json_object_get_double(json_object_array_get_idx(obj, 2));
            i.lng = json_object_get_double(json_object_array_get_idx(obj, 3));
            msg->body = &i;
            break;
        case CMD_VEHICLE_PARKING:
            msg->code = 0;
            break;
        case CMD_VEHICLE_ABOARD:
            msg->code = 0;
            break;
        default :
            break;
    }

    json_object_put(obj);

    lwsl_user("in: %s, command: %d, code: %d\n", in, msg->cmd, msg->code);

    return msg;
}
