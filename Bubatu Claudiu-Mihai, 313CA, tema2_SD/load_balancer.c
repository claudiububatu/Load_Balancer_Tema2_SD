/* Copyright 2021 <> */
#include <stdlib.h>
#include <string.h>

#include "load_balancer.h"
#include "server.h"
#include "utils.h"

#define H 10000
#define MAX_SERVERS 99999

struct info {
    unsigned int hash;
    int server_id;
    int server_tag;
    server_memory *server;
};

typedef struct load_balancer load_balancer;
struct load_balancer {
    linked_list_t *list;
};

unsigned int hash_function_servers(void *a) {
    unsigned int uint_a = *((unsigned int *)a);

    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = ((uint_a >> 16u) ^ uint_a) * 0x45d9f3b;
    uint_a = (uint_a >> 16u) ^ uint_a;
    return uint_a;
}

load_balancer* init_load_balancer() {
    load_balancer *load = malloc(sizeof(load_balancer));
    DIE(load == NULL, "eroare");
    load->list = ll_create(sizeof(struct info));
    DIE(load->list == NULL, "eroare");
    return load;
}

// Stochează un produs (cheia - ID, valoarea - numele produsului)
// pe unul dintre serverele disponibile

void loader_store(load_balancer* main, char* key,
                     char* value, int* server_id) {
    ll_node_t *node = main->list->head;

    // iau primul nod de tip struct info din lista
    // parcurg lista si verific daca hash-ul cheii
    // primite ca parametru este mai mic decat
    // hash-ul nodului; daca nu este, trec mai departe;
    // cand ajung la primul hash mai mare decat
    // hash-ul cheii, stochez produsul pe server-ul
    // respectiv si ii salvez id-ul

    while (node != NULL) {
        if (hash_function_key(key) < ((struct info *)(node->data))->hash) {
            *server_id = ((struct info *)(node->data))->server_id;
            server_store(((struct info *)(node->data))->server, key, value);
            return;
        }
        node = node->next;
    }

    // daca hash-ul cheii este mai mare decat hash-ul
    // fiecarui nod din lista, atunci stochez
    // produsul pe primul nod din lista

    *server_id = ((struct info *)(main->list->head->data))->server_id;
    server_store(((struct info *)(main->list->head->data))->server, key, value);
}

char* loader_retrieve(load_balancer* main, char* key, int* server_id) {
    ll_node_t *node = main->list->head;

    // iau primul nod de tip struct info din lista
    // parcurg lista si verific daca hash-ul cheii
    // primite ca parametru este mai mic decat
    // hash-ul nodului; daca nu este, trec mai departe;
    // cand ajung la primul hash mai mare decat
    // hash-ul cheii, returnez produsul de pe server-ul
    // respectiv si ii salvez id-ul

    while (node != NULL) {
        if (hash_function_key(key) < ((struct info *)(node->data))->hash) {
            *server_id = ((struct info *)(node->data))->server_id;
            return server_retrieve(((struct info *)(node->data))->server,
                 key);
        }
        node = node->next;
    }

    // daca hash-ul cheii este mai mare decat hash-ul
    // fiecarui nod din lista, atunci stochez
    // produsul pe primul nod din lista

    *server_id = ((struct info *)(main->list->head->data))->server_id;
    return server_retrieve(((struct info *)(main->list->head->data))->server,
             key);
}

