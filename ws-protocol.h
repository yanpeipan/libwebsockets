/*
 * ws-client
 *
 * Copyright (C) 2018 yanpeipan <yanpeipan_82@qq.com>
 */
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

const char *get_hailing_end_msg(int err);

const char *get_parking_end_msg(int err);

const char *get_info_msg(double lat, double lng);

const char *get_login_msg(const char *vin);

Message parse_msg(char *in);
