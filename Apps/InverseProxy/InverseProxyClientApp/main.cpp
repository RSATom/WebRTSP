#include <deque>

#include <glib.h>

#include <libwebsockets.h>

#include "CxxPtr/libconfigDestroy.h"

#include "Common/ConfigHelpers.h"
#include "../InverseProxyClient/Log.h"
#include "../InverseProxyClient/InverseProxyClient.h"


static const auto Log = InverseProxyClientLog;


static bool LoadConfig(InverseProxyClientConfig* config)
{
    const std::deque<std::string> configDirs = ::ConfigDirs();
    if(configDirs.empty())
        return false;

    InverseProxyClientConfig loadedConfig {};

    bool someConfigFound = false;
    for(const std::string& configDir: configDirs) {
        const std::string configFile = configDir + "/webrtsp-client.conf";
        if(!g_file_test(configFile.c_str(),  G_FILE_TEST_IS_REGULAR)) {
            Log()->info("Config \"{}\" not found", configFile);
            continue;
        }

        someConfigFound = true;

        config_t config;
        config_init(&config);
        ConfigDestroy ConfigDestroy(&config);

        Log()->info("Loading config \"{}\"", configFile);
        if(!config_read_file(&config, configFile.c_str())) {
            Log()->error("Fail load config. {}. {}:{}",
                config_error_text(&config),
                configFile,
                config_error_line(&config));
            return false;
        }

        config_setting_t* targetConfig = config_lookup(&config, "target");
        if(targetConfig && CONFIG_TRUE == config_setting_is_group(targetConfig)) {
            const char* host = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(targetConfig, "host", &host)) {
                loadedConfig.clientConfig.server = host;
            }
            int port = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(targetConfig, "port", &port)) {
                loadedConfig.clientConfig.serverPort = static_cast<unsigned short>(port);
            }
            int timeout = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(targetConfig, "reconnect-timeout", &timeout)) {
                loadedConfig.reconnectTimeout = static_cast<unsigned>(timeout);
            }
        }
        config_setting_t* authConfig = config_lookup(&config, "auth");
        if(authConfig && CONFIG_TRUE == config_setting_is_group(authConfig)) {
            const char* name = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(authConfig, "name", &name)) {
                loadedConfig.name = name;
            }
            const char* authToken = nullptr;
            if(CONFIG_TRUE == config_setting_lookup_string(authConfig, "token", &authToken)) {
                loadedConfig.authToken = authToken;
            }
        }
        config_setting_t* debugConfig = config_lookup(&config, "debug");
        if(debugConfig && CONFIG_TRUE == config_setting_is_group(debugConfig)) {
            int lwsLogLevel = 0;
            if(CONFIG_TRUE == config_setting_lookup_int(debugConfig, "lws-log-level", &lwsLogLevel)) {
                if(lwsLogLevel > 0)
                    lws_set_log_level(lwsLogLevel, nullptr);
            }
        }
    }

    if(!someConfigFound)
        return false;

    bool success = true;

    if(loadedConfig.clientConfig.server.empty()) {
        Log()->error("Missing target host");
        success = false;
    }
    if(!loadedConfig.clientConfig.serverPort) {
        Log()->error("Missing target port");
        success = false;
    }
    if(loadedConfig.name.empty()) {
        Log()->error("Missing auth name");
        success = false;
    }
    if(loadedConfig.authToken.empty()) {
        Log()->error("Missing auth token");
        success = false;
    }

    if(success)
        *config = loadedConfig;

    return success;
}

int main(int argc, char *argv[])
{
    InverseProxyClientConfig config;
    if(!LoadConfig(&config))
        return -1;

    return InverseProxyClientMain(config);
}