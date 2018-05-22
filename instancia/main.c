#include "../libs/logger.h"
#include "instancia.h"
#include "cfg_instancia.h"
#include "coordinator_api.h"

#define PROCESS	"Instancia"
#define LOGGER	"instancia.log"
#define CFG		"instancia.cfg"
#define IP		cfg_instancia_get_coordinador_ip()
#define HOST	cfg_instancia_get_coordinador_port()
#define NAME	cfg_instancia_get_instance_name()

int main(int argc, char* argv[]) {
	logger_init(LOGGER, PROCESS, (argc == 1) ? "INFO" : argv[1]);

	cfg_instancia_init(CFG);

	coordinator_api_connect(IP, HOST);

	coordinator_api_handshake(NAME);

	for(;;) {

		int status;

		switch(coordinator_api_receive_header()) {
			case PROTOCOL_CI_SET: {
				key_value_t* key_value = coordinator_api_receive_set();

				instance_set(key_value);

				key_value_destroy(key_value);

				status = 1;

				break;
			}

			default: {
				status = 0;

				break;
			}
		}

		coordinator_api_notify_status(status);
	}
}