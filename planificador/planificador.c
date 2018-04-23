#include <stdio.h>
#include <stdlib.h>
#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/string.h>

#include "../libs/conector.h"
#include "planificador.h"

#define MAXCONN 20

// Data Structures

typedef enum { SJFCD, SJFSD, HRRN } t_scheduling_algorithm;

typedef struct {
	int port;
	int coordinator_port;
	t_scheduling_algorithm scheduling_algorithm;
	double initial_estimation;
	char* coordinator_ip;
	char** blocked_keys;
} t_planificador_config;

// Local function prototypes

static char *_string_join(char **string_array, char *separator);
static bool algorithm_is_preemptive();
static esi_information* obtain_esi_information_by_id(int esi_fd);
static void take_esi_away_from_queue(t_list* queue, int esi_fd);
static void we_must_reschedule(int* flag);
static void remove_fd(int fd, fd_set *fdset);

// Global variables

t_config* config;

t_planificador_config setup;

t_log* logger;

t_list* g_locked_keys;
t_list* g_esi_bursts;

t_list* g_ready_queue;
t_list* g_execution_queue;
t_list* g_blocked_queue;
t_list* g_blocked_queue_by_console;
t_list* g_finished_queue;

int main(void) {

	init_log();

	init_config();

	char* host = setup.coordinator_ip;
	int port_coordinator = setup.coordinator_port;
	int server_port = setup.port;

	int coordinator_fd = connect_to_server(host, port_coordinator);
	log_info(logger, "Connecting to the coordinator...");

	if (send_handshake(coordinator_fd, SCHEDULER) != 1) {
		log_error(logger, "Failure in send_handshake");
		close(coordinator_fd);
		exit_gracefully(EXIT_FAILURE);
	}

	bool confirmation;
	int received = receive_confirmation(coordinator_fd, &confirmation);
	if (!received || !confirmation) {
		log_error(logger, "Failure in confirmation reception");
		close(coordinator_fd);
		exit_gracefully(EXIT_FAILURE);
	}

	log_info(logger, "Succesfully connected to the coordinator");

	int listener = init_listener(server_port, MAXCONN);
	log_info(logger, "Listening on port %i...", server_port);

	fd_set connected_fds;
	fd_set read_fds;

	FD_ZERO(&connected_fds);
	FD_ZERO(&read_fds);
	FD_SET(coordinator_fd, &connected_fds);
	FD_SET(listener, &connected_fds);

	int max_fd = (listener > coordinator_fd) ? listener : coordinator_fd;
	int flag;
	we_must_reschedule(&flag);

	while(1)
	{
		read_fds = connected_fds;

		if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
					log_error(logger, "Select error");
					exit(EXIT_FAILURE);
		}

		int fd;

		for (fd = 0; fd <= max_fd; fd++) {

					if (FD_ISSET(fd, &read_fds) == 0) {
						continue;

					} else if (fd == listener) {

						struct sockaddr_in client_info;
						socklen_t addrlen = sizeof client_info;
						log_info(logger, "New client connecting...");

						int new_client_fd = accept(listener, (struct sockaddr *) &client_info, &addrlen);

						if (new_client_fd == -1) {

							log_error(logger, "Accept error");
						} else {

							FD_SET(new_client_fd, &connected_fds);

							if (new_client_fd > max_fd) {

								max_fd = new_client_fd;
							}

							log_info(logger, "Socket %d connected", new_client_fd);

							bool client_confirmation = false;
							if (receive_handshake(new_client_fd) == -1) {

								send_confirmation(new_client_fd, confirmation);
								remove_fd(new_client_fd, &connected_fds);
								log_error(logger, "Handshake fail with socket %d", new_client_fd);
								close(new_client_fd);
							} else {

								client_confirmation = true;
								send_confirmation(new_client_fd, confirmation);
							}

							list_add(g_esi_bursts, create_esi_information(new_client_fd));
							put_new_esi_on_ready_queue(new_client_fd);

							if(algorithm_is_preemptive()) we_must_reschedule(&flag);
						}

					} else if(fd == coordinator_fd){

						/* TODO -- El coordinador se comunica con el planificador*/
						        //Si hicieron un GET el coordinador le debería decir qué clave fue bloqueada y debería agregar al esi en ejecución junto con esta clave a la lista key_blocker
								//si se pregunta por clave bloqueada, se contesta y nunca se replanifica
								//si se avisa el desbloqueo, se desbloquea y si es con desalojo flag de replanificar en 1
					} else if (fd == *(int*)g_execution_queue->head->data) {

						int confirmation = receive_execution_result(fd);
						if(confirmation /*==TODOLINDO */) {

							update_waiting_time_of_ready_esis();
                            update_executing_esi(fd);

						} else /*if(confirmation == BLOQUEADO)*/ {

							move_esi_from_and_to_queue(g_execution_queue, g_blocked_queue, fd);
							we_must_reschedule(&flag);
						}

					}
				}

        if(flag == 1) {
		int esi_fd_to_execute = schedule_esis();
		move_esi_from_and_to_queue(g_ready_queue, g_execution_queue, esi_fd_to_execute);
		//SETEAR en 0 last burst del esi a ejecutar
		flag = 0;
		authorize_esi_execution(esi_fd_to_execute);
        }
	}

	return EXIT_SUCCESS;
}