void redistribution(load_balancer *main, ll_node_t *node) {
    // daca am un singur server in lista
    // nu este nevoie sa redistribui produsele
    if (main->list->size == 1) {
        return;
    }

    // am luat 2 noduri vecine cu nodul node
    // declarat ca parametru al functiei

    ll_node_t *left = main->list->head;

    ll_node_t *right = node->next;

    // daca nodul curent este primul in lista
    // atunci vecinul din stanga va fi ultimul din lista
    // altfel il parcurg pana ajunge in spatele lui node

    if (node == main->list->head) {
        while (left->next != NULL) {
            left = left->next;
        }
    } else {
        while (left->next != NULL && left->next != node) {
            left = left->next;
        }
    }

    // daca nodul curent este ultimul in lista
    // atunci vecinul din dreapta va fi primul din lista

    if (right == NULL) {
        right = main->list->head;
    }

    if (((struct info *)(node->data))->server_id ==
                ((struct info *)(right->data))->server_id) {
        return;
    }

    // daca am 2 noduri in lista atunci
    // vecinul din stanga si din dreapta
    // al nodului curent vor fi unul si acelasi nod;
    // am comparat hash-ul cheii nodului curent cu fiecare
    // cheie din serverele vecine pentru a face redistribuirea
    // obiectelor dupa hash; acest procedeu se aplica si pe else

    if (node == main->list->head) {
        for (int i = 0; i < ((struct info *)(right->data))->server->hmax; i++) {
            ll_node_t *curr =
                ((struct info *)(right->data))->server->buckets[i]->head;
            while (curr != NULL) {
                if (hash_function_key(((struct data *)(curr->data))->key)
                        < ((struct info *)(node->data))->hash ||
                        hash_function_key(((struct data *)(curr->data))->key)
                        > ((struct info *)(left->data))->hash) {
                    server_store(((struct info *)(node->data))->server,
                            ((struct data *)(curr->data))->key,
                            ((struct data *)(curr->data))->value);
                    curr = curr->next;
                } else {
                    curr = curr->next;
                }
            }
        }
    } else {
        for (int i = 0; i < ((struct info *)(right->data))->server->hmax; i++) {
            ll_node_t *curr =
                ((struct info *)(right->data))->server->buckets[i]->head;
            while (curr != NULL) {
                if (hash_function_key(((struct data *)(curr->data))->key)
                        < ((struct info *)(node->data))->hash ||
                        hash_function_key(((struct data *)(curr->data))->key)
                        > ((struct info *)(left->data))->hash) {
                    server_store(((struct info *)(node->data))->server,
                            ((struct data *)(curr->data))->key,
                            ((struct data *)(curr->data))->value);
                    curr = curr->next;
                } else {
                    curr = curr->next;
                }
            }
        }
    }
}

void loader_add_server(load_balancer* main, int server_id) {
    // mi-am declarat nodul de tip struct info pe care vreau sa il adaug
    // si replicile lui; i-am atribuit fiecaruia server id-ul
    // eticheta, hash-ul si server-ul

    struct info node_1;
    struct info node_2;
    struct info node_3;
    node_1.server_id = server_id;
    node_2.server_id = server_id;
    node_3.server_id = server_id;
    node_1.server_tag = server_id;
    node_2.server_tag = 100000 + server_id;
    node_3.server_tag = 200000 + server_id;
    node_1.hash = hash_function_servers(&node_1.server_tag);
    node_2.hash = hash_function_servers(&node_2.server_tag);
    node_3.hash = hash_function_servers(&node_3.server_tag);
    node_1.server = init_server_memory();
    node_2.server = node_1.server;
    node_3.server = node_1.server;
    ll_node_t *node1 = main->list->head;
    int pos = 0;

    // am parcurs hashring-ul pentru a gasi pozitia
    // pe care vreau sa adaug nodul; acest procedeu
    // l-am repetat de 3 ori pentru a face acest lucru
    // si pentru replici

    while (node1 != NULL) {
        if (node_1.hash < ((struct info *)(node1->data))->hash) {
            break;
        } else {
            if (node_1.hash == ((struct info *)(node1->data))->hash) {
                while (node_1.server_id >
                        ((struct info *)(node1->data))->server_id) {
                    node1 = node1->next;
                    pos++;
                }
            }
        }
        pos++;
        node1 = node1->next;
    }

    // cand am gasit pozitia pe care vreau sa imi adaug
    // nodul, pur si simplu apelez functia de adaugare
    // si rebalansez obiectele; acest lucru este necesar
    // dupa fiecare adaugare (inclusiv a replicilor)

    ll_add_nth_node(main->list, pos, &node_1);
    redistribution(main, get_nth_node(main->list, pos));

    pos = 0;
    ll_node_t *node2 = main->list->head;
    while (node2 != NULL) {
        if (node_2.hash < ((struct info *)(node2->data))->hash) {
            break;
        } else {
            if (node_2.hash == ((struct info *)(node2->data))->hash) {
                while (node_2.server_id >
                        ((struct info *)(node2->data))->server_id) {
                    node2 = node2->next;
                    pos++;
                }
            }
        }
        node2 = node2->next;
        pos++;
    }

    ll_add_nth_node(main->list, pos, &node_2);
    redistribution(main, get_nth_node(main->list, pos));

    pos = 0;
    ll_node_t *node3 = main->list->head;
    while (node3 != NULL) {
        if (node_3.hash < ((struct info *)(node3->data))->hash) {
            break;
        } else {
            if (node_3.hash == ((struct info *)(node3->data))->hash) {
                while (node_3.server_id >
                        ((struct info *)(node3->data))->server_id) {
                    node3 = node3->next;
                    pos++;
                }
            }
        }
        node3 = node3->next;
        pos++;
    }

    ll_add_nth_node(main->list, pos, &node_3);
    redistribution(main, get_nth_node(main->list, pos));
}

