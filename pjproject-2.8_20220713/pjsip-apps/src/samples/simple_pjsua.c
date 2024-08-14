/* $Id: simple_pjsua.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

/**
 * simple_pjsua.c
 *
 * This is a very simple but fully featured SIP user agent, with the 
 * following capabilities:
 *  - SIP registration
 *  - Making and receiving call
 *  - Audio/media to sound device.
 *
 * Usage:
 *  - To make outgoing call, start simple_pjsua with the URL of remote
 *    destination to contact.
 *    E.g.:
 *	 simpleua sip:user@remote
 *
 *  - Incoming calls will automatically be answered with 200.
 *
 * This program will quit once it has completed a single call.
 */

#include <pjsua-lib/pjsua.h>
#include<time.h>

#define THIS_FILE	"APP"

#define SIP_DOMAIN	"example.com"
#define SIP_USER	"alice"
#define SIP_PASSWD	"secret"

int g_player_id = PJSUA_INVALID_ID;
pjsua_call_id g_call_id = PJSUA_INVALID_ID;
pjsua_call_id g_call_id2 = PJSUA_INVALID_ID;
void PlayerStop()
{
    if (g_player_id != PJSUA_INVALID_ID) {
            pjsua_conf_disconnect(pjsua_player_get_conf_port(g_player_id), 0);
            if (pjsua_player_destroy(g_player_id) == PJ_SUCCESS) {
                g_player_id = PJSUA_INVALID_ID;
            }
    }
}
struct pjsua_player_eof_data
{
    pj_pool_t          *pool;
    pjsua_player_id player_id;
};
void PlayerPlay(int noLoop)
{
    FILE *fp = fopen("ring.wav", "r");
    if (fp == NULL)
    {
        return;
    }
    else
    {
        fclose(fp);
    }
    PlayerStop();

    pj_str_t file = pj_str("ring.wav");
    if (pjsua_player_create(&file, noLoop ? PJMEDIA_FILE_NO_LOOP : 0, &g_player_id) == PJ_SUCCESS) {
        pjmedia_port *player_media_port;
        if (pjsua_player_get_port(g_player_id, &player_media_port) == PJ_SUCCESS) {
            if (noLoop) {
                struct pj_pool_t *pool = pjsua_pool_create("microsip_eof_data", 512, 512);
                //pjmedia_wav_player_set_eof_cb(player_media_port, NULL, NULL);

                struct pjsua_player_eof_data *eof_data = PJ_POOL_ZALLOC_T(pool, struct pjsua_player_eof_data);
                eof_data->pool = pool;
                eof_data->player_id = g_player_id;
                pjmedia_wav_player_set_eof_cb(player_media_port, eof_data, NULL);
            }
            if ( pjsua_conf_get_active_ports() <= 3 ) {
                        int in, out;
                        if (pjsua_get_snd_dev(&in, &out) != PJ_SUCCESS) {
                            pjsua_set_snd_dev(in, out);
                        }
            }
            pjsua_conf_connect(pjsua_player_get_conf_port(g_player_id), 0);
        }
    }
}

/* Callback called by the library upon receiving incoming call */
static void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id,
			     pjsip_rx_data *rdata)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(acc_id);
    PJ_UNUSED_ARG(rdata);

    pjsua_call_get_info(call_id, &ci);

    PJ_LOG(3,(THIS_FILE, "Incoming call from %.*s!!",
			 (int)ci.remote_info.slen,
			 ci.remote_info.ptr));

    /* Automatically answer incoming calls with 200/OK */
    g_call_id2 = PJSUA_INVALID_ID;
    g_call_id = call_id;
    printf("on_incoming_call 4444:%d:%d:%d\r\n", g_call_id, g_call_id2, call_id);

    pjsua_call_answer(call_id, 180, NULL, NULL);

    PlayerPlay(0);

    int valHW = 400;
    pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_INPUT_VOLUME_SETTING, &valHW, PJ_TRUE);
    pjsua_conf_adjust_rx_level(0, (float)valHW / 100);
    pjsua_snd_set_setting(PJMEDIA_AUD_DEV_CAP_OUTPUT_VOLUME_SETTING, &valHW, PJ_TRUE);
    pjsua_conf_adjust_rx_level(1, (float)valHW / 100);

    //pjsua_call_answer(call_id, 200, NULL, NULL);
}

