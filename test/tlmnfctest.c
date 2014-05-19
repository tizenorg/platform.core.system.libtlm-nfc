/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gsignond
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * Contact: Alexander Kanavin <alex.kanavin@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <check.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <glib-unix.h>
#include "gtlm-nfc.h"


static void _read_test_tag_found_callback(GTlmNfc* tlm_nfc,
                               const gchar* tag_path,
                               gpointer user_data)
{
    int* counter = (int*)user_data;
    (*counter)++;
    
    g_print("Tag %s found, count %d\n", tag_path,
            *counter);
}

static void _read_test_tag_lost_callback(GTlmNfc* tlm_nfc,
                               const gchar* tag_path,
                               gpointer user_data)
{
    int* counter = (int*)user_data;
    (*counter)++;
    g_print("Tag %s lost, count %d\n", tag_path,
            *counter);
}

static void _read_test_record_found_callback(GTlmNfc* tlm_nfc,
                                             const gchar* username,
                                             const gchar* password,
                                             gpointer user_data)
{
    g_print("Found username %s, password %s on tag\n", username, password);
}

static void _read_test_no_record_found_callback(GTlmNfc* tlm_nfc,
                                             gpointer user_data)
{
    g_print("Did not find username and password on tag\n");
}

START_TEST (test_tlm_nfc_read)
{
    int tag_found_counter = 0;
    int tag_lost_counter = 0;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    GTlmNfc* tlm_nfc = g_object_new(G_TYPE_TLM_NFC, NULL);
    G_IS_TLM_NFC(tlm_nfc);
    g_print("Please place the tag on the reader, and then remove it, twice\n");
    g_signal_connect(tlm_nfc, "tag-found", G_CALLBACK(_read_test_tag_found_callback), &tag_found_counter);
    g_signal_connect(tlm_nfc, "tag-lost", G_CALLBACK(_read_test_tag_lost_callback), &tag_lost_counter);
    g_signal_connect(tlm_nfc, "record-found", G_CALLBACK(_read_test_record_found_callback), NULL);
    g_signal_connect(tlm_nfc, "no-record-found", G_CALLBACK(_read_test_no_record_found_callback), NULL);

    while (1) {
        g_main_context_iteration(g_main_context_default(), TRUE);
        if(tag_found_counter == 2 && tag_lost_counter >= 2)
            break;
    }
    
    g_object_unref(tlm_nfc);
    g_main_loop_unref(loop);
}
END_TEST


static void write_plaintext_callback(GTlmNfc* tlm_nfc,
                               const gchar* tag_path,
                               gpointer user_data)
{
    GError* error = NULL;
    const gchar* username = (const gchar*)user_data;
    
    g_print("Tag %s found\n", tag_path);

    g_print("Waiting 5 seconds due to https://01.org/jira/browse/NFC-57\n");
    sleep(5);

    g_print("Writing username %s to tag\n", username);

    gtlm_nfc_write_username_password(tlm_nfc,
                                     tag_path,
                                     username,
                                     "somesecret",
                                     &error);

    if (!error)
        g_print("success!\n");
    else
        g_print("error: %s\n", error->message);

}

static void _write_test_record_found_callback_check_username(GTlmNfc* tlm_nfc,
                                             const gchar* username,
                                             const gchar* password,
                                             gpointer user_data)
{
    g_print("Found username %s, password %s on tag\n", username, password);

    gchar* original_username = (gchar*)user_data;

    fail_if(g_strcmp0(username, original_username) != 0);
}

static void _write_test_record_found_callback_increase_counter(GTlmNfc* tlm_nfc,
                                             const gchar* username,
                                             const gchar* password,
                                             gpointer user_data)
{
    int* counter = (int*)user_data;
    (*counter)++;
}


static void _write_test_no_record_found_callback(GTlmNfc* tlm_nfc,
                                             gpointer user_data)
{
    g_print("Did not find username and password on tag\n");
    fail_if(TRUE);
}


START_TEST (test_tlm_nfc_write)
{
    int tag_lost_counter = 0;
    int record_found_counter = 0;
    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    GTlmNfc* tlm_nfc = g_object_new(G_TYPE_TLM_NFC, NULL);
    G_IS_TLM_NFC(tlm_nfc);
    g_print("WARNING: the following test will perform a destructive write on the tag.\n");
    g_print("Press Ctrl-C if you do not wish to continue.\n");
    g_print("Please place the tag on the reader, and then remove it\n");

    gchar* username = g_strdup_printf("user%d", g_random_int());

    g_signal_connect(tlm_nfc, "tag-found", G_CALLBACK(write_plaintext_callback), username);
    g_signal_connect(tlm_nfc, "tag-lost", G_CALLBACK(_read_test_tag_lost_callback), &tag_lost_counter);

    while (1) {
        g_main_context_iteration(g_main_context_default(), TRUE);
        if(tag_lost_counter == 1)
            break;
    }

    g_print("Please place the tag on the reader, and then remove it\n");
    g_signal_handlers_disconnect_by_func(tlm_nfc, write_plaintext_callback, username);
    g_signal_connect(tlm_nfc, "record-found", G_CALLBACK(_write_test_record_found_callback_check_username), username);
    g_signal_connect(tlm_nfc, "record-found", G_CALLBACK(_write_test_record_found_callback_increase_counter), &record_found_counter);
    g_signal_connect(tlm_nfc, "no-record-found", G_CALLBACK(_write_test_no_record_found_callback), NULL);
    while (1) {
        g_main_context_iteration(g_main_context_default(), TRUE);
        if(tag_lost_counter == 2 && record_found_counter == 1)
            break;
    }


    g_free(username);
    
    g_object_unref(tlm_nfc);
    g_main_loop_unref(loop);
}
END_TEST


Suite* common_suite (void)
{
    Suite *s = suite_create ("TLM NFC");
    
    /* Core test case */
    TCase *tc_core = tcase_create ("Tests");
//    tcase_add_test (tc_core, test_tlm_nfc_write);
    tcase_add_test (tc_core, test_tlm_nfc_read);
    tcase_add_test (tc_core, test_tlm_nfc_write);
    tcase_set_timeout(tc_core, 60);
    suite_add_tcase (s, tc_core);
    return s;
}

int main (void)
{
    int number_failed;

#if !GLIB_CHECK_VERSION (2, 36, 0)
    g_type_init ();
#endif

    Suite *s = common_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
  
