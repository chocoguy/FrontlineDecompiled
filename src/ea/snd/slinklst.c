#include "ea/snd/sndi.h"

void SNDLINKI_init(SNDLINKLIST* list) {
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
}

void SNDLINKI_push(SNDLINKLIST* list, SNDLINKNODE* node) {
    node->next = list->head;
    node->prev = NULL;
    if (list->head != NULL) {
        list->head->prev = node;
    } else {
        list->tail = node;
    }
    list->head = node;
    list->count++;
}

void SNDLINKI_pushtail(SNDLINKLIST* list, SNDLINKNODE* node) {
    node->next = NULL;
    node->prev = list->tail;
    if (list->tail != NULL) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->count++;
}

SNDLINKNODE* SNDLINKI_pop(SNDLINKLIST* list) {
    SNDLINKNODE* node = list->head;
    if (node != NULL) {
        list->head = node->next;
        if (list->head == NULL) {
            list->tail = NULL;
        } else {
            list->head->prev = NULL;
        }
        list->count--;
    }
    return node;
}

// The node's own links are left as they were.
void SNDLINKI_remove(SNDLINKLIST* list, SNDLINKNODE* node) {
    if (node == list->head) {
        list->head = list->head->next;
    }
    if (node == list->tail) {
        list->tail = list->tail->prev;
    }
    if (node->prev != NULL) {
        node->prev->next = node->next;
    }
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }
    list->count--;
}
