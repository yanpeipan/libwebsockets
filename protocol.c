#if !defined (LWS_PLUGIN_STATIC)
#define LWS_DLL
#define LWS_INTERNAL
#include <libwebsockets.h>
#endif

#include <string.h>
#include <stdio.h> 
#include <json-c/json.h>

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
    char *body;
} Message;

struct loginMsg {
    int cmd:1;
    const char *vin;
};

const char* get_vehicle_login_message(const char *vin)
{
    struct json_object *obj = json_object_new_array();

    json_object_array_add(obj, json_object_new_int(CMD_VEHICLE_LOGIN));
    json_object_array_add(obj, json_object_new_string(vin));
    
    return json_object_to_json_string(obj);
}

Message* decode(const char *in) {
    struct json_object *obj;
    struct message *msg;

    obj = json_tokener_parse(in);
    msg->cmd = json_object_get_int(obj);

    switch (msg->cmd) {
        case  CMD_VEHICLE_LOGIN :
            break;
        case CMD_VEHICLE_INFO:
            break;
    }
    msg->code = json_object_get_int(obj);

    lwsl_user("cmd %d \n", msg->cmd);

    return msg;
}
