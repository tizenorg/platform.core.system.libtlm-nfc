/* vi: set et sw=4 ts=4 cino=t0,(0: */
/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of gtlm-nfc
 *
 * Copyright (C) 2013 Intel Corporation.
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

#include "gtlm-nfc.h"
#include <gio/gio.h>

GQuark
gtlm_nfc_error_quark (void)
{
    return g_quark_from_static_string ("gtlm-nfc");
}

#define GTLM_NFC_ERROR   (gtlm_nfc_error_quark())

/**
 * SECTION:gtlm-nfc
 * @short_description: a helper object that provides NFC functionality to user management code
 * @include: gtlm-nfc.h
 *
 * #GTlmNfc is used by Tizen user and login management to access NFC tags
 * and authenticate users through them. It provides signals to listen for tags
 * appearing and disappearing, and for reading and writing username/password pairs 
 * to tags.
 */
/**
 * GTlmNfcError:
 * @GTLM_NFC_ERROR_NONE: No error
 * @GTLM_NFC_ERROR_NO_TAG: Issued when attempting to write to an absent tag
 * 
 * This enum provides a list of errors that libtlm-nfc returns.
 * 
 */

/**
 * GTlmNfc:
 *
 * Opaque #GTlmNfc data structure.
 */
/**
 * GTlmNfcClass:
 * @parent_class: the parent class structure
 *
 * Opaque #GTlmNfcClass data structure.
 */

G_DEFINE_TYPE_WITH_CODE (GTlmNfc, gtlm_nfc, 
                         G_TYPE_OBJECT,
                         );

enum
{
    PROP_0
    
};

enum {
    SIG_TAG_FOUND,
    SIG_TAG_LOST,
    SIG_RECORD_FOUND,
    SIG_NO_RECORD_FOUND,
 
    SIG_MAX
};

static guint signals[SIG_MAX];


static gchar* _encode_username_password(const gchar* username, const gchar* password)
{
    GVariant* v = g_variant_new("(msms)", username, password);
    
    gchar* out = g_base64_encode(g_variant_get_data(v), g_variant_get_size(v));
    g_variant_unref(v);
    return out;
}

/**
 * gtlm_nfc_write_username_password:
 * @tlm_nfc: an instance of GTlmNfc object
 * @nfc_tag_path: an identificator of the nfc tag (returned by #GTlmNfc::tag-found)
 * @username: username to write
 * @password: password to write
 * @error: if non-NULL, set to an error, if one occurs
 * 
 * This function is used to write a username and password to a tag. The tag path
 * can be obtained by listening to #GTlmNfc::tag-found signals). @error is set to
 * @GTLM_NFC_ERROR_NO_TAG if no such tag exists.
 */
void gtlm_nfc_write_username_password(GTlmNfc* tlm_nfc,
                                      const gchar* nfc_tag_path,
                                      const gchar* username, 
                                      const gchar* password,
                                      GError** error)
{
    if (nfc_tag_path == NULL) {
        *error = g_error_new(GTLM_NFC_ERROR, GTLM_NFC_ERROR_NO_TAG, "No tag is present");
        return;
    }
    
    GDBusProxy* tag = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL, /* GDBusInterfaceInfo */
                                         "org.neard",
                                         nfc_tag_path,
                                         "org.neard.Tag",
                                         NULL, /* GCancellable */
                                         error);
    if (tag == NULL) {
        g_debug ("Error creating tag proxy: %s", (*error)->message);
        return;
    }    

    //gchar binary_data[] = {0x41, 0x42, 0x43, 0x44};
    gchar* binary_data = _encode_username_password(username, password);
    GVariant* payload =  g_variant_new_bytestring(binary_data);
    g_free(binary_data);
    
    GVariant* arguments = g_variant_new_parsed ("({'Type': <'MIME'>, 'MIME': <'application/gtlm-nfc'>, 'Payload' : %v },)", payload);
    
    GVariant *result = g_dbus_proxy_call_sync (tag,
                                        "Write",
                                        arguments,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        error);
    
   if (result == NULL) {
        g_debug ("Error writing to tag: %s", (*error)->message);
        g_object_unref(tag);
        return;
    }    

    g_variant_unref(result);
    g_object_unref(tag);
}