key_blocker* create_key_blocker(char* key, int esi_id){

    key_blocker* key_blocker = malloc(sizeof(key_blocker));
    key_blocker->key = key;
    key_blocker->esi_id = esi_id;
    return key_blocker;
}

esi_information* create_esi_information(int esi_id) {

	esi_information* esi_inf = malloc(sizeof(esi_information));
	esi_inf->esi_id = esi_id;
	esi_inf->next_left_estimated_burst = (double)setup.initial_estimation;
	esi_inf->last_estimated_burst = esi_inf->next_left_estimated_burst;
	esi_inf->last_real_burst = 0;
	esi_inf->waited_bursts = 0;
	return esi_inf;
}

void create_administrative_structures() {

	g_locked_keys = list_create();
	g_esi_bursts = list_create();
}

void destroy_administrative_structures() {
	void destroy_key_blocker(void* key_blocker_)
	{
		free(((key_blocker*)key_blocker_)->key);
		free(key_blocker_);
	}

	list_destroy_and_destroy_elements(g_locked_keys, destroy_key_blocker);

	void destroy_esi_information(void* esi_inf)
	{
	    free((esi_information*)esi_inf);
	}

	list_destroy_and_destroy_elements(g_esi_bursts, destroy_esi_information);
}

void init_config() {

	config = config_create("planificador.cfg");
	log_info(logger, "Se abrio el archivo de configuracion.");

	check_config("PUERTO");
	setup.port = config_get_int_value(config, "PUERTO");
	log_info(logger, "Asignando puerto %d.", setup.port);

	check_config("ALGORITMO_PLANIFICACION");
	char* algorithm_name = config_get_string_value(config, "ALGORITMO_PLANIFICACION");
	set_distribution(algorithm_name);
	log_info(logger, "Asignado algoritmo de reemplazo de planificacion %s.", algorithm_name);
	free(algorithm_name);

	check_config("ESTIMACION_INICIAL");
	setup.initial_estimation = config_get_double_value(config, "ESTIMACION_INICIAL");
	log_info(logger, "Asignando estimacion inicial %f.", setup.initial_estimation);

	check_config("IP_COORDINADOR");
	setup.coordinator_ip = config_get_string_value(config, "IP_COORDINADOR");
	log_info(logger, "Asignando direccion coordinador %s.", setup.coordinator_ip);

	check_config("PUERTO_COORDINADOR");
	setup.coordinator_port = config_get_int_value(config, "PUERTO_COORDINADOR");
	log_info(logger, "Asignando puerto coordinador %d.", setup.coordinator_port);

	check_config("CLAVES_BLOQUEADAS");
	setup.blocked_keys = config_get_array_value(config, "CLAVES_BLOQUEADAS");

	char *key_names_str = _string_join(setup.blocked_keys, ", ");
	log_info(logger, "Asignando claves inicialmente bloqueadas [%s].", key_names_str);
	free(key_names_str);

	log_info(logger, "Se configuro el planificador correctamente.");
}

static char *_string_join(char **string_array, char *separator) {
	char *str = string_new();
	int i;
	for (i = 0; string_array[i] != NULL; i++) {
		string_append(&str, string_array[i]);

		if (string_array[i + 1] != NULL) {
			string_append(&str, separator);
		} else {
			return str;
		}
	}

	return str;
}

