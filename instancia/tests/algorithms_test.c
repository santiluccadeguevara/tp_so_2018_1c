#include <stdlib.h>
#include <stdio.h>

#include "../entry_table.h"
#include "../globals.h"
#include "./utils.h"
#include "../algorithms.h"

int main(void)
{

	storage_setup_init(10,20);
			entry_table_init();
			key_value_t* key_value=key_value_generator("X",30);
			int next_entry = entry_table_next_entry(key_value);
			entry_table_insert(next_entry,key_value);

			key_value=key_value_generator("Y",30);
			next_entry = entry_table_next_entry(key_value);
			entry_table_insert(next_entry,key_value);

			key_value=key_value_generator("Z",15);
			next_entry = entry_table_next_entry(key_value);
			entry_table_insert(next_entry,key_value);



			key_value=key_value_generator("Y",30);
			entry_table_delete(key_value);



			key_value=key_value_generator("Y",30);
			next_entry = entry_table_next_entry(key_value);
			entry_table_insert(next_entry,key_value);


			key_value=key_value_generator("X",30);
			entry_table_delete(key_value);



			key_value=key_value_generator("X",15);
			next_entry = entry_table_next_entry(key_value);
			entry_table_insert(next_entry,key_value);


	t_list* entry_table_status = original_entry_table_migration_to_complete_one();

	entry_table_status_print_table(entry_table_status);


}