/* Callback called by the library when call's state has changed */
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &ci);
    PJ_LOG(3,(THIS_FILE, "Call %d state=%.*s", call_id,
			 (int)ci.state_text.slen,
			 ci.state_text.ptr));

    if (ci.state == PJSIP_INV_STATE_DISCONNECTED)
    {
        PlayerStop();
        remove("fsc_i.lock");
    }
}

/* Callback called by the library when call's media state has changed */
static void on_call_media_state(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);

    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	// When media is active, connect call to sound device.
	    pjsua_conf_connect(ci.conf_slot, 0);
	    pjsua_conf_connect(0, ci.conf_slot);
    }
}

/* Display error and exit application */
static void error_exit(const char *title, pj_status_t status)
{
    remove("fsc_a.lock");
    remove("fsc_h.lock");
    remove("fsc_l.lock");
    remove("fsc_i.lock");
    remove("fsc_q.lock");
    remove("fsc_u.lock");
    pjsua_perror(THIS_FILE, title, status);
    pjsua_destroy();
    exit(1);
}

static void on_reg_state2(pjsua_acc_id acc_id, pjsua_reg_info *info)
{
    time_t t;
    t = time(NULL);
    struct tm *l = localtime(&t);
    char c[32];
    sprintf(c, "%d-%02d-%02d %02d:%02d:%02d", l->tm_year + 1900, l->tm_mon + 1, l->tm_mday, l->tm_hour, l->tm_min, l->tm_sec);
    printf("%s on_reg_state2:%d %d %d!!\r\n", c, info->cbparam->status, info->renew, info->cbparam->code);

    if (info != NULL && info->cbparam != NULL && info->cbparam->status == PJ_SUCCESS && info->cbparam->code == 200)
    {
        remove("fsc_u.lock");
    }
    else
    {
        FILE *fp = fopen("fsc_u.lock", "wt");
        if (fp != NULL)
        {
            fclose(fp);
        }
    }
}

/*
 * main()
 *
 * argv[1] may contain URL to call.
 */