void set_distribution(char* algorithm_name) {

	if(string_equals_ignore_case(algorithm_name, "SJFCD")) {
		setup.scheduling_algorithm = SJFCD;
	}
	else if(string_equals_ignore_case(algorithm_name, "SJFSD")) {
		setup.scheduling_algorithm = SJFSD;
	}
	else if(string_equals_ignore_case(algorithm_name, "HRRN")){
		setup.scheduling_algorithm = HRRN;
	}
	else {
		log_error(logger, "Se intento asignar un algoritmo inexistente llamado %s.", algorithm_name);
		exit_gracefully(EXIT_FAILURE);
	}
}
void init_log() {

	logger = log_create("planificador.log", "planificador", 1 , LOG_LEVEL_INFO);
	log_info(logger, "Logger created");
}

void check_config(char* key) {
	if(!config_has_property(config, key)) {
		log_error(logger, "No existe la clave '%s' en el archivo de configuracion.", key);

		exit_gracefully(EXIT_FAILURE);
	}
}

void put_new_esi_on_ready_queue(int new_client_fd) {

	list_add(g_ready_queue, &new_client_fd);
}

void authorize_esi_execution(int esi_fd) {

	int operation_id = execute_order;
	send(esi_fd, &operation_id, sizeof(operation_id), 0);
}

int receive_confirmation_from_esi(int fd) {

	int* buffer;
	if(recv(fd, buffer, sizeof(int), MSG_WAITALL) == -1) {

		log_error(logger, "Confirmation failed");
		//no sabes hablar, sock my port -- matar esi --no hay tiempo para vos sorete esi
	}
	return *buffer;
}

void update_executing_esi(int esi_fd) {

	esi_information* executing_esi = obtain_esi_information_by_id(esi_fd);

	executing_esi->last_real_burst++;
	executing_esi->next_left_estimated_burst--;
}

int receive_execution_result(int fd) { //TODO

	int result;
	return result;
}

void update_waiting_time_of_ready_esis() {

	bool esi_information_in_ready(void* esi_inf) {

		bool condition(void* esi_id_in_ready) {

			return *(int*)esi_id_in_ready == ((esi_information*)esi_inf)->esi_id;
		}

		return list_any_satisfy(g_ready_queue, condition);
	}

	t_list* esis_in_ready_queue = list_filter(g_esi_bursts, esi_information_in_ready);

	void update_waited_bursts(void* esi_inf) {

		((esi_information*)esi_inf)->waited_bursts++;
	}

	list_iterate(esis_in_ready_queue, update_waited_bursts);
}

void move_esi_from_and_to_queue(t_list* from_queue, t_list* to_queue, int esi_fd) {

	take_esi_away_from_queue(from_queue, esi_fd);
	list_add(to_queue, (void*)esi_fd);
}

void exit_gracefully(int status) {

	log_info(logger, "Scheduler execution ended");

	config_destroy(config);

	log_destroy(logger);

	destroy_administrative_structures();

	exit(status);
}

/* --- PRIVATE FUNCTIONS --- */

static bool algorithm_is_preemptive() {

	int algorithm_type = setup.scheduling_algorithm;
	switch (algorithm_type) {

	case 0:
		return true;
		break;
	default:
		return false;
		break;
	}
}

static esi_information* obtain_esi_information_by_id(int esi_fd){

	bool equal_condition(void* esi_inf) {

		return ((esi_information*)esi_inf)->esi_id == esi_fd;
	}

	return list_find(g_esi_bursts, equal_condition);
 }

static void take_esi_away_from_queue(t_list* queue, int esi_fd) {

	bool remove_condition(void* esi_to_delete) {

		return *(int*)esi_to_delete == esi_fd;
	}

	void destroy_esi_fd(void* esi_fd_) {

		free((int*)esi_fd_);
	}

	list_remove_and_destroy_by_condition(queue, remove_condition, destroy_esi_fd);
}

static void we_must_reschedule(int* flag) {

	*flag = 1;
}

static void remove_fd(int fd, fd_set *fdset) {
	FD_CLR(fd, fdset);
	log_info(logger, "Socket %d kicked out", fd);
	close(fd);
}
