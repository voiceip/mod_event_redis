/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Based on mod_skel by
 * Anthony Minessale II <anthm@freeswitch.org>
 *
 * Contributor(s):
 * 
 * Kinshuk Bairagi <me@kinshuk.in>
 *
 * mod_event_redis.c -- Sends FreeSWITCH events to an Redis Queue
 *
 */

#include <iostream>
#include <string>
#include <thread>
#include <switch.h>
#include <sstream>

#include <cpp_redis/cpp_redis>
#include "mod_event_redis.hpp"

namespace mod_event_redis {

    template<class T>
    std::string toString(const T &value) {
        std::ostringstream os;
        os << value;
        return os.str();
    }

    static switch_xml_config_item_t instructions[] = {
        SWITCH_CONFIG_ITEM("hostname", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &globals.hostname,
                            "localhost", NULL, "hostname", "Redis Master hostname"),
        SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &globals.port,
                            6379, NULL, "hosts", "Redis Port"),
        SWITCH_CONFIG_ITEM("sentinals", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &globals.sentinals,
                            "localhost:xxxx", NULL, "hostname", "Redis Sentinals"),
        SWITCH_CONFIG_ITEM("topic-name", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &globals.topic_name,
                            "fs", NULL, "topic-name", "Topic Name"),
        SWITCH_CONFIG_ITEM_END()
    };

    static switch_status_t load_config(switch_bool_t reload)
    {
        memset(&globals, 0, sizeof(globals));
        if (switch_xml_config_parse_module_settings("event_redis.conf", reload, instructions) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Could not open event_redis.conf\n");
            return SWITCH_STATUS_FALSE;
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "event_redis.conf loaded [hostname: %s, port : %d, topic: %s]  \n", globals.hostname, globals.port , globals.topic_name);
        }
        return SWITCH_STATUS_SUCCESS;
    }

    class cpp_redis_fs_logger :  public cpp_redis::logger_iface {

        public:
        void debug(const std::string& msg, const std::string& file,  std::size_t line) { 
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s:%zu] %s", file.c_str(), line, msg.c_str());
        };
        
        void info(const std::string& msg, const std::string& file,  std::size_t line) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[%s:%zu] %s", file.c_str(), line, msg.c_str());
        };
        
        void warn(const std::string& msg, const std::string& file,  std::size_t line){
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[%s:%zu] %s", file.c_str(), line, msg.c_str());
        };

        void error(const std::string& msg, const std::string& file,  std::size_t line) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[%s:%zu] %s", file.c_str(), line, msg.c_str() );
        };

    };

    class RedisEventPublisher {

        
        public:
        RedisEventPublisher(){

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "RedisEventPublisher Initialising... \n");

            load_config(SWITCH_FALSE);

            cpp_redis::active_logger = std::unique_ptr<cpp_redis_fs_logger>(new cpp_redis_fs_logger);

            try {
                redisClient.connect(globals.hostname, globals.port, [this](const std::string& host, std::size_t port, cpp_redis::client::connect_state status) {
                    if (status == cpp_redis::client::connect_state::ok) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "client connection ok to %s:%zu  \n", host.c_str(), port);
                        _initialized = 1;
                    } else if (status == cpp_redis::client::connect_state::dropped) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "client disconnected from %s:%zu  \n", host.c_str(), port);
                        _initialized = 0;
                    }
                }, 0, -1, 5000);
                _initialized = 1;
            } catch (...) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Redis Connection Initial Connect Failed  \n");
            }

        }

        void PublishEvent(switch_event_t *event) {

            char *event_json = (char*)malloc(sizeof(char));
            switch_event_serialize_json(event, &event_json);
            std::string event_json_str(event_json);

            if(_initialized){
                send(event_json_str);
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "PublishEvent without active RedisConnection \n");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s\n", event_json );
            }
        }

        void Shutdown(){
            //flush within 1000ms
            redisClient.sync_commit(std::chrono::milliseconds(1000));
        }

        ~RedisEventPublisher(){
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "RedisEventPublisher Destroyed \n");
        }

        private:

        std::vector<std::string> split(const std::string& s, char delimiter)
        {
           std::vector<std::string> tokens;
           std::string token;
           std::istringstream tokenStream(s);
           while (std::getline(tokenStream, token, delimiter)) {
              tokens.push_back(token);
           }
           return tokens;
        }

        int send(const std::string data){

            std::vector<std::string> lpushData = {data};
            size_t len = data.size();

            redisClient.lpush(globals.topic_name, lpushData , [len](cpp_redis::reply& reply) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"Published messaged (%zu bytes), redis queue size (%" PRId64 ") messages.  \n", len ,reply.as_integer());
                // if (reply.is_string())
                //   do_something_with_string(reply.as_string())
            });
            redisClient.commit();

            return 0;    
        }   

        bool _initialized = 0;
        cpp_redis::client redisClient;
    };

    class RedisEventModule {
    
        public:
        RedisEventModule(switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool): _publisher() {
             
            // Subscribe to all switch events of any subclass
            // Store a pointer to ourself in the user data
            if (switch_event_bind_removable(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler,
                                            static_cast<void*>(&_publisher), &_node)
                != SWITCH_STATUS_SUCCESS) {
                throw std::runtime_error("Couldn't bind to switch events.");
            }
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Subscribed to events\n");

            // Create our module interface registration
            *module_interface = switch_loadable_module_create_module_interface(pool, modname);

            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module loaded completed\n");

        };

        void Shutdown() {
            // Send term message
            _publisher.Shutdown();
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Shutdown requested, flushing publisher\n");
        }

        ~RedisEventModule() {
            // Unsubscribe from the switch events
            switch_event_unbind(&_node);
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Module shut down\n");
        }

        private:

        // Dispatches events to the publisher
        static void event_handler(switch_event_t *event) {
            try {
                RedisEventPublisher *publisher = static_cast<RedisEventPublisher*>(event->bind_user_data);
                publisher->PublishEvent(event);
            } catch (std::exception ex) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Error publishing event via Redis: %s\n",
                                  ex.what());
            } catch (...) { // Exceptions must not propogate to C caller
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unknown error publishing event via Redis\n");
            }
        }

        switch_event_node_t *_node;
        RedisEventPublisher _publisher;

    };


    //*****************************//
    //           GLOBALS           //
    //*****************************//
    std::auto_ptr<RedisEventModule> module;


    //*****************************//
    //  Module interface funtions  //
    //*****************************//
    SWITCH_MODULE_LOAD_FUNCTION(mod_event_redis_load) {
            try {
                module.reset(new RedisEventModule(module_interface, pool));
                return SWITCH_STATUS_SUCCESS;
            } catch(...) { // Exceptions must not propogate to C caller
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading Redis Event module\n");
                return SWITCH_STATUS_GENERR;
            }
    }

    SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_redis_shutdown) {
            try {
                // Tell the module to shutdown
                module->Shutdown();
                // Free the module object
                module.reset();
            } catch(std::exception &ex) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error shutting down Redis Event module: %s\n",
                                  ex.what());
            } catch(...) { // Exceptions must not propogate to C caller
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
                                  "Unknown error shutting down Redis Event module\n");
            }
            return SWITCH_STATUS_SUCCESS;
    }

}
