/*
 * FogLAMP Google Cloud Platform IoT-Core north plugin.
 *
 * Copyright (c) 2019 Dianomic Systems
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <gcp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "jwt.h"
#include "openssl/ec.h"
#include "openssl/evp.h"
#include "MQTTClient.h"
#include "simple_https.h"
#include <rapidjson/document.h>

using namespace rapidjson;

static const int kQos = 1;
static const unsigned long kTimeout = 10000L;
static const char* kUsername = "unused";

static const unsigned long kInitialConnectIntervalMillis = 500L;
static const unsigned long kMaxConnectIntervalMillis = 6000L;
static const unsigned long kMaxConnectRetryTimeElapsedMillis = 900000L;
static const float kIntervalMultiplier = 1.5f;

using namespace std;

const string GCP::m_address("ssl://mqtt.googleapis.com:8883");

/*
 * Callback functions
 *
 * C Functions that are called by the MQTT library for various events
 */

/**
 * Callback function that is called when a message for one of the topic we subscribe to arrives.
 *
 * @param context	The GCP object instance
 * @param topicName	The name of the topic the message arrived on
 * @param topicLen	The length of the topic name
 * @param message	The MQTT message content
 */
static int messageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
GCP *gcp = (GCP *)context;

	gcp->msgArrived(topicName, message);
	return 1;
}

/**
 * Callback function that is called when the MQTT connection is lost
 *
 * @param context	The GCP object instance
 * @param cause		The cause of the lost connection
 */
static void connectionLost(void *context, char *cause)
{
GCP *gcp = (GCP *)context;

	gcp->lostConnection(cause);
}

/**
 * Callback function that is called when an MQTT message is delivered.
 *
 * @param context	The GCP object instance
 * @param dt		The delivery token of the packet that was delivered
 */
static void deliveryComplete(void *context, MQTTClient_deliveryToken dt)
{
GCP *gcp = (GCP *)context;

	gcp->delivered(dt);
}


/**
 * Constructor for the GCP object
 */
GCP::GCP() : m_jwtStr(NULL), m_subscribed(false), m_connected(false),
	m_lastDelivered(0), m_lastSent(0), m_jwtExpire(0)
{
	m_log = Logger::getLogger();
	OpenSSL_add_all_algorithms();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_ciphers();
}

/**
 * Destructor fpr the GCP object
 */
GCP::~GCP()
{
	if (m_jwtStr)
	{
		free(m_jwtStr);
		m_jwtStr = NULL;
	}
}

/**
 * GCP configuration method. This is mostly concerned with getting
 * the data from the FogLAMP configuration dategory that defines
 * the paramters of the GCP IoT Core we will connect with.
 *
 * @param conf	FogLAMP configuration category
 */
void GCP::configure(const ConfigCategory *conf)
{
	if (conf->itemExists("project_id"))
		m_projectID = conf->getValue("project_id");
	else
		m_log->error("Missing project ID in configuration");
	if (conf->itemExists("region"))
		m_region = conf->getValue("region");
	else
		m_log->error("Missing region in configuration");
	if (conf->itemExists("registry_id"))
		m_registryID = conf->getValue("registry_id");
	else
		m_log->error("Missing registry ID in configuration");
	if (conf->itemExists("device_id"))
		m_deviceID = conf->getValue("device_id");
	else
		m_log->error("Missing device ID in configuration");
	m_clientID = "projects/" + m_projectID +
		"/locations/" + m_region + "/registries/" + m_registryID
		+ "/devices/" + m_deviceID;
	m_topic = "/devices/" + m_deviceID + "/events";
	if (conf->itemExists("key"))
		m_key = conf->getValue("key");
	else
		m_log->error("Missing device key in configuration");
	if (conf->itemExists("algorithm"))
		m_algorithm = conf->getValue("algorithm");
	else
		m_log->error("Missing JWT algorithm in configuration");
}

/**
 * Send a block of readings to GCP IoT core service using MQTT
 *
 * @param readings	The readings to send
 * @return 		The number of readings sent
 */
