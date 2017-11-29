#define AFB_BINDING_VERSION 2
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include <afb/afb-binding.h>
#include "MQTTAsync.h"
#define TOPIC "MESSAGE RECEIVED"

MQTTAsync client;
MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
afb_event message_arrived_event;
volatile MQTTAsync_token deliveredtoken;
volatile char* payloadptr;

int finished = 0;
/*-------------------Callback functions---------------------*/
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    AFB_NOTICE("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

void connlost(void *context, char *cause)
{
     MQTTAsync client = (MQTTAsync)context;
     MQTTAsync_connectOptions conn_opts = MQTTAsync_connectOptions_initializer;
     int rc;
     AFB_NOTICE("\nConnection lost\n");
     AFB_NOTICE("     cause: %s\n", cause);
     AFB_NOTICE("Reconnecting\n");
     conn_opts.keepAliveInterval = 20;
     conn_opts.cleansession = 1;
     if ((rc = MQTTAsync_connect(client, &conn_opts)) != MQTTASYNC_SUCCESS)
     {
             AFB_NOTICE("Failed to start connect, return code %d\n", rc);
             finished = 1;
    }
    //AFB_NOTICE("\nConnection lost\n");
    //AFB_NOTICE("   cause: %s\n", cause);
}

void onDisconnect(void* context, MQTTAsync_successData* response)
{
        AFB_NOTICE("Successful disconnection\n");
        finished = 1;
}

void onSend(void* context, MQTTAsync_successData* response)
{
        MQTTAsync client = (MQTTAsync)context;
        MQTTAsync_disconnectOptions opts = MQTTAsync_disconnectOptions_initializer;
        int rc;
        AFB_NOTICE("Message with token value %d delivery confirmed\n", response->token);
        opts.onSuccess = onDisconnect;
        opts.context = client;
        if ((rc = MQTTAsync_disconnect(client, &opts)) != MQTTASYNC_SUCCESS)
        {
                printf("Failed to start sendMessage, return code %d\n", rc);
                exit(EXIT_FAILURE);
        }
}

void onConnectFailure(void* context, MQTTAsync_failureData* response)
{
        AFB_NOTICE"Connect failed, rc %d\n", response ? response->code : 0);
        finished = 1;
}