int main(int argc, char *argv[])
{
    if (argc < 3 || argc > 4)
    {
        return 0;
    }

    pjsua_acc_id acc_id;
    pj_status_t status;

    /* Create pjsua first! */
    status = pjsua_create();
    if (status != PJ_SUCCESS) error_exit("Error in pjsua_create()", status);

    /* If argument is specified, it's got to be a valid SIP URL */
//    if (argc > 1) {
//	status = pjsua_verify_url(argv[1]);
//	if (status != PJ_SUCCESS) error_exit("Invalid URL in argv", status);
//    }

    /* Init pjsua */
    {
	pjsua_config cfg;
	pjsua_logging_config log_cfg;

	pjsua_config_default(&cfg);
	cfg.cb.on_incoming_call = &on_incoming_call;
	cfg.cb.on_call_media_state = &on_call_media_state;
	cfg.cb.on_call_state = &on_call_state;
    cfg.cb.on_reg_state2 = &on_reg_state2;

	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = 4;//4

	status = pjsua_init(&cfg, &log_cfg, NULL);
	if (status != PJ_SUCCESS) error_exit("Error in pjsua_init()", status);
    }

    /* Add UDP transport. */
    {
	pjsua_transport_config cfg;

	pjsua_transport_config_default(&cfg);
	cfg.port = 5060;
	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, NULL);
	if (status != PJ_SUCCESS) error_exit("Error creating transport", status);
    }

    /* Initialization is done, now start pjsua */
    status = pjsua_start();
    if (status != PJ_SUCCESS) error_exit("Error starting pjsua", status);

    /* Register to SIP server by creating SIP account. */

    char buf[64];
    char buf2[64];
    char buf3[32];
    char bufD[32];
    remove("fsc_a.lock");
    remove("fsc_h.lock");
    remove("fsc_l.lock");
    remove("fsc_i.lock");
    remove("fsc_q.lock");
    remove("fsc_u.lock");
    {
	pjsua_acc_config cfg;

	pjsua_acc_config_default(&cfg);
    //cfg.id = pj_str("sip:" SIP_USER "@" SIP_DOMAIN);
    //cfg.id = pj_str("sip:1004@10.23.8.154");//* 9142@10.23.15.232
    sprintf(buf, "sip:%s@%s", argv[1], argv[2]);
    cfg.id = pj_str(buf);
    //cfg.reg_uri = pj_str("sip:" SIP_DOMAIN);
    //cfg.reg_uri = pj_str("sip:10.23.8.154");//*
    sprintf(buf2, "sip:%s", argv[2]);
    cfg.reg_uri = pj_str(buf2);//*
	cfg.cred_count = 1;

	//cfg.cred_info[0].realm = pj_str(SIP_DOMAIN);
    //cfg.cred_info[0].realm = pj_str("10.23.8.154");//*
    cfg.cred_info[0].realm = pj_str(argv[2]);//*

	//cfg.cred_info[0].scheme = pj_str("digest");
    //cfg.cred_info[0].scheme = pj_str("1004");//*
    cfg.cred_info[0].scheme = pj_str(argv[1]);

    //cfg.cred_info[0].username = pj_str(SIP_USER);
    //cfg.cred_info[0].username = pj_str("1004");//*
    cfg.cred_info[0].username = pj_str(argv[1]);

	cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	//cfg.cred_info[0].data = pj_str(SIP_PASSWD);
    if (argc > 3) 
    {
        sprintf(buf3, "%s", argv[3]);
    }
    else
    {
        sprintf(buf3, "%s", "1234");
    }
    cfg.cred_info[0].data = pj_str(buf3);//*

    //reg_timeout ???
    //cfg.reg_timeout = 30;
    //cfg.reg_retry_interval = 10;
    //cfg.reg_first_retry_interval = 0;
    //cfg.reg_retry_random_interval = 0;
    //cfg.reg_delay_before_refresh = 10;

	status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
	if (status != PJ_SUCCESS) error_exit("Error adding account", status);
    }

    /* If URL is specified, make call to the URL. */
    //if (argc > 1) {
	//pj_str_t uri = pj_str(argv[1]);
        //pj_str_t uri = pj_str("sip:1001@10.23.15.232");//*
	    //status = pjsua_call_make_call(acc_id, &uri, 0, NULL, NULL, NULL);
	    //if (status != PJ_SUCCESS) error_exit("Error making call", status);
    //}

    for (int i = 0; i < 10; i++)
    {
        FILE *fp = fopen("fsc_l.lock", "wt");
        if (fp != NULL)
        {
            fclose(fp);
            break;
        }
        pj_thread_sleep(10);
    }
    /* Wait until user press "q" to quit. */
    for (;;) {
	    char option[10];
        option[0] = 0;

        if (argc == 3)
        {
	        puts("Press 'h' to hangup all calls, , 'a' to answer, 'q' to quit");
	        if (fgets(option, sizeof(option), stdin) == NULL) {
	            puts("EOF while reading stdin, will quit now..");
	            break;
	        }

            if (option[0] == 'q')
                break;

            if (option[0] == 'h')
            {
                pjsua_call_hangup_all();
                if (g_call_id != PJSUA_INVALID_ID)
                {
                    pjsua_call_hangup(g_call_id, 0, NULL, NULL);
                    pjsua_call_answer(g_call_id, 487, NULL, NULL);
                    PlayerStop();
                }
            }

            if (option[0] == 'a')
            {
                PlayerStop();
                pjsua_call_answer(g_call_id, 200, NULL, NULL);
            }

            if (option[0] == 'd')
            {
                char d[8];
                d[0] = option[1];
                d[1] = option[2];
                d[2] = option[3];
                d[3] = option[4];
                d[4] = 0;
                sprintf(bufD, "sip:%s@%s", d, argv[2]);
                pj_str_t uriTmp = pj_str(bufD);
                status = pjsua_call_make_call(acc_id, &uriTmp, 0, NULL, NULL, NULL);
                if (status != PJ_SUCCESS)
                {
                    pjsua_call_hangup_all();
                }
            }

            continue;
        }

        FILE *fp = fopen("fsc_q.lock", "r");
        if (fp != NULL)
        {
            fclose(fp);
            for (int i = 0; i < 3; i++)
            {
                remove("fsc_q.lock");
            }
            break;
        }
        fp = fopen("fsc_h.lock", "r");
        if (fp != NULL)
        {
            fclose(fp);
            pjsua_call_hangup_all();
            if (g_call_id != PJSUA_INVALID_ID)
            {
                pjsua_call_hangup(g_call_id, 0, NULL, NULL);
                pjsua_call_answer(g_call_id, 487, NULL, NULL);
                PlayerStop();
            }
            for (int i = 0; i < 3; i++)
            {
                remove("fsc_h.lock");
            }
        }
        printf("call_id 2222:%d:%d\r\n", g_call_id, g_call_id2);
        if (g_call_id == PJSUA_INVALID_ID)
        {
            printf("call_id 4444:%d:%d\r\n", g_call_id, g_call_id2);
            FILE *fp = fopen("fsc_a.lock", "r");
            if (fp != NULL)
            {
                fclose(fp);
                printf("call_id 5555:%d:%d\r\n", g_call_id, g_call_id2);
                if (g_call_id2 != PJSUA_INVALID_ID)
                {
                    PlayerStop();
                    pjsua_call_answer(g_call_id2, 200, NULL, NULL);
                    g_call_id2 = PJSUA_INVALID_ID;
                    for (int i = 0; i < 3; i++)
                    {
                        remove("fsc_a.lock");
                        remove("fsc_i.lock");
                    }
                    printf("call_id 6666:%d:%d\r\n", g_call_id, g_call_id2);
                }
            }
            else
            {
                FILE *fp = fopen("fsc_d.lock", "r");
                if (fp != NULL)
                {
                    fseek(fp, 0, SEEK_END);
                    int iSize = ftell(fp);
                    char bd[16];
                    memset(bd, 0, 16);
                    fseek(fp, 0, SEEK_SET);
                    fread(bd, 1, iSize, fp);
                    fclose(fp);
                    sprintf(bufD, "sip:%s@%s", bd, argv[2]);
                    pj_str_t uriTmp = pj_str(bufD);
                    status = pjsua_call_make_call(acc_id, &uriTmp, 0, NULL, NULL, NULL);
                    if (status != PJ_SUCCESS)
                    {
                        pjsua_call_hangup_all();
                    }
                    for (int i = 0; i < 3; i++)
                    {
                        remove("fsc_d.lock");
                    }
                }
            }
        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                FILE *fp = fopen("fsc_i.lock", "wt");
                if (fp != NULL)
                {
                    fclose(fp);
                    g_call_id2 = g_call_id;
                    g_call_id = PJSUA_INVALID_ID;
                    printf("call_id 8888:%d:%d\r\n", g_call_id, g_call_id2);
                    break;
                }
                pj_thread_sleep(10);
            }
        }

        pj_thread_sleep(1000);
    }

    remove("fsc_a.lock");
    remove("fsc_h.lock");
    remove("fsc_l.lock");
    remove("fsc_i.lock");
    remove("fsc_q.lock");
    /*stroy pjsua */
    pjsua_destroy();

    return 0;
}
