/*
 * FogLAMP Google Cloud Platform IoT-Core north plugin.
 *
 * Copyright (c) 2019 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <plugin_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string>
#include <logger.h>
#include <plugin_exception.h>
#include <iostream>
#include <config_category.h>
#include <version.h>
#include <gcp.h>


using namespace std;
using namespace rapidjson;

extern "C" {

#define PLUGIN_NAME "GCP"

/**
 * Plugin specific default configuration
 */
const char *default_config = QUOTE({
			"plugin" : {
				"description" : "Google Cloud Platform IoT-Core",
				"type" : "string",
				"default" : PLUGIN_NAME,
				"readonly" : "true"
				},
			"project_id" : {
				"description" : "The GCP IoT Core Project ID",
				"type" : "string",
				"default" : "",
				"order" : "1",
				"displayName" : "Project ID"
			},
			"region" : {
				"description" : "The GCP Region",
				"type" : "enumeration",
				"options" : [ "us-central1", "europe-west1", "asia-east1" ],
				"default" : "us-central1",
				"order" : "2",
				"displayName" : "The GCP Region"
			},
			"registry_id" : {
				"description" : "The Registry ID of the GCP Project",
				"type" : "string",
				"default" : "",
				"order" : "3",
				"displayName" : "Registry ID"
			},
			"device_id" : {
				"description" : "Device ID within GCP IoT Core",
				"type" : "string",
				"default" : "",
				"order" : "4",
				"displayName" : "Device ID"
			},
			"key" : {
				"description" : "Name of the key file to use",
				"type" : "string",
				"default" : "",
				"order" : "5",
				"displayName" : "Key Name"
			},
			"algorithm" : {
				"description" : "JWT algorithm",
				"type" : "enumeration",
				"options" : [ "ES256", "RS256" ],
				"default" : "RS256",
				"order" : "6",
				"displayName" : "JWT Algorithm"
			},
			"source": {
				"description" : "The source of data to send",
				"type" : "enumeration",
				"default" : "readings",
				"order" : "8",
				"displayName" : "Data Source",
				"options" : ["readings", "statistics"]
			}
		});

/**
 * The GCP plugin interface
 */

/**
 * The C API plugin information structure
 */
static PLUGIN_INFORMATION info = {
	PLUGIN_NAME,			// Name
	VERSION,			// Version
	0,				// Flags
	PLUGIN_TYPE_NORTH,		// Type
	"1.0.0",			// Interface version
	default_config			// Configuration
};

/**
 * Return the information about this plugin
 */
PLUGIN_INFORMATION *plugin_info()
{
	return &info;
}

/**
 * Initialise the plugin with configuration.
 *
 * This function is called to get the plugin handle.
 */
PLUGIN_HANDLE plugin_init(ConfigCategory* configData)
{

	GCP *gcp = new GCP();
	gcp->configure(configData);
	gcp->connect();

	return (PLUGIN_HANDLE)gcp;
}

/**
 * Send Readings data to historian server
 */
uint32_t plugin_send(const PLUGIN_HANDLE handle,
		     const vector<Reading *>& readings)
{
GCP	*gcp = (GCP *)handle;

	return gcp->send(readings);
}

/**
 * Shutdown the plugin
 *
 * Delete allocated data
 *
 * @param handle    The plugin handle
 */
void plugin_shutdown(PLUGIN_HANDLE handle)
{
GCP	*gcp = (GCP *)handle;

        delete gcp;
}

// End of extern "C"
};