void loader_remove_server(load_balancer* main, int server_id) {
    // am luat etichetele de pe hashring si
    // mi-am luat un server in care salvez
    // server-ul pe care doresc sa il elimin
    // fac acest lucru pentru a putea ulterior
    // sa eliberez memoria

    int tag_1 = server_id;
    int tag_2 = 100000 + server_id;
    int tag_3 = 200000 + server_id;
    server_memory* removed_server;

    ll_node_t *node = main->list->head;
    int pos1 = 0;
    int pos2 = 0;
    int pos3 = 0;

    node = main->list->head;

    // parcurg hashring-ul pana cand ajung la eticheta dorita
    // si ii salvez pozitia pentru a sti pe ce pozitie folosesc
    // functia de remove; acest procedeu este repetat si pentru
    // replicile server-ului

    while (node != NULL) {
        if (((struct info *)node->data)->server_tag == tag_1) {
            removed_server = ((struct info *)node->data)->server;
            break;
        }
        pos1++;
        node = node->next;
    }

    ll_node_t* removed = ll_remove_nth_node(main->list, pos1);
    free(removed->data);
    free(removed);

    node = main->list->head;
    while (node != NULL) {
        if (((struct info *)node->data)->server_tag == tag_2) {
            break;
        }
        pos2++;
        node = node->next;
    }

    removed = ll_remove_nth_node(main->list, pos2);
    free(removed->data);
    free(removed);

    node = main->list->head;
    while (node != NULL) {
        if (((struct info *)node->data)->server_tag == tag_3) {
            break;
        }
        pos3++;
        node = node->next;
    }

    removed = ll_remove_nth_node(main->list, pos3);
    free(removed->data);
    free(removed);

    // dupa ce sterg server-ul original si replicile sale
    // trebuie sa fac rebalansarea obiectelor, fapt ce duce
    // la stocarea feicarei cheii din server-ul sters
    // pe celelalte servere

    for (int i = 0; i < removed_server->hmax; i++) {
        ll_node_t *curr = removed_server->buckets[i]->head;
        while (curr != NULL) {
            int aux_server = 0;
            loader_store(main, ((struct data *)(curr->data))->key,
                        ((struct data *)(curr->data))->value, &aux_server);
            curr = curr->next;
        }
    }

    // la final, am grija sa eliberez memoria serverului sters

    free_server_memory(removed_server);
}

void free_load_balancer(load_balancer* main) {
    ll_node_t *node = main->list->head;
    // parcurg lista de servere si elibere memoria
    // fiecarui nod de tip struct info la feicare pas;
    // la final eliberez lista si load balancer-ul

    while (node != NULL) {
        if (((struct info *)node->data)->server_tag < MAX_SERVERS) {
            server_memory *remove = ((struct info *)node->data)->server;
            free_server_memory(remove);
        }
        node = node->next;
    }
    ll_free(&main->list);
    free(main);
}
