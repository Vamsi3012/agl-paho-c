#define AFB_BINDING_VERSION 2
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <afb/afb-binding.h>
#include "MQTTClient.h"
#define CLIENTID "AGL-PAHO-C"

MQTTClient client;
MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

volatile MQTTClient_deliveryToken deliveredtoken;


void delivered(void *context, MQTTClient_deliveryToken dt)
{
    AFB_NOTICE("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

void connlost(void *context, char *cause)
{
    AFB_NOTICE("\nConnection lost\n");
    AFB_NOTICE("   cause: %s\n", cause);
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;
    AFB_NOTICE("Message arrived\n");
    AFB_NOTICE("  topic: %s\n", topicName);
    AFB_NOTICE("  message: ");
    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    AFB_NOTICE('\n');
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

/*-----------Create client with necessary data---------------*/

void initMQTT (afb_req req){
	json_object * jobj = afb_req_json(req);
	json_object * serverJSON;
	const char * serverURI;
	char * serverURI_b;
	// Search for serverURI value
	json_bool success = json_object_object_get_ex(jobj, "serverURI", &serverJSON);
	if(!success){
		afb_req_fail_f(req, "ERROR", "server URI not found in '%s'", json_object_get_string(jobj));	
	}
	// Validate serverURI value
	if(json_object_get_type(serverJSON) != json_type_string){
		afb_req_fail(req, "ERROR", "keepAlive is not an integer value");
	}
	serverURI = json_object_get_string(serverJSON);
	AFB_NOTICE(serverURI);
	serverURI_b = strdup(json_object_get_string(serverJSON));
	AFB_NOTICE(serverURI_b);

	MQTTClient_create(&client, serverURI_b, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTClient_setCallbacks(client, NULL, connlost, NULL, delivered);	
	
	afb_req_success_f(req, NULL, "Client created with serverURI : '%s'", serverURI);	
}

/*-----------------Connect Client to Server------------------*/
void connectServer(afb_req req){
	json_object * jobj = afb_req_json(req);
	json_object *keepAliveJSON, *connectTimeoutJSON, *usernameJSON, *passwdJSON;
	char *username, *password;
	int keepAlive =100;
	int connectTimeout = 100;
	int rc;


	//setenv("MQTT_C_CLIENT_TRACE", "ON", 1);
	//setenv("MQTT_C_CLIENT_TRACE_LEVEL", "DEBUG", 1);

  //Search for various values................
	
	// Search for keepAlive value
	json_bool success = json_object_object_get_ex(jobj, "keepAlive", &keepAliveJSON);
	if(!success){
		afb_req_fail_f(req, "ERROR", "keepAlive not found in '%s'", json_object_get_string(jobj));	
	}

	// Search for connectTimeout value
	success = json_object_object_get_ex(jobj, "connectTimeout", &connectTimeoutJSON);
	if(!success){
		afb_req_fail_f(req, "ERROR", "connectTimeout not found in '%s'", json_object_get_string(jobj));	
	}

	// Search for username value
	success = json_object_object_get_ex(jobj, "username", &usernameJSON);
	if(!success){
		afb_req_fail_f(req, "ERROR", "username not found in '%s'", json_object_get_string(jobj));	
	}

	// Search for passwpord value
	success = json_object_object_get_ex(jobj, "password", &passwdJSON);
	if(!success){
		afb_req_fail_f(req, "ERROR", "password not found in '%s'", json_object_get_string(jobj));	
	}

  //Validate all valuse obtained......................

	// Validate keepAlive value
	if(json_object_get_type(keepAliveJSON) != json_type_int){
		afb_req_fail(req, "ERROR", "keepAlive is not an integer value");
	}

	// Validate connectTimeout value
	if(json_object_get_type(connectTimeoutJSON) != json_type_int){
		afb_req_fail(req, "ERROR", "connectionTimeout is not an integer value");
	}

	// Validate username value
	if(json_object_get_type(usernameJSON) != json_type_string){
		afb_req_fail(req, "ERROR", "username is not a string value");
	}

	// Validate password value
	if(json_object_get_type(passwdJSON) != json_type_string){
		afb_req_fail(req, "ERROR", "password is not a string value");
	}

	username = strdup(json_object_get_string(usernameJSON));
	password = strdup(json_object_get_string(passwdJSON));
	//set connection options
	AFB_NOTICE("Assigning keepAlive");
	keepAlive = json_object_get_int(keepAliveJSON);
	conn_opts.keepAliveInterval = keepAlive;
	AFB_NOTICE("Assigning cleansession");
	conn_opts.cleansession = 1;
	AFB_NOTICE("Assigning username");
	conn_opts.username = username;
	AFB_NOTICE("Assigning password");
	conn_opts.password = password;
	AFB_NOTICE("Assigning connectTimeout");
	connectTimeout = json_object_get_int(connectTimeoutJSON);
	conn_opts.connectTimeout = connectTimeout;
	//AFB_NOTICE("Assigning MQTT version");
	conn_opts.MQTTVersion =  MQTTVERSION_DEFAULT;
	// conn_opts.reliable =  1;
	//conn_opts.serverURIcount++;
	conn_opts.will = NULL;
	conn_opts.reliable = 0;
	conn_opts.ssl = NULL;
	

	//connect client to server
	AFB_NOTICE("Assigning connecting to server");
	rc = MQTTClient_connect(client, &conn_opts);

	//Check connection status
	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success(req, NULL, "Client has sucessfully connected to the server");	
	}
	else{
		afb_req_fail_f(req, "ERROR", "failed with error '%d'", rc);
	}

}

/*-------------Publish a message for a topic-----------------*/
void publish(afb_req req){
	// Initialize an default message.
	MQTTClient_message pubmsg = MQTTClient_message_initializer;
	MQTTClient_deliveryToken token;

	int qos = 0;
	int retained = 0;
	int rc;
	
	// get all values
	const char * topic = afb_req_value(req, "topic");
	const char * payload = afb_req_value(req, "payload");
	const char * qos_char = afb_req_value(req, "qos");
	const char * reatined_char = afb_req_value(req, "retained");
	qos = atoi(qos_char);
	retained = atoi(reatined_char);
	
	// put all valuse
	pubmsg.payload = payload;
	pubmsg.payloadlen = strlen(payload);
	pubmsg.qos = qos;
	pubmsg.retained = retained;
	MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
	MQTTClient_publishMessage(client, topic, &pubmsg, &token);
	rc = MQTTClient_waitForCompletion(client, token, conn_opts.connectTimeout);

	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success_f(req, NULL, "Payload '%s' published", payload);
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
}

void subscribe(afb_req req){

	// TODO: Replace subscribe with subscribe many later.
	const char * topic;
	int rc = 0;
	int qos = 0;
	topic = afb_req_value(req, "topic");
	qos = atoi(afb_req_value(req, "qos"));
	AFB_NOTICE("topic is %s, qos is %d", topic, qos);
	MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
	rc = MQTTClient_subscribe(client, topic, qos);
	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success_f(req, NULL, "Subscribed to topic '%s' successfully", topic);
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
}

void unsubscribe(afb_req req){
	//TODO: Replace subscribe with subscribe many later.
	const char * topic = afb_req_value(req, "topic");
	int rc = MQTTClient_unsubscribe(client, topic);
	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success_f(req, NULL, "Unsubscribed to topic '%s' successfully", topic);
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
}

void disconnectServer(afb_req req){
	int rc;
	rc = MQTTClient_disconnect(client, conn_opts.connectTimeout);
	MQTTClient_destroy(&client);
	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success(req, NULL, "success");
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
}


/*---------------paho.mqtt.c binding for AGL-----------------*/

void init() {
	AFB_NOTICE("agl-pahoc is starting....init");
}

void preinit() {
	AFB_NOTICE("agl-pahoc is starting....preinit");
}

static const struct afb_verb_v2 verbs[] = {
	{.verb = "initMQTT", .session = AFB_SESSION_NONE, .callback =initMQTT, .auth = NULL},
	{.verb = "connect", .session = AFB_SESSION_NONE, .callback =connectServer, .auth = NULL},
	{.verb = "disconnect", .session = AFB_SESSION_NONE, .callback =disconnectServer, .auth = NULL},
	{.verb = "pub", .session = AFB_SESSION_NONE, .callback =publish, .auth = NULL},
	{.verb = "sub", .session = AFB_SESSION_NONE, .callback =subscribe, .auth = NULL},
	{.verb = "unsub", .session = AFB_SESSION_NONE, .callback =unsubscribe, .auth = NULL}		
};

const struct afb_binding_v2 afbBindingV2 = {
	.api = "pahoc",
	.verbs = verbs,
	.init = init,
	.preinit = preinit,
	.onevent = NULL,
	.noconcurrency = 0
};