uint32_t GCP::send(const vector<Reading *>& readings)
{
uint32_t	n = 0;
struct timeval tv1, tv2;
int		rc;

	gettimeofday(&tv1, NULL);
	m_log->warn("GCP Send block of %d ....", readings.size());
	if (!m_connected)
	{
		rc = connect();
		if (rc != MQTTCLIENT_SUCCESS)
		{
			m_log->error("Failed to connect to MQTT service %s, %d", m_address.c_str(), rc);
			return 0;
		}
	}


	/*
	 * First do a pass to get all the asset names in this block of readings and
	 * add them to the set of assets to process.
	 */
	for (auto reading = readings.cbegin(); reading != readings.cend(); reading ++)
	{
		string assetName = (*reading)->getAssetName();
		mapAssetName(assetName);
		if (m_asset.find(assetName) == m_asset.end())
		{
				m_asset.insert(assetName);
		}
	}

	string payload = "{";
	bool first = true;

	/*
	 * For each asset gather the readings for that asset
	 */
	for (auto itr = m_asset.cbegin(); itr != m_asset.cend(); itr++)
	{
		bool outputAsset = true;
		for (auto reading = readings.cbegin(); reading != readings.cend(); reading++)
		{

			string assetName = (*reading)->getAssetName();
			mapAssetName(assetName);
			if (itr->compare(assetName))	// Skip over if asset name does not match
				continue;
			if (outputAsset)
			{
				if (!first)
				{
					payload += ",";
				}
				payload += "\"";
				payload += *itr;
				payload += "\" : [ ";
				outputAsset = false;
			}
			else
			{
				payload += ",";
			}
			payload += makePayload(*reading);
			n++;
		}
		if (!outputAsset)
		{
			payload += "]";
		}
	}
	payload += "}";
	char *pl = strdup(payload.c_str());
	char topic[1024];
	snprintf(topic, sizeof(topic), "/devices/%s/events", m_deviceID.c_str());
	int retryCnt = 0;
retry:
	if (!m_connected)
	{
		m_log->info("GCP connection lost, reconnecting");
		if ((rc = connect()) != MQTTCLIENT_SUCCESS)
		{
			m_log->error("GCP Send block lost connection after %d readings",
				       n);
			return 0;
		}
	}
	if ((rc = publish(topic, pl, strlen(pl))) == MQTTCLIENT_SUCCESS)
	{
		m_log->info("Published %s, %d sent, %d delivered", pl, m_lastSent, m_lastDelivered);
	}
	else if (rc == -3)
	{
		m_log->info("Publish returned -3, retry?");
		// We got disconnected
		disconnect();
		if (retryCnt++ < 3)
			goto retry;
		m_log->error("Failed after 3 disconnects to publish %s", topic);
	}
	else
	{
		m_log->error("MQTT publication to topic %s failed, %d", topic, rc);
		disconnect();
	}
	free(pl);
	// Wait for last message sent to complete
	m_log->info("Waiting for delivery completion of the message");
	MQTTClient_deliveryToken dt = m_lastSent;
	if ((rc = MQTTClient_waitForCompletion(m_client, dt, kTimeout)) != MQTTCLIENT_SUCCESS)
		m_log->error("Failed to complete message transmission, %d", rc);
	gettimeofday(&tv2, NULL);
	m_log->warn("GCP Send block sent %d readings, averages %.1f per second", n,
			(float)(1000 * n) / (((tv2.tv_sec - tv1.tv_sec) * 1000) + (tv2.tv_usec - tv1.tv_usec) / 1000));
	return n;
}

/**
 * Construct a payload from a single reading.
 *
 * @param reading	The reading to use for payload construction
 * @return	The JSON payload
 */
string GCP::makePayload(Reading *reading)
{
	string payload = "{";
	struct timeval tm;
	reading->getTimestamp(&tm);
	payload += "\"ts\":\"";
	// Add timestamp
	payload += reading->getAssetDateUserTime(Reading::FMT_DEFAULT, true);
	payload += "\",";
	string assetName = reading->getAssetName();
	vector<Datapoint *> dpv = reading->getReadingData();
	for (auto dp = dpv.cbegin(); dp != dpv.cend(); dp++)
	{
		payload += (*dp)->toJSONProperty();
		if ((dp + 1) != dpv.cend())
		{
			payload += ",";
		}
	}
	payload += "}";
	return payload;
}

/**
 * Connect to the Google Cloud IoT Core using MQTT
 *
 * @return connection status
 */
