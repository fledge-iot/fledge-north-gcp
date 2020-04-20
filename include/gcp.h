#ifndef _GCP_H
#define _GCP_H
#include <reading.h>
#include <config_category.h>
#include <logger.h>
#include <string>
#include "MQTTClient.h"
#include <jwt.h>
#include <set>

class GCP {
	public:
		GCP();
		~GCP();
		void		configure(const ConfigCategory *conf);
		uint32_t	send(const std::vector<Reading *>& readings);
		void		msgArrived(char *topic, MQTTClient_message *msg);
		void		lostConnection(const char *reason);
		void		delivered(MQTTClient_deliveryToken dt);
		int		connect();
	private:
		int		publish(char *payload, const int payload_size);
		int		publish(const std::string& topic, char *payload, const int payload_size);
		void		mapAssetName(std::string& name);
		void		disconnect();
		void		createSubscriptions();
		std::string	makePayload(Reading *reading);
		void		createJWT();
		void		getIatExp(char* iat, char* exp, int time_size);
		jwt_alg_t	getAlgorithm();
		std::string	getRootPath();
		std::string	getKeyPath();
		MQTTClient	m_client;
		static const std::string
				m_address;
		std::string	m_projectID;
		std::string	m_region;
		std::string	m_registryID;
		std::string	m_deviceID;
		std::string	m_clientID;
		std::string	m_topic;
		std::string	m_algorithm;
		std::string	m_key;
		std::string	m_keyPath;
		std::string	m_rootPath;
		std::string	m_authToken;
		char		*m_jwtStr;
		time_t		m_jwtExpire;
		Logger		*m_log;
		bool		m_subscribed;
		bool		m_connected;
		std::set<std::string>
				m_asset;
		int		m_lastSent;
		int		m_lastDelivered;
};

#endif