static void _decode_username_password(GTlmNfc* self, const gchar* data)
{
    gsize variant_s_size = 0;
    guchar* variant_s = g_base64_decode(data, &variant_s_size);
    
    GVariantType* v_t = g_variant_type_new("(msms)");
    GVariant* v = g_variant_new_from_data(v_t, variant_s, variant_s_size, FALSE, NULL, NULL);
    
    if (v == NULL) {
        g_debug("Couldn't decode Payload data to variant");
        g_variant_type_free(v_t);
        g_free(variant_s);
        g_signal_emit(self, signals[SIG_NO_RECORD_FOUND], 0);
        return;
    }
    
    gchar* username = NULL;
    gchar* password = NULL;
    g_variant_get(v, "(msms)", &username, &password);
    
    g_signal_emit(self, signals[SIG_RECORD_FOUND], 0, username, password);
    
    g_free(username);
    g_free(password);
    g_variant_unref(v);
    g_variant_type_free(v_t);
    g_free(variant_s);
}

static void
_handle_agent_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
    GTlmNfc* self = GTLM_NFC(user_data);
    gchar *parameters_str;

    parameters_str = g_variant_print (parameters, TRUE);
    g_debug ("Agent received method call: %s\n\tParameters: %s\n\tSender: %s\n\tObject path: %s\n\tInteface name: %s",
           method_name, parameters_str, sender, object_path, interface_name);
    g_free (parameters_str);
    
    if (g_strcmp0(method_name, "GetNDEF") != 0) {
        goto out;
    }

    GVariant* parameters_dict = g_variant_get_child_value(parameters, 0);
    if (parameters_dict == NULL)
    {
        g_debug ("Error getting parameters dict");
        g_signal_emit(self, signals[SIG_NO_RECORD_FOUND], 0);
        goto out;
    }

    gchar* payload_data;
    
    if (g_variant_lookup(parameters_dict, "Payload", "^ay", &payload_data) == FALSE) {
        g_debug ("Error getting raw Payload data");
        g_signal_emit(self, signals[SIG_NO_RECORD_FOUND], 0);        
        g_variant_unref(parameters_dict);
        goto out;
    }
    g_variant_unref(parameters_dict);
    
    _decode_username_password(self, payload_data);
    g_free(payload_data);
    
out:    
    g_dbus_method_invocation_return_value (invocation, NULL);
}

static GVariant *
_handle_agent_get_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GError          **error,
                     gpointer          user_data)
{
    return NULL;
}

static gboolean
_handle_agent_set_property (GDBusConnection  *connection,
                     const gchar      *sender,
                     const gchar      *object_path,
                     const gchar      *interface_name,
                     const gchar      *property_name,
                     GVariant         *value,
                     GError          **error,
                     gpointer          user_data)
{
    return FALSE;
}