int GCP::connect()
{
int rc = -1;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

	createJWT();
	MQTTClient_create(&m_client, m_address.c_str(), m_clientID.c_str(),
			MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks(m_client, this, connectionLost, messageArrived, deliveryComplete);
	conn_opts.keepAliveInterval = 60;
	conn_opts.cleansession = 1;
	conn_opts.username = kUsername;
	conn_opts.password = m_jwtStr;
	MQTTClient_SSLOptions sslopts = MQTTClient_SSLOptions_initializer;

	getRootPath();
	getKeyPath();
	sslopts.trustStore = m_rootPath.c_str();
	sslopts.privateKey = m_keyPath.c_str();
	conn_opts.ssl = &sslopts;
	unsigned long retry_interval_ms = kInitialConnectIntervalMillis;
	unsigned long total_retry_time_ms = 0;
	while ((rc = MQTTClient_connect(m_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
	{
		if (rc == 3)
		{
		      	// connection refused: server unavailable
			usleep(retry_interval_ms * 1000);
			total_retry_time_ms += retry_interval_ms;
			if (total_retry_time_ms >= kMaxConnectRetryTimeElapsedMillis)
			{
				m_log->error("Failed to connect, maximum retry time exceeded.");
				return -1; 
			}
			retry_interval_ms *= kIntervalMultiplier;
			if (retry_interval_ms > kMaxConnectIntervalMillis)
			{
				retry_interval_ms = kMaxConnectIntervalMillis;
			}
		}
		else
		{
			if (rc < 0)
			{
				m_log->error("Failed to connect to MQTT server %s, return code %d\n", 
					m_address.c_str(), rc);
			}
			else
			{
				switch (rc)
				{
					case 1:
						m_log->error("MQTT Connection refused: Unacceptable protocol version");
						break;
					case 2:
						m_log->error("MQTT Connection refused: Identifier rejected");
						break;
					case 3:
						m_log->error("MQTT Connection refused: Server unavailable");
						break;
					case 4:
						m_log->error("MQTT Connection refused: Bad user name or password");
						break;
					case 5:
						m_log->error("MQTT Connection refused: Not authorized");
						break;
					default:
						m_log->error("Failed to connect to MQTT server %s, return code %d\n", 
							m_address.c_str(), rc);
						break;
				}
			}
			return -1;
		}
	}
	if (rc == MQTTCLIENT_SUCCESS)
	{
		m_connected = true;
		createSubscriptions();
	}
	return rc;
}

/**
 * Publish a reading payload to the GCP IoT Core Device default topic
 * using MQTT
 * 
 * @param payload	The payload to publich
 * @param payload_size	Size of the payload
 */
int GCP::publish(char *payload, const int payload_size)
{
	return publish(m_topic, payload, payload_size);
}

/**
 * Publish a reading payload to the GCP IoT Core Device topic using
 * MQTT
 * 
 * @param topic		The topic to send to
 * @param payload	The payload to publich
 * @param payload_size	Size of the payload
 */
int GCP::publish(const string& topic, char *payload, const int payload_size)
{
MQTTClient_message pubmsg = MQTTClient_message_initializer;
MQTTClient_deliveryToken token = {++m_lastSent};

	pubmsg.payload = payload;
	pubmsg.payloadlen = payload_size;
	pubmsg.qos = 0;
	pubmsg.retained = 0;
	return MQTTClient_publishMessage(m_client, topic.c_str(), &pubmsg, &token);
}


/**
 * Create the subscriptions to the IoT core to get the errors messages and other
 * useful data published by IoT core for the gateway.
 */
void GCP::createSubscriptions()
{
	char	topic[1024];
	snprintf(topic, sizeof(topic), "/devices/%s/errors", m_deviceID.c_str());
	int rc;
	if ((rc = MQTTClient_subscribe(m_client, topic, 0)) != MQTTCLIENT_SUCCESS)
	{
		m_log->error("Failed to subscribe to error topic '%s', %d", topic, rc);
	}
}

/**
 * Disconnect from the IoT Core MQTT
 */
void GCP::disconnect()
{
	m_connected = false;
	MQTTClient_disconnect(m_client, 10000);
	MQTTClient_destroy(&m_client);
}

/**
 * Called when a message has been delivered
 * 
 * @param dt	Delivery token
 */
void GCP::delivered(MQTTClient_deliveryToken dt)
{
	if (dt > m_lastDelivered)
		m_lastDelivered = dt;
}

/**
 * Process an MQTT message from the Cloud IoT Core
 *
 * @param topic	The topic that IoT published to
 * @param msg	The message content that IoT Core published
 */
void GCP::msgArrived(char *topic, MQTTClient_message *msg)
{
	m_log->error("MQTT message received for topic '%s'", topic);
	int len = msg->payloadlen;
	char *buf = (char *)malloc(len + 1);
	memcpy(buf, msg->payload, len);
	buf [len] = 0;
	m_log->error("Message payload is %*s", len, buf);
	free(buf);
	MQTTClient_freeMessage(&msg);
	MQTTClient_free(topic);
}

/**
 * Handle a failed connection event from the underlying MQTT library.
 *
 * @param reason	The reason for the disconnection
 */
void GCP::lostConnection(const char *reason)
{
	m_log->error("MQTT connection lost: %s", reason);
	m_connected = false;
}

/**
 * Calculates a JSON Web Token (JWT) given the path to a private key and
 * Google Cloud project ID. The JWT token is saved in the member variable
 * m_jwtStr.
 */
void GCP::createJWT()
{
char iat_time[sizeof(time_t) * 3 + 2];
char exp_time[sizeof(time_t) * 3 + 2];
uint8_t* key = NULL; // Stores the Base64 encoded certificate
size_t key_len = 0;
jwt_t *jwt = NULL;
int ret = 0;
char *out = NULL;

	if (m_jwtExpire && m_jwtExpire < time(0))
	{
		m_log->info("JWT token has not yet expired");
		return;
	}
	else
	{
		m_log->info("Generating a new JWT token for MQTT bridge.");
	}
	// Read private key from file
	FILE *fp = fopen(getKeyPath().c_str(), "r");
	if (fp == (void*) NULL)
	{
		m_log->error("Could not open private key file: %s\n", getKeyPath().c_str());
		return;
	}
	fseek(fp, 0L, SEEK_END);
	key_len = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	key = (uint8_t *)malloc(sizeof(uint8_t) * (key_len + 1)); // certificate length + \0

	if (fread(key, 1, key_len, fp) != key_len)
	{
		m_log->error("Failed to read key %s", getKeyPath().c_str());
	}
	key[key_len] = '\0';
	fclose(fp);

	// Get JWT parts
	getIatExp(iat_time, exp_time, sizeof(iat_time));

	jwt_new(&jwt);

	// Write JWT
	ret = jwt_add_grant(jwt, "iat", iat_time);
	if (ret)
	{
		m_log->error("Error setting issue timestamp: %d\n", ret);
	}
	ret = jwt_add_grant(jwt, "exp", exp_time);
	if (ret)
	{
		m_log->error("Error setting expiration: %d\n", ret);
	}
	ret = jwt_add_grant(jwt, "aud", m_projectID.c_str());
	if (ret)
	{
		m_log->error("Error adding audience: %d\n", ret);
	}
	ret = jwt_set_alg(jwt, getAlgorithm(), key, key_len);
	if (ret)
	{
		m_log->error("Error during set alg: %d\n", ret);
	}
	out = jwt_encode_str(jwt);
	if(!out)
	{
		extern int errno;
		m_log->error("Error during JWT token creation: %d", errno);
	}
	if (m_jwtStr)
	{
		free(m_jwtStr);
	}
	m_jwtStr = out;

	jwt_free(jwt);
	free(key);
	m_jwtExpire = time(0) + 3500;	// Set expiry time to give us a little leeway
}

/**
 * Populate the issue time and expiry time of the JWT token
 *
 * @param iat	Buffer to write issue time into
 * @param exp	Buffer to write expiry time into (issue time + 1 hour)
 * @param time_size	The size of each of the buffers above
 */
void GCP::getIatExp(char* iat, char* exp, int time_size)
{
time_t now_seconds = time(NULL);

	snprintf(iat, time_size, "%lu", now_seconds);
	snprintf(exp, time_size, "%lu", now_seconds + 3600);
}

/**
 * Map the algorithm name used to configure us into a JWT alg type
 * 
 * @return jwt_alg_t	The algorithm type
 */
jwt_alg_t GCP::getAlgorithm()
{
	if (m_algorithm.compare("RS256") == 0)
	{
		return JWT_ALG_RS256;
	}
	if (m_algorithm.compare("ES256") == 0)
	{
		return JWT_ALG_ES256;
	}
	return JWT_ALG_ES256;
}

/**
 * Return the path to the key for this device.
 *
 * @return the pathname of the key file
 */
string GCP::getKeyPath()
{
	if (getenv("FOGLAMP_DATA"))
	{
		m_keyPath = getenv("FOGLAMP_DATA");
		m_keyPath += "/etc/certs/";
	}
	else if (getenv("FOGLAMP_ROOT"))
	{
		m_keyPath = getenv("FOGLAMP_ROOT");
		m_keyPath += "/data/etc/certs/";
	}
	else
	{
		m_keyPath = "/usr/local/foglamp/data/etc/certs/"; 
	}
	m_keyPath +=	m_key;
	m_keyPath += ".pem";

	return m_keyPath;
}

/**
 * Return the path of the root key
 *
 * @return path to the rot key file.
 */
string GCP::getRootPath()
{
	if (getenv("FOGLAMP_DATA"))
	{
		m_rootPath = getenv("FOGLAMP_DATA");
		m_rootPath += "/etc/certs/";
	}
	else if (getenv("FOGLAMP_ROOT"))
	{
		m_rootPath = getenv("FOGLAMP_ROOT");
		m_rootPath += "/data/etc/certs/";
	}
	else
	{
		m_rootPath = "/usr/local/foglamp/data/etc/certs/"; 
	}
	m_rootPath += "roots.pem";

	return m_rootPath;
}

/**
 * Map an asset name to a suitable device name in GCP IoT Core
 *
 * @param assetName 	The asset name to map
 * @result		The mapped asset name
 */
void GCP::mapAssetName(string& assetName)
{
	for (string::iterator it = assetName.begin(); it != assetName.end(); ++it)
	{
		if (*it == ' ')
		{
			*it = '_';
		}
	}
}