void onConnect(void* context, MQTTAsync_successData* response)
{
        MQTTAsync client = (MQTTAsync)context;
        MQTTAsync_responseOptions opts = MQTTAsync_responseOptions_initializer;
        MQTTAsync_message pubmsg = MQTTAsync_message_initializer;
        int rc;
        AFB_NOTICE("Successful connection\n");
        opts.onSuccess = onSend;
        opts.context = client;
        pubmsg.payload = PAYLOAD;
        pubmsg.payloadlen = strlen(PAYLOAD);
        pubmsg.qos = QOS;
        pubmsg.retained = 0;
        deliveredtoken = 0;
        if ((rc = MQTTAsync_sendMessage(client, TOPIC, &pubmsg, &opts)) != MQTTASYNC_SUCCESS)
        {
                printf("Failed to start sendMessage, return code %d\n", rc);
                exit(EXIT_FAILURE);
        }
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    
    int rc;
    json_object *msg = json_object_new_object();
    AFB_NOTICE("Message arrived");
    AFB_NOTICE("  topic: %s", topicName);
    AFB_NOTICE("  message: ");
    AFB_NOTICE("Copying message to localpointer");
    AFB_NOTICE("payloadlength = %d", message->payloadlen);
    payloadptr = (char*)malloc(message->payloadlen);
    snprintf(payloadptr, message->payloadlen+1, "%s", ((char *)message->payload));
    AFB_NOTICE("Printing message: ");
    AFB_NOTICE(payloadptr);
    json_object_object_add(msg, "topic", json_object_new_string(topicName));
    json_object_object_add(msg, "message", json_object_new_string(payloadptr));
    rc = afb_event_push(message_arrived_event, msg);
    if(rc == 0) {
       AFB_NOTICE("No client subscribed !!");
    } 
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

/*-----------Create client with necessary data---------------*/

void initMQTT (afb_req req){
	int rc;
	json_object * jobj = afb_req_json(req);
	json_object * serverJSON, *clientidJSON;
	const * serverURI;
	char *clientID;
	// Search for serverURI value
	json_bool success = json_object_object_get_ex(jobj, "serverURI", &serverJSON);
	if(!success){
		afb_req_fail_f(req, "ERROR", "server URI not found in '%s'", json_object_get_string(jobj));	
	}
	// Search for client_ID value
	success = json_object_object_get_ex(jobj, "client_id", &clientidJSON);
	if(!success){
		afb_req_fail_f(req, "ERROR", "clientID not found in '%s'", json_object_get_string(jobj));	
	}
	// Validate serverURI value
	if(json_object_get_type(serverJSON) != json_type_string){
		afb_req_fail(req, "ERROR", "serverURI is not a string value");
	}
	// Validate client_ID value
	if(json_object_get_type(clientidJSON) != json_type_string){
		afb_req_fail(req, "ERROR", "keepAlive is not a string value");
	}
	clientID = strdup(json_object_get_string(clientidJSON));
	AFB_NOTICE(clientID);
	serverURI = strdup(json_object_get_string(serverJSON));
	AFB_NOTICE(serverURI);
	// Create client with default persistence
	MQTTAsync_create(&client, ADDRESS, clientID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
	MQTTAsync_setCallbacks(client, NULL, connlost, msgarrvd, delivered);	
	
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

#ifdef DEBUG
   // Trace that shows the code flow
   setenv("MQTT_C_CLIENT_TRACE", "ON", 1);
   setenv("MQTT_C_CLIENT_TRACE_LEVEL", "DEBUG", 1);
#endif
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
	// set connection options
	conn_opts.connectTimeout = connectTimeout;
	conn_opts.MQTTVersion =  MQTTVERSION_DEFAULT;
	conn_opts.will = NULL;
	conn_opts.reliable = 0;
	conn_opts.ssl = NULL;
	conn_opts.onSuccess = onConnect;
        conn_opts.onFailure = onConnectFailure;
        conn_opts.context = client;	
	

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
	// Creating a single afb_event for all MQTT events. The various MQTT events can be differentiated from the data fiel of the message recieved.	
	message_arrived_event = afb_daemon_make_event(TOPIC);	
	if(!afb_event_is_valid(message_arrived_event)){
		AFB_NOTICE("Event is not valid");
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
	
	// put all values
	pubmsg.payload = payload;
	AFB_NOTICE(payload);
	pubmsg.payloadlen = strlen(payload);
	pubmsg.qos = qos;
	pubmsg.retained = retained;
	// Publish message
	MQTTClient_publishMessage(client, topic, &pubmsg, &token);
	rc = MQTTClient_waitForCompletion(client, token, conn_opts.connectTimeout);

	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success_f(req, NULL, "Payload '%s' published", payload);
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
}

/*-------------Subscribe messages from a particular topic-----------------*/
/*void subscribe(afb_req req){

	// TODO: Replace subscribe with subscribe many later.
	const char * topic;
	int rc, afb_res;
	int qos = 0;
	topic = afb_req_value(req, "topic");
	qos = atoi(afb_req_value(req, "qos"));
	AFB_NOTICE("topic is %s, qos is %d", topic, qos);
	// Subscribe to a topic
	rc = MQTTClient_subscribe(client, topic, qos);

	afb_res = afb_req_subscribe(req, message_arrived_event);
	if(afb_res != 0){
		AFB_NOTICE("Event subscription failed");
	}	
	
	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success_f(req, NULL, "Subscribed to topic '%s' successfully", topic);
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
}*/
/*------------Unsubscribe messages from a particular topic----------------*/
/*void unsubscribe(afb_req req){
	//TODO: Replace subscribe with subscribe many later.
	const char * topic = afb_req_value(req, "topic");
	int rc = MQTTClient_unsubscribe(client, topic);
	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success_f(req, NULL, "Unsubscribed to topic '%s' successfully", topic);
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
}*/
/*--------------Disconnect the client from the server------------------*/
/*void disconnectServer(afb_req req){
	int rc;
	// Disconnect from the server.
	rc = MQTTClient_disconnect(client, conn_opts.connectTimeout);
	// Destroy client object
	MQTTClient_destroy(&client);
	if(rc == MQTTCLIENT_SUCCESS){
		afb_req_success(req, NULL, "success");
	}
	else{
		afb_req_fail_f(req, NULL, "Failed with returned code: '%d'", rc);
	}
	rc = afb_req_unsubscribe(req, message_arrived_event);
	if(rc != 0){
		AFB_NOTICE("unsubscribe daemon failed");
	}
}*/


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
	/*{.verb = "disconnect", .session = AFB_SESSION_NONE, .callback =disconnectServer, .auth = NULL},*/
	{.verb = "pub", .session = AFB_SESSION_NONE, .callback =publish, .auth = NULL}
	/*{.verb = "sub", .session = AFB_SESSION_NONE, .callback =subscribe, .auth = NULL},
	{.verb = "unsub", .session = AFB_SESSION_NONE, .callback =unsubscribe, .auth = NULL}*/		
};

const struct afb_binding_v2 afbBindingV2 = {
	.api = "pahoc",
	.verbs = verbs,
	.init = init,
	.preinit = preinit,
	.onevent = NULL,
	.noconcurrency = 0
};
