#ifndef MOD_EVENT_REDIS_H
#define MOD_EVENT_REDIS_H


namespace mod_event_redis {

	static struct {
		char *hostname;
		int port;
		char *sentinals;
		char *topic_name;
	} globals;

	SWITCH_MODULE_LOAD_FUNCTION(mod_event_redis_load);
	SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_redis_shutdown);

	extern "C" {
		SWITCH_MODULE_DEFINITION(mod_event_redis, mod_event_redis_load, mod_event_redis_shutdown, NULL);
	};
};

#endif // MOD_EVENT_REDIS_H