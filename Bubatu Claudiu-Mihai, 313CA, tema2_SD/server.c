/* Copyright 2021 <> */
#include <stdlib.h>
#include <string.h>
extern char* strdup(const char*);

#include "LinkedList.c"
#include "LinkedList.h"
#include "server.h"

#define H 10000

unsigned int hash_function_key(void *a) {
    unsigned char *puchar_a = (unsigned char *) a;
    unsigned int hash = 5381;
    int c;

    while ((c = *puchar_a++))
        hash = ((hash << 5u) + hash) + c;

    return hash;
}

server_memory *init_server_memory() {
	server_memory *server = malloc(sizeof(server_memory));
	server->hmax = H;
	server->size = 0;
	server->buckets = malloc((server->hmax * sizeof(linked_list_t)));
	for (int i = 0; i < server->hmax; i++) {
		server->buckets[i] = ll_create(sizeof(struct data));
	}
	return server;
}

void server_store(server_memory* server, char* key, char* value) {
	// iau index-ul bucket-ului pe care vreau sa adaug produsul
	// si stochez produsul acolo

	int index = hash_function_key(key) % server->hmax;
	ll_node_t *node = server->buckets[index]->head;
	while (node != NULL) {
		if (strcmp(((struct data *)node->data)->key, key) == 0) {
			memcpy(((struct data *)node->data)->value, value, strlen(value));
			return;
		}
		node = node->next;
	}
	struct data new_bucket;
	new_bucket.key = strdup(key);
	new_bucket.value = strdup(value);
	ll_add_nth_node(server->buckets[index],
				server->buckets[index]->size, &new_bucket);
	server->size++;
}

void server_remove(server_memory* server, char* key) {
	// iau index-ul bucket-ului de unde vreau sa sterg produsul
	// si eliberez memoria produsului de la bucket-ul de pe
	// pozitia index
	int index = hash_function_key(key) % server->hmax;
	ll_node_t *node = server->buckets[index]->head;
	unsigned int count = 0;
	while (node != NULL) {
		if (strcmp(((struct data *)node->data)->key, key) == 0) {
			node = ll_remove_nth_node(server->buckets[index], count);
			free(((struct data *)node->data)->key);
			free(((struct data *)node->data)->value);
			free(node->data);
			free(node);
			server->size--;
			break;
		}
		count++;
		node = node->next;
	}
}

char* server_retrieve(server_memory* server, char* key) {
	int index = hash_function_key(key) % server->hmax;
	// returnează valoarea asociată lui key din hashtable
	ll_node_t *node = server->buckets[index]->head;
	while (node != NULL) {
		if (strcmp(((struct data *)node->data)->key, key) == 0) {
			return (((struct data *)node->data)->value);
		}
		node = node->next;
	}
	return NULL;
}

void free_server_memory(server_memory* server) {
	// eliberez memoria fiecarui produs si lista de buckets-uri
	// urmand a elibera si server-ul
	for (int i = 0; i < server->hmax; i++) {
		ll_node_t *node = server->buckets[i]->head;
		while (node != NULL) {
			free(((struct data *)node->data)->key);
			free(((struct data *)node->data)->value);
			node = node->next;
		}
		ll_free(&server->buckets[i]);
	}
	free(server->buckets);
	free(server);
}