static void
_setup_nfc_adapter(GTlmNfc *self, GDBusProxy *nfc_adapter)
{
    GError* error = NULL;
    gboolean powered;
    gboolean polling;

    // for some reason the cached properties aren't updated, so we request them directly
    //GVariant* powered_v = g_dbus_proxy_get_cached_property(nfc_adapter, "Powered");
    GVariant* powered_resp = g_dbus_connection_call_sync (self->system_bus,
                                        "org.neard",
                                        g_dbus_proxy_get_object_path(nfc_adapter) ,
                                        "org.freedesktop.DBus.Properties",
                                        "Get",
                                        g_variant_new("(ss)", 
                                                      g_dbus_proxy_get_interface_name(nfc_adapter), 
                                                      "Powered"
                                                     ),
                                        NULL,              
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
    
    if (powered_resp == NULL) {
        g_debug("Powered property is absent on an adapter: %s", error->message);
        g_error_free (error);
        return;
    }
    
    GVariant* powered_v;
    g_variant_get_child(powered_resp, 0, "v", &powered_v);
    
    if (powered_v == NULL || !g_variant_is_of_type(powered_v, G_VARIANT_TYPE_BOOLEAN)) {
        g_debug("Error retrieving powered property");
        return;
    }
    
    powered = g_variant_get_boolean(powered_v);
    g_variant_unref(powered_resp);
    
    //GVariant* polling_v = g_dbus_proxy_get_cached_property(nfc_adapter, "Polling");
    GVariant* polling_resp = g_dbus_connection_call_sync (self->system_bus,
                                        "org.neard",
                                        g_dbus_proxy_get_object_path(nfc_adapter) ,
                                        "org.freedesktop.DBus.Properties",
                                        "Get",
                                        g_variant_new("(ss)", 
                                                      g_dbus_proxy_get_interface_name(nfc_adapter), 
                                                      "Polling"
                                                     ),
                                        NULL,              
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
    if (polling_resp == NULL) {
        g_debug("Polling property is absent on an adapter: %s", error->message);
        g_error_free (error);
        return;
    }
    
    GVariant* polling_v;
    g_variant_get_child(polling_resp, 0, "v", &polling_v);
    
    if (polling_v == NULL || !g_variant_is_of_type(polling_v, G_VARIANT_TYPE_BOOLEAN)) {
        g_debug("Error retrieving polling property");
        return;
    }
    
    polling = g_variant_get_boolean(polling_v);
    g_variant_unref(polling_resp);
    
    
    GVariant* response;
    if (powered == FALSE) {
        // switch power on

        response = g_dbus_connection_call_sync (self->system_bus,
                                        "org.neard",
                                        g_dbus_proxy_get_object_path(nfc_adapter) ,
                                        "org.freedesktop.DBus.Properties",
                                        "Set",
                                        g_variant_new("(ssv)", 
                                                      g_dbus_proxy_get_interface_name(nfc_adapter), 
                                                      "Powered",
                                                      g_variant_new_boolean(TRUE)
                                                     ),
                                        NULL,              
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
        
        if (response == NULL ) {
            g_debug("Error swithing NFC adapter on: %s", error->message);
            g_error_free (error);
            return;
        }
        g_variant_unref(response);
        g_debug("Switched NFC adapter on");
    } else
        g_debug("Adapter already switched on");

    if (polling == FALSE) {
        // start polling
        response = g_dbus_proxy_call_sync (nfc_adapter,
                                        "StartPollLoop",
                                        g_variant_new("(s)", "Initiator"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
        if (response == NULL)
        {
            g_debug("Error starting NFC poll loop: %s", error->message);
            g_error_free (error);
            return;
        }
        g_variant_unref(response);
        g_debug("Started NFC poll loop");

    } else
        g_debug("Adapter already in polling mode");


}


void _on_interface_added(GDBusObjectManager *manager,
                         GDBusObject        *object,
                         GDBusInterface     *interface,
                         gpointer            user_data)
{
    GTlmNfc* self = GTLM_NFC(user_data);
    GDBusProxy* proxy = G_DBUS_PROXY(interface);
    g_debug("Object %s added interface %s", 
                    g_dbus_object_get_object_path (object),
                    g_dbus_proxy_get_interface_name (proxy));
    
    if (g_strcmp0(g_dbus_proxy_get_interface_name (proxy),
                "org.neard.Adapter") == 0) {
        _setup_nfc_adapter(self, proxy);
        return;
    }
    if (g_strcmp0(g_dbus_proxy_get_interface_name (proxy),
                "org.neard.Tag") == 0) {
        g_signal_emit(self, signals[SIG_TAG_FOUND], 0, g_dbus_object_get_object_path (object));
        return;
    }

    if (g_strcmp0(g_dbus_proxy_get_interface_name (proxy),
                "org.neard.Record") == 0) {
        GVariant* type_v = g_dbus_proxy_get_cached_property(proxy, "Type");
        if (type_v == NULL || !g_variant_is_of_type(type_v, G_VARIANT_TYPE_STRING)) {
            g_debug("Type property is absent on a record");
            g_signal_emit(self, signals[SIG_NO_RECORD_FOUND], 0);
            return;
        }
        const gchar* type = g_variant_get_string(type_v, NULL);
        g_debug("Record has type %s", type);
        if (g_strcmp0(type, "MIME") != 0) {
            g_variant_unref(type_v);
            g_signal_emit(self, signals[SIG_NO_RECORD_FOUND], 0);
            return;
        }
        g_variant_unref(type_v);

        GVariant* mimetype_v = g_dbus_proxy_get_cached_property(proxy, "MIME");
        if (mimetype_v == NULL || !g_variant_is_of_type(type_v, G_VARIANT_TYPE_STRING)) {
            g_debug("MIME property is absent on a record");
            g_signal_emit(self, signals[SIG_NO_RECORD_FOUND], 0);
            return;
        }
        const gchar* mimetype = g_variant_get_string(mimetype_v, NULL);
        g_debug("Record has MIME type %s", mimetype);
        if (g_strcmp0(mimetype, "application/gtlm-nfc") != 0) {
            g_variant_unref(mimetype_v);
            g_signal_emit(self, signals[SIG_NO_RECORD_FOUND], 0);
            return;
        }
        g_variant_unref(mimetype_v);
    }
}

static void
_setup_nfc_adapters(GTlmNfc *self)
{
    GList* objects = g_dbus_object_manager_get_objects(self->neard_manager);
    GList* objects_iter = objects;
    
    while (objects_iter != NULL) {
        GList* interfaces = g_dbus_object_get_interfaces(objects_iter->data);
        GList* interfaces_iter = interfaces;
        while (interfaces_iter != NULL) {
            g_debug("Checking managed object %s, interface %s", 
                    g_dbus_object_get_object_path (objects_iter->data),
                    g_dbus_proxy_get_interface_name (interfaces_iter->data));
            if (g_strcmp0(g_dbus_proxy_get_interface_name (interfaces_iter->data),
                "org.neard.Adapter") == 0) {
                _setup_nfc_adapter(self, interfaces_iter->data);
            }
            g_object_unref(interfaces_iter->data);
            interfaces_iter = interfaces_iter->next;
        }
        g_list_free(interfaces);
        g_object_unref(objects_iter->data);
        objects_iter = objects_iter->next;
    }
    g_list_free(objects);
    
}



void _on_interface_removed(GDBusObjectManager *manager,
                         GDBusObject        *object,
                         GDBusInterface     *interface,
                         gpointer            user_data)
{
    GTlmNfc* self = GTLM_NFC(user_data);
    GDBusProxy* proxy = G_DBUS_PROXY(interface);
    g_debug("Object %s removed interface %s", 
                    g_dbus_object_get_object_path (object),
                    g_dbus_proxy_get_interface_name (proxy));    

    if (g_strcmp0(g_dbus_proxy_get_interface_name (proxy),
                "org.neard.Tag") == 0) {
        g_signal_emit(self, signals[SIG_TAG_LOST], 0, g_dbus_object_get_object_path (object));
    
        GVariant* adapter_v = g_dbus_proxy_get_cached_property(proxy, "Adapter");
        if (adapter_v == NULL || !g_variant_is_of_type(adapter_v, G_VARIANT_TYPE_OBJECT_PATH)) {
            g_debug("Adapter property is absent on a tag");
            return;
        }
        const gchar* adapter_path = g_variant_get_string(adapter_v, NULL);
        g_debug("Tag belongs to adapter %s", adapter_path);
        GError* error = NULL;
        GDBusProxy* adapter = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SYSTEM,
                                         G_DBUS_PROXY_FLAGS_NONE,
                                         NULL, /* GDBusInterfaceInfo */
                                         "org.neard",
                                         adapter_path,
                                         "org.neard.Adapter",
                                         NULL, /* GCancellable */
                                         &error);
        if (adapter == NULL) {
            g_debug ("Error creating adapter proxy: %s", error->message);
            g_error_free(error);
            g_variant_unref(adapter_v);
            return;
        }        
        // start polling on an adapter
        _setup_nfc_adapter(self, adapter);
        g_object_unref(adapter);
        g_variant_unref(adapter_v);
        return;
    }

    
}


void _on_object_added(GDBusObjectManager *manager,
                         GDBusObject        *object,
                         gpointer            user_data)
{
    //GTlmNfc* self = GTLM_NFC(user_data);
    g_debug("Object %s added", 
                    g_dbus_object_get_object_path (object));
    
    GList* interfaces = g_dbus_object_get_interfaces(object);
    GList* interfaces_iter = interfaces;
    while (interfaces_iter != NULL) {
        _on_interface_added(manager, object, interfaces_iter->data, user_data);
        g_object_unref(interfaces_iter->data);
        interfaces_iter = interfaces_iter->next;
    }
    g_list_free(interfaces);
    g_debug("Finished checking interfaces");   
}

void _on_object_removed(GDBusObjectManager *manager,
                         GDBusObject        *object,
                         gpointer            user_data)
{
    //GTlmNfc* self = GTLM_NFC(user_data);
    g_debug("Object %s removed", 
                    g_dbus_object_get_object_path (object));

    GList* interfaces = g_dbus_object_get_interfaces(object);
    GList* interfaces_iter = interfaces;
    while (interfaces_iter != NULL) {
        _on_interface_removed(manager, object, interfaces_iter->data, user_data);
        g_object_unref(interfaces_iter->data);
        interfaces_iter = interfaces_iter->next;
    }
    g_list_free(interfaces);
    g_debug("Finished checking interfaces");       
}

void _on_property_changed (GDBusObjectManagerClient *manager,
                                                        GDBusObjectProxy         *object_proxy,
                                                        GDBusProxy               *interface_proxy,
                                                        GVariant                 *changed_properties,
                                                        GStrv                     invalidated_properties,
                                                        gpointer                  user_data)
{
    gchar *parameters_str;

    parameters_str = g_variant_print (changed_properties, TRUE);
    g_debug("Property of object %s changed:\n%s",
            g_dbus_object_get_object_path(G_DBUS_OBJECT(object_proxy)), parameters_str);
    g_debug("Invalidated properties:");
    while (*invalidated_properties != NULL) {
        g_print(*invalidated_properties);
        invalidated_properties++;
    }
    g_free(parameters_str);
}

static void _setup_agent_and_adapters(GTlmNfc* self)
{
    GError *error = NULL;

    /* Introspection data for the agent object we are exporting */
    const gchar introspection_xml[] =
        "<node>"
        "  <interface name='org.neard.NDEFAgent'>"
        "    <method name='GetNDEF'>"
        "      <arg type='a{sv}' name='values' direction='in'/>"
        "    </method>"
        "    <method name='Release'>"
        "    </method>"
        "  </interface>"
        "</node>";
    
    const GDBusInterfaceVTable interface_vtable =
    {
        _handle_agent_method_call,
        _handle_agent_get_property,
        _handle_agent_set_property
    };

    self->system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    
    if (self->system_bus == NULL) {
        g_debug ("Error getting a system bus: %s", error->message);
        g_error_free (error);
        return;
    }        
    
    GDBusNodeInfo *introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
      
    self->agent_registration_id = g_dbus_connection_register_object (self->system_bus,
                                                       "/org/tlmnfc/agent",
                                                       introspection_data->interfaces[0],
                                                       &interface_vtable,
                                                       self,
                                                       NULL,
                                                       &error);

    if (self->agent_registration_id <= 0) {
        g_debug ("Error registering an agent object: %s", error->message);
        g_dbus_node_info_unref (introspection_data);
        g_error_free (error);
        return;
    }
    g_dbus_node_info_unref (introspection_data);
    
    GVariant* agent_register_response = g_dbus_connection_call_sync (self->system_bus,
                                         "org.neard",
                                         "/org/neard",
                                         "org.neard.AgentManager",
                                        "RegisterNDEFAgent",
                                        g_variant_new("(os)", 
                                                      "/org/tlmnfc/agent", 
                                                      "application/gtlm-nfc"),
                                        NULL,              
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
    if (agent_register_response == NULL) {
        g_debug ("Error registering an agent with neard: %s", error->message);
        g_error_free (error);
        return;
    }
    g_variant_unref(agent_register_response);

    
    self->neard_manager =  g_dbus_object_manager_client_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                         G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                         "org.neard",
                                         "/",
                                         NULL, NULL, NULL, NULL,
                                         &error);
    if (self->neard_manager == NULL)
    {
        g_debug ("Error creating neard object manager: %s", error->message);
        g_error_free (error);
        return ;
    }
    
    // subscribe to interface added/removed signals
    g_signal_connect (G_DBUS_OBJECT_MANAGER(self->neard_manager),
                    "interface-added",
                    G_CALLBACK (_on_interface_added),
                    self);
    g_signal_connect (G_DBUS_OBJECT_MANAGER(self->neard_manager),
                    "interface-removed",
                    G_CALLBACK (_on_interface_removed),
                    self);
    g_signal_connect (G_DBUS_OBJECT_MANAGER(self->neard_manager),
                    "object-added",
                    G_CALLBACK (_on_object_added),
                    self);
    g_signal_connect (G_DBUS_OBJECT_MANAGER(self->neard_manager),
                    "object-removed",
                    G_CALLBACK (_on_object_removed),
                    self);
    
    
    g_signal_connect (G_DBUS_OBJECT_MANAGER(self->neard_manager),
                    "interface-proxy-properties-changed",
                    G_CALLBACK (_on_property_changed),
                    self);
    

    
    _setup_nfc_adapters(self);    
    return;
}


static void
gtlm_nfc_init (GTlmNfc *self)
{
    
    _setup_agent_and_adapters(self);
}

static void
gtlm_nfc_set_property (GObject      *object,
                                       guint         property_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
    switch (property_id)
    {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
gtlm_nfc_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    GTlmNfc *tlm_nfc = GTLM_NFC (object);
    (void) tlm_nfc;
    switch (prop_id)
    {
            
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void gtlm_nfc_dispose(GObject *object)
{
    GTlmNfc* self = GTLM_NFC (object);

    if (self->system_bus) {
        GError* error = NULL;
        GVariant* agent_register_response = g_dbus_connection_call_sync (self->system_bus,
                                         "org.neard",
                                         "/org/neard",
                                         "org.neard.AgentManager",
                                        "UnregisterNDEFAgent",
                                        g_variant_new("(os)",
                                                      "/org/tlmnfc/agent",
                                                      "application/gtlm-nfc"),
                                        NULL,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
        if (agent_register_response == NULL) {
            g_debug ("Error unregistering an agent with neard: %s", error->message);
            g_error_free (error);
        }
        g_variant_unref(agent_register_response);
    }
    if (self->agent_registration_id > 0)
        if (g_dbus_connection_unregister_object(self->system_bus, self->agent_registration_id) == FALSE)
            g_debug("Error unregistering agent object");


    if (self->neard_manager) {
        g_object_unref(self->neard_manager);
        self->neard_manager = NULL;
    }
    if (self->system_bus) {
        g_object_unref(self->system_bus);
        self->system_bus = NULL;
    }
    
    G_OBJECT_CLASS (gtlm_nfc_parent_class)->dispose (object);
}

static void gtlm_nfc_finalize(GObject *object)
{
    //GTlmNfc* self = GTLM_NFC (object);
    

    G_OBJECT_CLASS (gtlm_nfc_parent_class)->finalize (object);
}

static void
gtlm_nfc_class_init (GTlmNfcClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    
    gobject_class->set_property = gtlm_nfc_set_property;
    gobject_class->get_property = gtlm_nfc_get_property;
    gobject_class->dispose = gtlm_nfc_dispose;
    gobject_class->finalize = gtlm_nfc_finalize;
    
    
    /**
     * GTlmNfc::tag-found:
     * @tlm_nfc: the object which emitted the signal
     * @nfc_tag_path: an identifier of the tag (use in gtlm_nfc_write_username_password())
     * 
     * This signal is issued by #GTlmNfc object when a tag has been found.
     */
    signals[SIG_TAG_FOUND] = g_signal_new ("tag-found", 
        G_TYPE_TLM_NFC,
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
        1, G_TYPE_STRING);
        //2, G_TYPE_STRING, G_TYPE_STRING);    

    /**
     * GTlmNfc::tag-lost:
     * @tlm_nfc: the object which emitted the signal
     * @nfc_tag_path: an identifier of the tag
     * 
     * This signal is issued by #GTlmNfc object when a tag has been lost.
     */
    signals[SIG_TAG_LOST] = g_signal_new ("tag-lost", 
        G_TYPE_TLM_NFC,
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
        1, G_TYPE_STRING);

    /**
     * GTlmNfc::record-found:
     * @tlm_nfc: the object which emitted the signal
     * @username: the username on the tag
     * @password: the password on the tag
     * 
     * This signal is issued by #GTlmNfc object when a username and password pair
     * has been found on a tag.
     */
    signals[SIG_RECORD_FOUND] = g_signal_new ("record-found", 
        G_TYPE_TLM_NFC,
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
        2, G_TYPE_STRING, G_TYPE_STRING);    

    /**
     * GTlmNfc::no-record-found:
     * @tlm_nfc: the object which emitted the signal
     * 
     * This signal is issued by #GTlmNfc object when a username and password pair
     * has not been found on a tag
     */
    signals[SIG_NO_RECORD_FOUND] = g_signal_new ("no-record-found", 
        G_TYPE_TLM_NFC,
        G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
        0);     
    
}
